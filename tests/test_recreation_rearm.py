import copy
import hashlib
import json
import time
import unittest
from unittest.mock import patch

from ffr_helper.recreation_rearm import RecreationRearmBridge,validate_rearm
import test_recreation_trial as fixtures


def idle_rearm():
    return dict(operation='verified-child-rearm',restorable=False,supported=True,phase='idle',nonce='',session='',
        sequence=0,openingHash='',deadline=0,generation=1,completedGeneration=0,archiveGeneration=0,
        archiveSha256='',archiveError='',message='Fixture')


class RearmTests(unittest.TestCase):
    def setUp(self):
        self.helper=fixtures.RecreationTrialTests();self.helper.setUp();self.helper.verified()
        self.f=self.helper.fixture;self.child=self.helper.child();f=self.f
        f.frame['guard'].update(battleActive=True,commandMenu=True,fieldReady=False,managersReleased=False,units=4,sequence=2)
        f.frame['rearmTrial']=idle_rearm()
        manifest=dict(format='ffr-helper-verified-opening-cycle',protocol=1,restorable=False,generation=1,
            session=f.recorder.session,sourceSequence=1,childSequence=2,openingHash=f.capture.metadata['sha256'],
            recreationNonce=self.helper.bridge.request['nonce'],abortNonce=f.bridge.request['nonce'],
            byteLength=len(f.capture.progression),entry=copy.deepcopy(f.capture.metadata['entry']),
            sourceFieldAnchor=copy.deepcopy(f.capture.metadata['fieldAnchor']),
            childFieldAnchor=copy.deepcopy(self.child.metadata['fieldAnchor']),progressionUnchanged=True,
            childMenuVerified=True,fullRestorationVerified=False)
        self.directory=f.recorder.connection/'cycle-history'/f.recorder.session/'generation-1';self.directory.mkdir(parents=True)
        (self.directory/'progression.gvas').write_bytes(f.capture.progression)
        self.manifest=manifest;self.write_manifest()
        self.bridge=RecreationRearmBridge(self.helper.bridge)

    def tearDown(self):
        self.helper.tearDown()

    def write_manifest(self):
        raw=json.dumps(self.manifest).encode('utf8');(self.directory/'manifest.json').write_bytes(raw)
        self.f.frame['rearmTrial'].update(archiveGeneration=1,archiveSha256=hashlib.sha256(raw).hexdigest())
        self.f.write_frame()

    def prepare(self):
        self.bridge.prepare(self.f.bridge.status(),self.child)

    def acknowledge(self):
        r=self.bridge.request;f=self.f
        f.frame.update(phase='idle',nonce='',trialSession='',sequence=0,recreationTrial=fixtures.recreation_status())
        f.frame['rearmTrial'].update(phase='accepted',nonce=r['nonce'],session=f.recorder.session,
            sequence=2,openingHash=r['sha256'],deadline=r['deadline'],generation=2,completedGeneration=1)
        f.write_frame()

    def test_independent_child_archive_and_fresh_request_precede_idle_acknowledgment(self):
        self.prepare();f=self.f;r=self.bridge.request
        lines=(f.recorder.connection/'abort-request.txt').read_text().splitlines()
        self.assertEqual(lines[-1],'rearm');self.assertEqual(lines[4],'2')
        self.assertNotIn(r['nonce'],[f.bridge.request['nonce'],self.helper.bridge.request['nonce']])
        self.assertFalse(self.bridge.accepted(f.bridge.status()))
        self.acknowledge();self.assertTrue(self.bridge.accepted(f.bridge.status()))
        self.assertEqual(self.bridge.archive_progression,self.child.progression)
        self.assertEqual(json.loads(self.bridge.archive)['generation'],1)
        with self.assertRaisesRegex(ValueError,'already attempted'):self.prepare()

    def test_legacy_or_invalid_status_cannot_request_rearm(self):
        good=idle_rearm();validate_rearm(good)
        for key,value in [('supported',1),('restorable',True),('generation',True),('generation',0),
                          ('archiveGeneration',1),('phase','running'),('unknown',0),('completedGeneration',1)]:
            with self.subTest(key=key),self.assertRaises(ValueError):validate_rearm(dict(good,**{key:value}))
        self.f.frame.pop('rearmTrial');self.f.write_frame()
        with self.assertRaises(ValueError):self.prepare()
        self.assertIsNone(self.bridge.request)

    def test_missing_or_changed_immutable_bytes_never_rearms(self):
        before=(self.f.recorder.connection/'abort-request.txt').read_bytes()
        (self.directory/'progression.gvas').write_bytes(b'changed')
        with self.assertRaisesRegex(ValueError,'progression differs'):self.prepare()
        self.assertIsNone(self.bridge.request)
        self.assertEqual((self.f.recorder.connection/'abort-request.txt').read_bytes(),before)
        (self.directory/'progression.gvas').write_bytes(self.child.progression)
        (self.directory/'manifest.json').write_bytes(b'changed')
        with self.assertRaisesRegex(ValueError,'manifest changed'):self.prepare()

    def test_manifest_claims_nonces_sequence_and_nested_types_are_checked(self):
        original=copy.deepcopy(self.manifest)
        for key,value in [('protocol',True),('fullRestorationVerified',True),('sourceSequence',2),
                          ('childSequence',3),('recreationNonce','f'*32),('byteLength',True),('unknown',0)]:
            self.manifest=dict(original,**{key:value});self.write_manifest()
            with self.subTest(key=key),self.assertRaises(ValueError):self.prepare()
            self.assertIsNone(self.bridge.request)
        self.manifest=copy.deepcopy(original);self.manifest['entry']['skipIntro']=0;self.write_manifest()
        with self.assertRaises(ValueError):self.prepare()

    def test_changed_history_sequence_pending_jobs_and_stale_frames_discard_intent(self):
        f=self.f;original=copy.deepcopy(f.frame)
        for group,key,value in [('guard','openingHistoryUntouched',False),('guard','workingJobs',1),
                                ('guard','sequence',3),('guard','commandAbortReady',False),
                                (None,'probeTime',int(time.time())-5),('rearmTrial','archiveError','write failed')]:
            f.frame=copy.deepcopy(original);(f.frame[group] if group else f.frame)[key]=value;f.write_frame()
            with self.subTest(key=key),self.assertRaises(ValueError):self.prepare()
            self.assertIsNone(self.bridge.request)

    def test_changed_or_refused_acknowledgment_never_enables_new_abort(self):
        self.prepare();self.acknowledge();f=self.f;original=copy.deepcopy(f.frame)
        for group,key,value in [('rearmTrial','generation',3),('rearmTrial','sequence',3),
                                ('guard','openingHistoryUntouched',False),('guard','workingJobs',1),
                                (None,'phase','manager-cleared')]:
            f.frame=copy.deepcopy(original)
            (f.frame[group] if group else f.frame)[key]=value
            f.write_frame()
            with self.subTest(key=key),self.assertRaises(ValueError):self.bridge.accepted(f.bridge.status())
        f.frame=original;f.frame['rearmTrial'].update(phase='refused',message='Fixture refusal');f.write_frame()
        with self.assertRaisesRegex(ValueError,'Fixture refusal'):self.bridge.accepted(f.bridge.status())

    def test_uncertain_request_write_is_never_resent(self):
        with patch('ffr_helper.recreation_rearm.atomic_write',side_effect=OSError('write failed')):
            with self.assertRaises(OSError):self.prepare()
        self.assertIsNotNone(self.bridge.request)
        with self.assertRaisesRegex(ValueError,'already attempted'):self.prepare()

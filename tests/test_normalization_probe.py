import copy
import hashlib
import json
import time
import unittest
from unittest.mock import patch

from ffr_helper.normalization_probe import BOOLEANS, NormalizationProbeBridge, validate_normalization_probe
from ffr_helper.import_probe import ImportProbeBridge
from ffr_helper.playtime_plan import plan_opening_playtime
from ffr_helper.abort_trial import AbortTrialBridge
import test_abort_trial
from test_playtime_plan import capture_with_clock


class NormalizationProbeTests(unittest.TestCase):
    def setUp(self):
        test_abort_trial.AbortClientTests.setUp(self)
        self.capture=capture_with_clock()
        self.report['objects']['nativeEntry']['opening']=self.capture.metadata
        (self.root/'connection/opening-progression.gvas').write_bytes(self.capture.progression)
        self.frame['guard'].update(battleActive=False,commandMenu=False,units=0,fieldReady=True,openingHash=self.capture.metadata['sha256'])
        self.frame['normalizationProbe']=dict({k:False for k in BOOLEANS},operation='detached-playtime-normalization',restorable=False,
            phase='idle',nonce='',session='',openingHash='',sequence=0,captureTime=0,retainedOwnedPins=0,
            normalizedSeconds=0,expected=None,clone=None,message='Fixture')
        self.frame['normalizationProbe'].update(available=True,loaderReady=True,fieldReady=True,pinsReleased=True)
        self.bridge=NormalizationProbeBridge(self.recorder)
        self.write_frame()

    def tearDown(self):self.temp.cleanup()

    def write_frame(self):
        (self.root/'connection/abort-status.json').write_text(json.dumps(self.frame),encoding='utf8')

    def passed_receipt(self):
        source=self.bridge.send(self.report);plan=plan_opening_playtime(source)
        files={}
        for key,name in [('expected','normalization-expected.gvas'),('clone','normalization-clone.gvas')]:
            (self.root/'connection'/name).write_bytes(plan.normalized_progression)
            files[key]=dict(file=name,sha256=hashlib.sha256(plan.normalized_progression).hexdigest(),byteLength=len(plan.normalized_progression))
        self.frame['normalizationProbe'].update({k:True for k in ['attempted','passed','enginePassed','sourceValid','inputUnchanged',
            'cloneBeforeMatches','writeAttempted','writeReturned','cloneAfterMatches','liveUnchanged','pinsReleased','filesCommitted']})
        self.frame['normalizationProbe'].update(phase='passed',nonce=self.bridge.request['nonce'],session=self.recorder.session,
            openingHash=self.capture.metadata['sha256'],sequence=1,captureTime=int(time.time()),normalizedSeconds=1970.25,**files)
        self.write_frame()
        return self.bridge.receipt()

    def test_explicit_distinct_one_shot_command_and_independent_evidence(self):
        self.assertIsNone(self.bridge.receipt())
        receipt=self.passed_receipt()
        self.assertEqual((self.root/'connection/abort-request.txt').read_text().splitlines()[-1],'probe-normalize')
        evidence=self.bridge.evidence(receipt)
        self.assertEqual(evidence['expected'],self.bridge.plan.normalized_progression)
        self.assertEqual(evidence['clone'],evidence['expected'])
        self.assertNotEqual(evidence['clone'],self.capture.progression)
        with self.assertRaisesRegex(ValueError,'already requested'):self.bridge.send(self.report)

    def test_refused_boundaries_never_write_a_command(self):
        before=copy.deepcopy(self.frame)
        cases=[('normalizationProbe','available',False),('normalizationProbe','loaderReady',False),
               ('normalizationProbe','fieldReady',False),('normalizationProbe','attempted',True),
               ('guard','battleActive',True),('guard','units',1),('guard','waitingJobs',1),
               ('guard','fieldReady',False),('guard','sequence',2),('guard','openingHash','e'*64)]
        for parent,key,value in cases:
            self.frame=copy.deepcopy(before);self.frame[parent][key]=value;self.write_frame()
            with self.subTest(key=key),self.assertRaises(ValueError):self.bridge.send(self.report)
            self.assertIsNone(self.bridge.request)
        self.assertFalse((self.root/'connection/abort-request.txt').exists())

    def test_unsupported_clock_is_diagnostic_only_and_sends_nothing(self):
        self.capture.metadata['playtimeBasis'].pop('roundingMode')
        with self.assertRaisesRegex(ValueError,'nearest-even'):self.bridge.send(self.report)
        self.assertIsNone(self.bridge.request)
        self.assertFalse((self.root/'connection/abort-request.txt').exists())

    def test_uncertain_command_write_cannot_be_retried(self):
        with patch('ffr_helper.abort_trial.atomic_write',side_effect=OSError('uncertain')):
            with self.assertRaises(OSError):self.bridge.send(self.report)
        self.assertIsNotNone(self.bridge.request)
        with self.assertRaisesRegex(ValueError,'already requested'):self.bridge.send(self.report)

    def test_success_requires_every_comparison_cleanup_and_committed_pair(self):
        self.passed_receipt();before=copy.deepcopy(self.frame)
        changes=[(key,False) for key in ['enginePassed','sourceValid','inputUnchanged','cloneBeforeMatches','writeReturned',
                                        'cloneAfterMatches','liveUnchanged','pinsReleased','filesCommitted']]
        changes += [('rootChanged',True),('identityUncertain',True),('retainedOwnedPins',1),('restorable',True),
                    ('normalizedSeconds',float('nan')),('normalizedSeconds',True),('phase','restored'),('sequence',True)]
        for key,value in changes:
            self.frame=copy.deepcopy(before);self.frame['normalizationProbe'][key]=value;self.write_frame()
            with self.subTest(key=key):self.assertIsNone(self.bridge.status())
        self.frame=copy.deepcopy(before);self.frame['normalizationProbe']['clone']['file']='../save.sav';self.write_frame()
        self.assertIsNone(self.bridge.status())

    def test_changed_files_or_receipt_are_refused(self):
        receipt=self.passed_receipt();path=self.root/'connection/normalization-clone.gvas'
        original=path.read_bytes();path.write_bytes(b'broken')
        with self.assertRaisesRegex(ValueError,'verification'):self.bridge.evidence(receipt)
        path.write_bytes(original)
        calls=0;get=self.bridge.receipt
        def changed_receipt():
            nonlocal calls
            calls+=1;value=get()
            if calls==2:value['captureTime']+=1
            return value
        with patch.object(self.bridge,'receipt',side_effect=changed_receipt):
            with self.assertRaisesRegex(ValueError,'changed while'):self.bridge.evidence(receipt)

    def test_native_claim_cannot_replace_independent_source_plan(self):
        receipt=self.passed_receipt()
        self.bridge.plan=plan_opening_playtime(capture_with_clock(elapsed=1))
        with self.assertRaisesRegex(ValueError,'independent'):self.bridge.evidence(receipt)
        self.frame['normalizationProbe']['nonce']='a'*32;self.write_frame()
        self.assertIsNone(self.bridge.receipt())

    def test_idle_claim_cannot_conceal_engine_actions_and_other_probes_are_blocked(self):
        original=copy.deepcopy(self.frame['normalizationProbe'])
        self.frame['normalizationProbe']['writeAttempted']=True;self.write_frame()
        self.assertIsNone(self.bridge.status())
        self.frame['normalizationProbe']=original
        self.frame['normalizationProbe']['attempted']=True;self.write_frame()
        with self.assertRaises(ValueError):AbortTrialBridge(self.recorder).prepare(self.report)
        # Native marks import unavailable after consuming normalization.
        self.frame['importProbe']=dict(available=False,loaderReady=True,phase='idle',nonce='',session='',sequence=0,openingHash='',
            passed=False,cloneMatches=False,liveUnchanged=False,inputUnchanged=False,pinsReleased=True,byteLength=0,message='Fixture',restorable=False)
        self.write_frame()
        with self.assertRaises(ValueError):ImportProbeBridge(self.recorder).send(self.report)

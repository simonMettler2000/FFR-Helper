import copy
import json
import time
import unittest
from unittest.mock import patch

from ffr_helper.opening import OpeningCapture
from ffr_helper.recreation_trial import RecreationTrialBridge, validate_recreation, FLAGS
import test_abort_trial
from test_opening_comparison import full_capture
from test_map_cache import sample_cache


def recreation_status():
    return dict({k:False for k in FLAGS},available=True,cleanupVerified=True,
                operation='same-setup-current-field',phase='idle',nonce='',session='',sourceHash='',
                sourceSequence=0,childSequence=0,deadline=0,entryFieldAnchor=None,message='Fixture')


class RecreationTrialTests(unittest.TestCase):
    def setUp(self):
        self.fixture=test_abort_trial.AbortClientTests();self.fixture.setUp();f=self.fixture
        f.capture.metadata['entry']=full_capture().metadata['entry']
        f.capture.metadata.update(fieldAnchor=dict(kind='partial-field-anchor',restorable=False,
            source='live-player-root-transform',coverage='position-and-direction-only',sameWorldChecked=True,
            mapId=1000,position=[1.0,2.0,3.0],direction=2),mapUnitCache=sample_cache())
        f.arm();f.bridge.commit(f.bridge.status())
        f.frame.update(phase='manager-cleared',recreationTrial=recreation_status(),
            nativeTaskExit=dict({k:True for k in ['endIssued','forceEndFlagObserved','uiEndIssued','uiClosedObserved',
                'resultTaskObserved','forceEndResultObserved']},uncertain=False),
            after=dict(f.frame['recovery'],file='abort-after.gvas'))
        f.frame['guard'].update(fieldReady=True,managersReleased=True,battleActive=False,commandMenu=False,units=0)
        (f.root/'connection/abort-after.gvas').write_bytes(f.capture.progression);f.write_frame()
        self.bridge=RecreationTrialBridge(f.bridge)

    def tearDown(self):self.fixture.tearDown()

    def arm(self):
        f=self.fixture;self.bridge.prepare(f.bridge.status(),f.capture);r=self.bridge.request
        f.frame['recreationTrial'].update(phase='armed',nonce=r['nonce'],session=f.recorder.session,
            sourceSequence=r['sequence'],sourceHash=r['sha256'],deadline=r['deadline'],entryFieldAnchor=copy.deepcopy(f.capture.metadata['fieldAnchor']))
        f.write_frame();return f.bridge.status()

    def child(self):
        f=self.fixture;metadata=copy.deepcopy(f.capture.metadata);metadata['sequence']+=1
        return OpeningCapture(metadata,f.capture.progression)

    def verified(self):
        f=self.fixture;self.bridge.commit(self.arm())
        f.frame['recreationTrial'].update(phase='verified',entryAttempted=True,inputTransferred=True,
            nativeReturned=True,newEncounterVerified=True,firstMenuProgressionUnchanged=True,childSequence=2)
        f.write_frame();return f.bridge.status()

    def test_prepared_source_is_owned_and_exact_child_never_claims_rollback(self):
        state=self.verified();f=self.fixture;original=copy.deepcopy(f.capture.metadata)
        f.capture.metadata['entry']['friendliesUnitIdList'].append(999)
        self.assertEqual(self.bridge.source.metadata,original)
        child=OpeningCapture(dict(original,sequence=2),f.capture.progression)
        result=self.bridge.compare_child(state,child)
        self.assertTrue(result['currentFieldRecreationVerified'])
        for key in ['battleRestartImplemented','progressionRestored','rngRestored','effectGraphsVerified','fullCacheCoherenceVerified']:
            self.assertIs(result[key],False)
        self.assertEqual(self.bridge.source.metadata['mapUnitCache']['units'][0]['continuousEffectCount'],2)

    def test_strict_receipt_types_outcomes_and_restoration_claims(self):
        good=self.verified()['recreationTrial'];validate_recreation(good)
        for key,value in [('restorable',True),('rngRestored',True),('newEncounterVerified',1),
                          ('childSequence',3),('inputTransferred',False),('nativeReturned',False),
                          ('firstMenuProgressionUnchanged',False),('sourceHash','x'*64),('deadline',True),('unknown',0)]:
            with self.subTest(key=key),self.assertRaises(ValueError):validate_recreation(dict(good,**{key:value}))
        with self.assertRaises(ValueError):validate_recreation(dict(recreation_status(),childSequence=1))
        with self.assertRaises(ValueError):validate_recreation(dict(good,phase='armed'))

    def test_missing_field_or_cache_never_prepares(self):
        for key in ['fieldAnchor','mapUnitCache']:
            source=copy.deepcopy(self.fixture.capture);source.metadata.pop(key)
            with self.subTest(key=key),self.assertRaises(ValueError):self.bridge.prepare(self.fixture.bridge.status(),source)
            self.assertIsNone(self.bridge.request)

    def test_exit_guard_history_rewards_or_stale_game_frame_never_prepares(self):
        f=self.fixture;original=copy.deepcopy(f.frame)
        changes=[('guard','managersReleased',False),('guard','openingHistoryUntouched',False),
            ('guard','battleActive',True),('guard','waitingJobs',1),('guard','sequence',True),('nativeTaskExit','uncertain',True),
            ('nativeTaskExit','resultTaskObserved',False),(None,'probeTime',int(time.time())-5)]
        for group,key,value in changes:
            f.frame=copy.deepcopy(original)
            (f.frame[group] if group else f.frame)[key]=value;f.write_frame()
            with self.subTest(key=key),self.assertRaises(ValueError):self.bridge.prepare(f.bridge.status(),f.capture)
            self.assertIsNone(self.bridge.request)

    def test_changed_durable_after_bytes_never_prepares(self):
        f=self.fixture;(f.root/'connection/abort-after.gvas').write_bytes(b'damaged')
        with self.assertRaises(ValueError):self.bridge.prepare(f.bridge.status(),f.capture)
        self.assertIsNone(self.bridge.request)

    def test_expiry_foreign_nonce_or_fresh_field_change_prevents_commit(self):
        state=self.arm();f=self.fixture;original=copy.deepcopy(f.frame)
        for group,key,value in [('recreationTrial','nonce','f'*32),('guard','units',1),
                                ('recreationTrial','phase','returned')]:
            f.frame=copy.deepcopy(original);f.frame[group][key]=value;f.write_frame()
            with self.subTest(key=key),self.assertRaises(ValueError):self.bridge.commit(state)
            self.assertFalse(self.bridge.commit_attempted)
        f.frame=original;f.write_frame()
        with patch('ffr_helper.recreation_trial.time.time',return_value=self.bridge.request['deadline']+1):
            with self.assertRaises(ValueError):self.bridge.commit(state)

    def test_uncertain_prepare_and_commit_io_cannot_be_retried(self):
        with patch('ffr_helper.recreation_trial.atomic_write',side_effect=OSError('uncertain')):
            with self.assertRaises(OSError):self.bridge.prepare(self.fixture.bridge.status(),self.fixture.capture)
        with self.assertRaisesRegex(ValueError,'already requested'):self.bridge.prepare(self.fixture.bridge.status(),self.fixture.capture)
        self.bridge=RecreationTrialBridge(self.fixture.bridge);state=self.arm()
        with patch('ffr_helper.recreation_trial.atomic_write',side_effect=OSError('uncertain')):
            with self.assertRaises(OSError):self.bridge.commit(state)
        self.assertTrue(self.bridge.commit_attempted)
        with self.assertRaisesRegex(ValueError,'already attempted'):self.bridge.commit(state)

    def test_child_entry_cache_anchor_sequence_and_identity_changes_are_refused(self):
        state=self.verified();good=self.child()
        for key in ['entry','fieldAnchor','mapUnitCache','sequence','gameSha256']:
            child=copy.deepcopy(good)
            if key=='entry':child.metadata[key]['enemiesUnitIdList'].append(42)
            elif key=='fieldAnchor':child.metadata[key]['position'][0]+=1
            elif key=='mapUnitCache':child.metadata[key]['units'][0]['raw']['hpBase']+=1
            elif key=='sequence':child.metadata[key]+=1
            else:child.metadata[key]='0'*64
            with self.subTest(key=key),self.assertRaises(ValueError):self.bridge.compare_child(state,child)
        changed=copy.deepcopy(state);changed['recreationTrial']['nonce']='b'*32
        with self.assertRaises(ValueError):self.bridge.compare_child(changed,good)

    def test_stable_post_exit_position_is_bound_separately_without_rewriting_source(self):
        state=self.arm();f=self.fixture;original=copy.deepcopy(f.capture.metadata['fieldAnchor'])
        f.frame['recreationTrial']['entryFieldAnchor']['position'][0]+=47
        f.frame['recreationTrial']['entryFieldAnchor']['direction']=3
        f.write_frame();self.bridge.commit(f.bridge.status())
        f.frame['recreationTrial'].update(phase='verified',entryAttempted=True,inputTransferred=True,nativeReturned=True,
            newEncounterVerified=True,firstMenuProgressionUnchanged=True,childSequence=2);f.write_frame()
        child=self.child();child.metadata['fieldAnchor']=copy.deepcopy(self.bridge.entry_field)
        result=self.bridge.compare_child(f.bridge.status(),child)
        self.assertTrue(result['childMatchesPreparedExitField'])
        self.assertFalse(result['originalFieldAnchorUnchanged']);self.assertFalse(result['worldPositionRestored'])
        self.assertEqual(f.capture.metadata['fieldAnchor'],original)
        f.frame['recreationTrial']['entryFieldAnchor']['position'][0]+=1;f.write_frame()
        with self.assertRaises(ValueError):self.bridge.compare_child(f.bridge.status(),child)

    def test_changed_prepared_anchor_cannot_commit(self):
        state=self.arm();f=self.fixture
        f.frame['recreationTrial']['entryFieldAnchor']['position'][0]+=1;f.write_frame()
        with self.assertRaisesRegex(ValueError,'field/map'):self.bridge.commit(state)
        self.assertFalse(self.bridge.commit_attempted)

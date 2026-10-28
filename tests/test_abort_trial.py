import copy
import hashlib
import json
from pathlib import Path
import tempfile
import time
import unittest
from unittest.mock import patch

from ffr_helper.abort_trial import AbortTrialBridge, compare_progression
from ffr_helper.editor import SaveEditor
from ffr_helper.runtime import NativeEntryRecorder
from test_opening import sample_opening
from test_runtime import sample_report


class AbortClientTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.root=Path(self.temp.name)
        (self.root/'connection').mkdir();self.recorder=NativeEntryRecorder(self.root);self.recorder.capture()
        self.bridge=AbortTrialBridge(self.recorder);self.capture=sample_opening()
        self.report=sample_report(self.recorder);self.report['adapter']=self.recorder.adapter_name
        self.report['objects']={'nativeEntry':{'opening':self.capture.metadata}}
        (self.root/'connection/opening-progression.gvas').write_bytes(self.capture.progression)
        self.frame=dict(protocol=1,adapter='ffr-helper-opening-abort-trial',format='ffr-helper-abort-trial-status',
                        runtimeMode='experimental-abort-test',readOnly=False,restorable=False,
                        session=self.recorder.session,time=int(time.time()),probeTime=int(time.time()),
                        available=True,phase='idle',nonce='',trialSession='',sequence=0,message='Fixture',
                        guard=dict(gameThread=True,matchingWorld=True,ordinaryEncounter=True,battleActive=True,
                                   commandMenu=True,units=4,waitingJobs=0,workingJobs=0,friendlyActions=0,
                                   sequence=1,openingHash=self.capture.metadata['sha256']))
        self.write_frame()

    def tearDown(self):
        self.temp.cleanup()

    def write_frame(self):
        (self.root/'connection/abort-status.json').write_text(json.dumps(self.frame),encoding='utf8')

    def arm(self):
        self.bridge.prepare(self.report);request=self.bridge.request
        self.frame.update(phase='armed',nonce=request['nonce'],trialSession=self.recorder.session,sequence=1)
        raw=self.capture.progression;(self.root/'connection/abort-recovery.gvas').write_bytes(raw)
        self.frame['recovery']=dict(kind='progression-only',restorable=False,file='abort-recovery.gvas',
                                    byteLength=len(raw),sha256=hashlib.sha256(raw).hexdigest())
        self.write_frame()
        return self.bridge.status()

    def test_explicit_prepare_and_verified_recovery_precede_single_commit(self):
        frame=self.arm()
        self.assertEqual((self.root/'connection/abort-request.txt').read_text().splitlines()[-1],'prepare')
        self.assertEqual(self.bridge.commit(frame),self.capture.progression)
        self.assertEqual((self.root/'connection/abort-request.txt').read_text().splitlines()[-1],'commit')
        with self.assertRaisesRegex(ValueError,'already attempted'):self.bridge.commit(frame)

    def test_read_only_other_stale_or_mismatched_status_is_not_actionable(self):
        original=copy.deepcopy(self.frame)
        for key,value in [('readOnly',True),('restorable',True),('session','f'*32),
                          ('time',int(time.time())-30),('adapter','ffr-helper-native-entry-observer')]:
            self.frame=copy.deepcopy(original);self.frame[key]=value;self.write_frame()
            self.assertIsNone(self.bridge.status())
        self.assertFalse((self.root/'connection/abort-request.txt').exists())

    def test_pending_actions_or_an_old_opening_prevent_preparation(self):
        for key,value in [('workingJobs',1),('waitingJobs',1),('friendlyActions',1),
                          ('sequence',2),('openingHash','0'*64),('units',False)]:
            old=self.frame['guard'][key];self.frame['guard'][key]=value;self.write_frame()
            with self.assertRaises(ValueError):self.bridge.prepare(self.report)
            self.assertIsNone(self.bridge.request)
            self.frame['guard'][key]=old

    def test_changed_receipt_payload_or_progression_class_prevents_commit(self):
        frame=self.arm()
        (self.root/'connection/abort-recovery.gvas').write_bytes(b'damaged')
        with self.assertRaisesRegex(ValueError,'verification'):self.bridge.commit(frame)
        self.assertFalse(self.bridge.commit_attempted)
        raw=self.capture.progression.replace(b'CPP_MuchaGameSaveData',b'CPP_InvalidClassName')
        (self.root/'connection/abort-recovery.gvas').write_bytes(raw)
        self.frame['recovery'].update(byteLength=len(raw),sha256=hashlib.sha256(raw).hexdigest());self.write_frame()
        with self.assertRaises(ValueError):self.bridge.commit(self.bridge.status())
        self.assertFalse(self.bridge.commit_attempted)

    def test_expiry_changed_nonce_or_changed_battle_prevents_commit(self):
        frame=self.arm()
        self.frame['nonce']='d'*32;self.write_frame()
        with self.assertRaises(ValueError):self.bridge.commit(frame)
        self.frame['nonce']=self.bridge.request['nonce'];self.frame['guard']['friendlyActions']=1;self.write_frame()
        with self.assertRaises(ValueError):self.bridge.commit(self.bridge.status())
        self.frame['guard']['friendlyActions']=0;self.write_frame()
        self.bridge.request['deadline']=int(time.time())-1
        with self.assertRaisesRegex(ValueError,'expired'):self.bridge.commit(self.bridge.status())
        self.assertFalse(self.bridge.commit_attempted)

    def test_uncertain_commit_io_is_never_retried(self):
        frame=self.arm()
        with patch('ffr_helper.abort_trial.atomic_write',side_effect=OSError('fixture write failure')):
            with self.assertRaises(OSError):self.bridge.commit(frame)
        self.assertTrue(self.bridge.commit_attempted)
        with self.assertRaisesRegex(ValueError,'already attempted'):self.bridge.commit(frame)

    def test_progression_comparison_reports_rewards_without_claiming_restoration(self):
        raw=self.capture.progression
        result=compare_progression(raw,raw)
        self.assertTrue(all(value for key,value in result.items() if key!='restoresInvoked'))
        changed=SaveEditor.open(raw);changed.set_experience(1001,125);changed.set_mrp(13017,1001,43)
        result=compare_progression(raw,changed.export())
        self.assertFalse(result['expUnchanged']);self.assertFalse(result['mrpUnchanged'])
        self.assertTrue(result['inventoryUnchanged']);self.assertFalse(result['restoresInvoked'])

"""Opening-abort CLI lifecycle: skip excluded frames and commit only once."""
import copy
from contextlib import redirect_stdout
import io
import json
from pathlib import Path
import tempfile
import time
import unittest
from unittest.mock import patch

from ffr_helper.abort_trial import AbortTrialBridge
from test_opening import sample_opening
from tools import run_abort_trial


class AbortControllerTests(unittest.TestCase):
    def test_preemptive_and_setup_frames_are_skipped_before_durable_single_commit(self):
        opening=sample_opening();opening.metadata['sequence']=2
        now=int(time.time());sent=[];closed=[]
        ordinary=dict(gameThread=True,matchingWorld=True,ordinaryEncounter=True,battleActive=True,
            commandMenu=True,units=4,waitingJobs=0,workingJobs=0,friendlyActions=1,
            openingHistoryUntouched=True,sequence=2,openingHash=opening.metadata['sha256'])
        guards=[dict(ordinary,ordinaryEncounter=False,sequence=1),dict(ordinary,commandMenu=False),ordinary]
        class Recorder:
            def __init__(self,directory):self.request='0'*32
            def heartbeat(self):pass
            def report(self):return {'sequence':2}
            def disconnect(self):closed.append(True)
        class Bridge:
            safe_guard=staticmethod(AbortTrialBridge.safe_guard)
            def __init__(self,recorder):self.phase='idle';self.index=0
            def status(self):
                guard=copy.deepcopy(guards[min(self.index,2)]);self.index+=1
                return dict(available=True,phase=self.phase,guard=guard,message='Fixture',probeTime=now,
                    openingBoundaryPolicy='ordinary-first-menu-event-history',after={'sha256':'f'*64})
            def prepare(self,report):sent.append(('prepare',report));self.phase='armed';return opening
            def progression(self,status,after=False):return opening.progression
            def commit(self,status):sent.append(('commit',status['phase']));self.phase='manager-cleared'
        with tempfile.TemporaryDirectory() as folder:
            out=Path(folder)/'out'
            argv=['run_abort_trial.py',str(Path(folder)/'FFRHelperBattlePractice'),'--output',str(out),'--abort-at-opening']
            with patch.object(run_abort_trial,'NativeEntryRecorder',Recorder),patch.object(run_abort_trial,'AbortTrialBridge',Bridge),patch.object(run_abort_trial.sys,'argv',argv),patch.object(run_abort_trial.time,'sleep'),redirect_stdout(io.StringIO()):
                run_abort_trial.main()
            self.assertEqual(sent,[('prepare',{'sequence':2}),('commit','armed')])
            self.assertEqual(closed,[True])
            result=json.loads((out/'comparison.json').read_text())
            self.assertFalse(result['restoresInvoked'])
            self.assertTrue(result['expUnchanged'] and result['mrpUnchanged'])

    def test_unsupported_policy_cannot_prepare(self):
        sent=[];closed=[]
        class Recorder:
            def __init__(self,directory):pass
            def heartbeat(self):pass
            def disconnect(self):closed.append(True)
        class Bridge:
            def __init__(self,recorder):pass
            def status(self):return dict(available=True,phase='idle',message='Fixture',openingBoundaryPolicy='legacy',
                guard=dict(ordinaryEncounter=True,battleActive=True,commandMenu=True,friendlyActions=1,waitingJobs=0,workingJobs=0))
            def prepare(self,report):sent.append(report)
        with tempfile.TemporaryDirectory() as folder:
            argv=['run_abort_trial.py',str(Path(folder)/'FFRHelperBattlePractice'),'--output',str(Path(folder)/'out'),'--abort-at-opening']
            with patch.object(run_abort_trial,'NativeEntryRecorder',Recorder),patch.object(run_abort_trial,'AbortTrialBridge',Bridge),patch.object(run_abort_trial.sys,'argv',argv),redirect_stdout(io.StringIO()),self.assertRaisesRegex(RuntimeError,'no abort request was sent'):
                run_abort_trial.main()
        self.assertEqual(sent,[]);self.assertEqual(closed,[True])

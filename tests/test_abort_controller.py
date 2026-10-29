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
            openingHistoryUntouched=True,commandAbortReady=True,sequence=2,openingHash=opening.metadata['sha256'])
        guards=[dict(ordinary,ordinaryEncounter=False,sequence=1),dict(ordinary,commandMenu=False),
            dict(ordinary,openingHistoryUntouched=False,sequence=99),ordinary]
        class Recorder:
            def __init__(self,directory):self.request='0'*32
            def heartbeat(self):pass
            def report(self):return {'sequence':2}
            def disconnect(self):closed.append(True)
        class Bridge:
            safe_guard=staticmethod(AbortTrialBridge.safe_guard)
            def __init__(self,recorder):self.phase='idle';self.index=0
            def status(self):
                guard=copy.deepcopy(guards[min(self.index,len(guards)-1)]);self.index+=1
                return dict(available=True,phase=self.phase,guard=guard,message='Fixture',probeTime=now,
                    nativeTaskExit=dict({k:self.phase=='manager-cleared' for k in ['endIssued','forceEndFlagObserved','uiEndIssued','uiClosedObserved','forceEndResultObserved']},uncertain=False),
                    openingBoundaryPolicy='ordinary-first-menu-force-end-task',after={'sha256':'f'*64})
            def prepare(self,report):sent.append(('prepare',report));self.phase='armed';return opening
            def progression(self,status,after=False):return opening.progression
            def commit(self,status):sent.append(('commit',status['phase']));self.phase='manager-cleared'
        with tempfile.TemporaryDirectory() as folder:
            out=Path(folder)/'out'
            argv=['run_abort_trial.py',str(Path(folder)/'FFRHelperBattlePractice'),'--output',str(out),'--abort-at-opening','--trigger','automatic']
            with patch.object(run_abort_trial,'NativeEntryRecorder',Recorder),patch.object(run_abort_trial,'AbortTrialBridge',Bridge),patch.object(run_abort_trial.sys,'argv',argv),patch.object(run_abort_trial.time,'sleep'),redirect_stdout(io.StringIO()):
                run_abort_trial.main()
            self.assertEqual(sent,[('prepare',{'sequence':2}),('commit','armed')])
            self.assertEqual(closed,[True])
            result=json.loads((out/'comparison.json').read_text())
            self.assertFalse(result['restoresInvoked'])
            self.assertTrue(result['expUnchanged'] and result['mrpUnchanged'])
            self.assertTrue(result['nativeTaskExitUsed'] and result['openingHistoryUntouchedAfterCleanup'])

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
            argv=['run_abort_trial.py',str(Path(folder)/'FFRHelperBattlePractice'),'--output',str(Path(folder)/'out'),'--abort-at-opening','--trigger','automatic']
            with patch.object(run_abort_trial,'NativeEntryRecorder',Recorder),patch.object(run_abort_trial,'AbortTrialBridge',Bridge),patch.object(run_abort_trial.sys,'argv',argv),redirect_stdout(io.StringIO()),self.assertRaisesRegex(RuntimeError,'no abort request was sent'):
                run_abort_trial.main()
        self.assertEqual(sent,[]);self.assertEqual(closed,[True])

    def test_default_manual_trigger_discards_preemptive_press_and_waits_for_a_new_press(self):
        opening=sample_opening();now=int(time.time());sent=[];closed=[];loop=[0];reports=[0]
        ordinary=dict(gameThread=True,matchingWorld=True,ordinaryEncounter=True,battleActive=True,
            commandMenu=True,units=4,waitingJobs=0,workingJobs=0,friendlyActions=1,
            openingHistoryUntouched=True,commandAbortReady=True,sequence=1,openingHash=opening.metadata['sha256'])
        class Shortcut:
            focused=True
            def __init__(self,executable):pass
            def poll(self):
                loop[0]+=1
                if loop[0]>12:raise AssertionError('Manual test did not complete')
                return 'controller' if loop[0] in [1,6] else None
        class Recorder:
            def __init__(self,directory):self.request='0'*32
            def heartbeat(self):pass
            def report(self):
                reports[0]+=1
                return None if reports[0]==1 else {'sequence':1}
            def disconnect(self):closed.append(True)
        class Bridge:
            safe_guard=staticmethod(AbortTrialBridge.safe_guard)
            def __init__(self,recorder):self.phase='idle'
            def status(self):
                guard=dict(ordinary,ordinaryEncounter=loop[0]!=1)
                return dict(available=True,phase=self.phase,guard=guard,message='Fixture',probeTime=now,
                    nativeTaskExit=dict({k:self.phase=='manager-cleared' for k in ['endIssued','forceEndFlagObserved','uiEndIssued','uiClosedObserved','forceEndResultObserved']},uncertain=False),
                    openingBoundaryPolicy='ordinary-first-menu-force-end-task',after={'sha256':'f'*64})
            def prepare(self,report):
                self_test.assertEqual(loop[0],7)
                sent.append('prepare');self.phase='armed';return opening
            def progression(self,status,after=False):return opening.progression
            def commit(self,status):sent.append('commit');self.phase='manager-cleared'
        self_test=self
        with tempfile.TemporaryDirectory() as folder:
            out=Path(folder)/'out'
            argv=['run_abort_trial.py',str(Path(folder)/'FFRHelperBattlePractice'),'--output',str(out),'--abort-at-opening']
            with patch.object(run_abort_trial,'NativeEntryRecorder',Recorder),patch.object(run_abort_trial,'AbortTrialBridge',Bridge),patch.object(run_abort_trial,'WindowsPracticeShortcut',Shortcut),patch.object(run_abort_trial.sys,'argv',argv),patch.object(run_abort_trial.time,'sleep'),redirect_stdout(io.StringIO()):
                run_abort_trial.main()
            self.assertEqual(json.loads((out/'activation.json').read_text()),dict(mode='manual',shortcut='controller',sequence=1))
        self.assertEqual(sent,['prepare','commit']);self.assertEqual(closed,[True])

    def test_manual_intent_cannot_follow_a_different_encounter_or_lost_foreground(self):
        for change in ['encounter','foreground']:
            now=int(time.time());loop=[0];sent=[];closed=[];opening=sample_opening()
            ordinary=dict(gameThread=True,matchingWorld=True,ordinaryEncounter=True,battleActive=True,
                commandMenu=True,units=4,waitingJobs=0,workingJobs=0,friendlyActions=1,
                openingHistoryUntouched=True,commandAbortReady=True,sequence=1,openingHash=opening.metadata['sha256'])
            class StopTest(Exception):pass
            class Shortcut:
                focused=True
                def __init__(self,executable):pass
                def poll(self):
                    loop[0]+=1
                    if loop[0]>5:raise StopTest()
                    self.focused=change!='foreground' or loop[0]==1
                    return 'keyboard' if loop[0]==1 else None
            class Recorder:
                def __init__(self,directory):self.request='0'*32
                def heartbeat(self):pass
                def report(self):return None if loop[0]==1 else {'unexpected':True}
                def disconnect(self):closed.append(True)
            class Bridge:
                safe_guard=staticmethod(AbortTrialBridge.safe_guard)
                def __init__(self,recorder):pass
                def status(self):
                    guard=dict(ordinary,sequence=2 if change=='encounter' and loop[0]>1 else 1)
                    return dict(available=True,phase='idle',guard=guard,message='Fixture',probeTime=now,
                        openingBoundaryPolicy='ordinary-first-menu-force-end-task')
                def prepare(self,report):sent.append('prepare');raise AssertionError('Stale intent used')
            with tempfile.TemporaryDirectory() as folder:
                argv=['run_abort_trial.py',str(Path(folder)/'FFRHelperBattlePractice'),'--output',str(Path(folder)/'out'),'--abort-at-opening']
                with patch.object(run_abort_trial,'NativeEntryRecorder',Recorder),patch.object(run_abort_trial,'AbortTrialBridge',Bridge),patch.object(run_abort_trial,'WindowsPracticeShortcut',Shortcut),patch.object(run_abort_trial.sys,'argv',argv),patch.object(run_abort_trial.time,'sleep'),redirect_stdout(io.StringIO()),self.assertRaises(StopTest):
                    run_abort_trial.main()
            self.assertEqual(sent,[]);self.assertEqual(closed,[True])

    def test_manual_focus_loss_after_prepare_cannot_commit(self):
        opening=sample_opening();now=int(time.time());loop=[0];sent=[];closed=[]
        ordinary=dict(gameThread=True,matchingWorld=True,ordinaryEncounter=True,battleActive=True,
            commandMenu=True,units=4,waitingJobs=0,workingJobs=0,friendlyActions=1,
            openingHistoryUntouched=True,commandAbortReady=True,sequence=1,openingHash=opening.metadata['sha256'])
        class Shortcut:
            focused=True
            def __init__(self,executable):pass
            def poll(self):loop[0]+=1;self.focused=loop[0]==1;return 'keyboard' if loop[0]==1 else None
        class Recorder:
            def __init__(self,directory):self.request='0'*32
            def heartbeat(self):pass
            def report(self):return {'sequence':1}
            def disconnect(self):closed.append(True)
        class Bridge:
            safe_guard=staticmethod(AbortTrialBridge.safe_guard)
            def __init__(self,recorder):self.phase='idle'
            def status(self):return dict(available=True,phase=self.phase,guard=ordinary,message='Fixture',probeTime=now,
                openingBoundaryPolicy='ordinary-first-menu-force-end-task')
            def prepare(self,report):sent.append('prepare');self.phase='armed';return opening
            def commit(self,status):sent.append('commit')
        with tempfile.TemporaryDirectory() as folder:
            argv=['run_abort_trial.py',str(Path(folder)/'FFRHelperBattlePractice'),'--output',str(Path(folder)/'out'),'--abort-at-opening']
            with patch.object(run_abort_trial,'NativeEntryRecorder',Recorder),patch.object(run_abort_trial,'AbortTrialBridge',Bridge),patch.object(run_abort_trial,'WindowsPracticeShortcut',Shortcut),patch.object(run_abort_trial.sys,'argv',argv),patch.object(run_abort_trial.time,'sleep'),redirect_stdout(io.StringIO()),self.assertRaisesRegex(RuntimeError,'lost foreground before commit'):
                run_abort_trial.main()
        self.assertEqual(sent,['prepare']);self.assertEqual(closed,[True])

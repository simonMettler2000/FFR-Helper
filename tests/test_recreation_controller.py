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
from test_opening_comparison import full_capture
from test_map_cache import sample_cache
from tools import run_abort_trial


class RecreationControllerTests(unittest.TestCase):
    def run_trial(self,mode='good'):
        source=full_capture();source.metadata['mapUnitCache']=sample_cache();child=copy.deepcopy(source);child.metadata['sequence']=2
        loop=[0];events=[];closed=[];phase=['idle'];recreation_phase=['idle'];now=int(time.time())
        ordinary=dict(gameThread=True,matchingWorld=True,ordinaryEncounter=True,battleActive=True,commandMenu=True,
            units=4,waitingJobs=0,workingJobs=0,friendlyActions=1,openingHistoryUntouched=True,commandAbortReady=True,
            sequence=1,openingHash=source.metadata['sha256'])
        class Shortcut:
            focused=True
            def __init__(self,executable):pass
            def poll(self):
                loop[0]+=1
                if loop[0]>12:raise AssertionError('Controller did not terminate')
                self.focused=not (mode=='focus-loss' and loop[0]>=4)
                return 'keyboard' if loop[0]==1 else None
        class Recorder:
            def __init__(self,directory):self.request='0'*32
            def heartbeat(self):pass
            def report(self):return dict(child=recreation_phase[0]=='verified')
            def opening(self,report):return child if report['child'] else source
            def disconnect(self):closed.append(True)
        class Bridge:
            safe_guard=staticmethod(AbortTrialBridge.safe_guard)
            def __init__(self,recorder):self.recorder=recorder
            def status(self):return dict(available=True,phase=phase[0],guard=ordinary,message='Fixture',probeTime=now,
                openingBoundaryPolicy='ordinary-first-menu-force-end-task',after={'sha256':'f'*64},
                nativeTaskExit=dict({k:phase[0]=='manager-cleared' for k in ['endIssued','forceEndFlagObserved',
                    'uiEndIssued','uiClosedObserved','forceEndResultObserved']},uncertain=False),
                recreationTrial=dict(available=mode!='unavailable',phase=recreation_phase[0],message='Fixture'))
            def prepare(self,report):events.append('exit-prepare');phase[0]='armed';return source
            def progression(self,status,after=False):return source.progression
            def commit(self,status):events.append('exit-commit');phase[0]='manager-cleared'
        class Recreation:
            def __init__(self,bridge):pass
            def probe(self,state):return state['recreationTrial']
            def prepare(self,state,opening):events.append('recreate-prepare');recreation_phase[0]='armed'
            def commit(self,state):events.append('recreate-commit');recreation_phase[0]='verified'
            def compare_child(self,state,opening):
                if opening.metadata['sequence']!=2:raise AssertionError('Old report reused')
                events.append('compare-child');return dict(currentFieldRecreationVerified=True,battleRestartImplemented=False)
        error=None
        with tempfile.TemporaryDirectory() as folder:
            out=Path(folder)/'out'
            argv=['run_abort_trial.py',str(Path(folder)/'FFRHelperBattlePractice'),'--output',str(out),
                  '--abort-at-opening','--recreate-after-abort']
            with patch.object(run_abort_trial,'NativeEntryRecorder',Recorder),patch.object(run_abort_trial,'AbortTrialBridge',Bridge),\
                 patch.object(run_abort_trial,'RecreationTrialBridge',Recreation),patch.object(run_abort_trial,'WindowsPracticeShortcut',Shortcut),\
                 patch.object(run_abort_trial.sys,'argv',argv),patch.object(run_abort_trial.time,'sleep'),redirect_stdout(io.StringIO()):
                try:run_abort_trial.main()
                except RuntimeError as exc:error=str(exc)
            if mode=='good':
                self.assertTrue((out/'abort-receipt.json').exists())
                self.assertTrue((out/'recreated-opening.ffropening').exists())
                self.assertFalse(json.loads((out/'recreation-comparison.json').read_text())['battleRestartImplemented'])
        self.assertEqual(closed,[True]);return events,error

    def test_exit_comparison_precedes_single_recreation_then_fresh_child_export(self):
        events,error=self.run_trial()
        self.assertIsNone(error)
        self.assertEqual(events,['exit-prepare','exit-commit','recreate-prepare','recreate-commit','compare-child'])

    def test_unavailable_recreation_refuses_before_any_exit(self):
        events,error=self.run_trial('unavailable');self.assertEqual(events,[]);self.assertIn('no abort request',error)

    def test_focus_loss_after_recreation_prepare_never_commits(self):
        events,error=self.run_trial('focus-loss')
        self.assertEqual(events,['exit-prepare','exit-commit','recreate-prepare']);self.assertIn('lost foreground',error)

    def test_recreation_cannot_be_selected_as_probe_or_automatic_trial(self):
        for flags in [[],['--abort-at-opening','--trigger','automatic']]:
            with patch.object(run_abort_trial.sys,'argv',['trial','FFRHelperBattlePractice','--output','unused',
                '--recreate-after-abort']+flags),redirect_stdout(io.StringIO()),patch('sys.stderr',io.StringIO()),self.assertRaises(SystemExit):
                run_abort_trial.main()

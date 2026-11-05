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
from ffr_helper.practice_input import ShortcutEdge
from test_opening_comparison import full_capture
from test_map_cache import sample_cache
import test_combat
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
                return 'keyboard' if loop[0]==1 or mode=='startup' and loop[0]==3 else None
        class Recorder:
            def __init__(self,directory):self.request='0'*32
            def heartbeat(self):pass
            def report(self):return dict(child=recreation_phase[0]=='verified')
            def opening(self,report):return child if report['child'] else source
            def disconnect(self):closed.append(True)
        class Bridge:
            safe_guard=staticmethod(AbortTrialBridge.safe_guard)
            def __init__(self,recorder):self.recorder=recorder
            def status(self):return dict(available=mode!='startup' or loop[0]>1,phase=phase[0],guard=ordinary,message='Fixture',probeTime=now,
                openingBoundaryPolicy='ordinary-first-menu-force-end-task',after={'sha256':'f'*64},
                nativeTaskExit=dict({k:phase[0]=='manager-cleared' for k in ['endIssued','forceEndFlagObserved',
                    'uiEndIssued','uiClosedObserved','forceEndResultObserved']},uncertain=False),
                recreationTrial=dict(available=mode!='unavailable' and (mode!='startup' or loop[0]>1),phase=recreation_phase[0],message='Fixture'))
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
        class Diagnostic:
            def __init__(self,recorder):self.request=None;self.polls=0
            def send(self,report):
                if report.get('child') is not True:raise AssertionError('Diagnostic did not use the new child')
                self.request={'deadline':now-2 if mode=='diagnostics-expired' else now+8};events.append('read-child')
            def receipt(self):
                self.polls+=1
                if self.polls==1:return None
                frame=test_combat.sample_status('a'*32)
                observation=test_combat.sample_observation()
                observation['timelineUnitLinks']=None if mode=='diagnostics-unavailable' else test_combat.sample_timeline()
                if mode=='diagnostics-stale':
                    observation['timelineUnitLinks']['unitsWithoutCurrentViewModel']=[0]
                    observation['timelineUnitLinks']['links'][0].update(cachedUnit=-1,sourceMatchesCached=False,
                        cachedParameterState='null',cachedUnitId=None,fallbackRelation=0)
                frame.update(request='b'*32,sequence=2,openingHash=source.metadata['sha256'],captureNumber=1,
                             captureTime=int(time.time()),phase='captured',observation=observation)
                return frame
        error=None
        with tempfile.TemporaryDirectory() as folder:
            out=Path(folder)/'out'
            argv=['run_abort_trial.py',str(Path(folder)/'FFRHelperBattlePractice'),'--output',str(out),
                  '--abort-at-opening','--recreate-after-abort']
            if mode.startswith('diagnostics'):argv.append('--capture-child-diagnostics')
            with patch.object(run_abort_trial,'NativeEntryRecorder',Recorder),patch.object(run_abort_trial,'AbortTrialBridge',Bridge),\
                 patch.object(run_abort_trial,'RecreationTrialBridge',Recreation),patch.object(run_abort_trial,'WindowsPracticeShortcut',Shortcut),\
                 patch.object(run_abort_trial,'CombatObservationBridge',Diagnostic),\
                 patch.object(run_abort_trial.sys,'argv',argv),patch.object(run_abort_trial.time,'sleep'),redirect_stdout(io.StringIO()):
                try:run_abort_trial.main()
                except RuntimeError as exc:error=str(exc)
            if mode=='good':
                self.assertTrue((out/'abort-receipt.json').exists())
                self.assertTrue((out/'recreated-opening.ffropening').exists())
                self.assertFalse(json.loads((out/'recreation-comparison.json').read_text())['battleRestartImplemented'])
            if mode in ['diagnostics','diagnostics-unavailable','diagnostics-stale']:
                self.assertTrue((out/'recreated-menu.ffrcombat.json').exists())
                self.assertTrue((out/'child-diagnostic-receipt.json').exists())
            if mode=='diagnostics-expired':self.assertFalse((out/'recreated-menu.ffrcombat.json').exists())
        self.assertEqual(closed,[True]);return events,error

    def test_exit_comparison_precedes_single_recreation_then_fresh_child_export(self):
        events,error=self.run_trial()
        self.assertIsNone(error)
        self.assertEqual(events,['exit-prepare','exit-commit','recreate-prepare','recreate-commit','compare-child'])

    def test_unavailable_recreation_refuses_before_any_exit(self):
        events,error=self.run_trial('unavailable');self.assertEqual(events,[]);self.assertIn('no abort request',error)

    def test_child_diagnostic_is_read_once_and_saved_before_disconnect(self):
        events,error=self.run_trial('diagnostics');self.assertIsNone(error)
        self.assertEqual(events[-2:],['compare-child','read-child'])
        self.assertEqual(events.count('exit-commit'),1);self.assertEqual(events.count('read-child'),1)

    def test_missing_child_timeline_stops_after_retaining_the_partial_evidence(self):
        events,error=self.run_trial('diagnostics-unavailable');self.assertIn('timeline links are unavailable',error)
        self.assertEqual(events.count('exit-commit'),1);self.assertEqual(events.count('read-child'),1)

    def test_expired_child_read_is_not_resent_and_never_starts_another_exit(self):
        events,error=self.run_trial('diagnostics-expired');self.assertIn('diagnostic expired',error)
        self.assertEqual(events.count('exit-commit'),1);self.assertEqual(events.count('read-child'),1)

    def test_missing_party_view_model_retains_evidence_and_refuses_another_cycle(self):
        events,error=self.run_trial('diagnostics-stale');self.assertIn('links are missing or stale',error)
        self.assertEqual(events.count('exit-commit'),1);self.assertEqual(events.count('read-child'),1)

    def test_startup_frame_discards_intent_then_accepts_a_new_press_after_readiness(self):
        events,error=self.run_trial('startup');self.assertIsNone(error)
        self.assertEqual(events,['exit-prepare','exit-commit','recreate-prepare','recreate-commit','compare-child'])

    def test_focus_loss_after_recreation_prepare_never_commits(self):
        events,error=self.run_trial('focus-loss')
        self.assertEqual(events,['exit-prepare','exit-commit','recreate-prepare']);self.assertIn('lost foreground',error)

    def test_recreation_cannot_be_selected_as_probe_or_automatic_trial(self):
        for flags in [[],['--abort-at-opening','--trigger','automatic']]:
            with patch.object(run_abort_trial.sys,'argv',['trial','FFRHelperBattlePractice','--output','unused',
                '--recreate-after-abort']+flags),redirect_stdout(io.StringIO()),patch('sys.stderr',io.StringIO()),self.assertRaises(SystemExit):
                run_abort_trial.main()

    def run_repeated_trial(self,mode='good'):
        source=full_capture();source.metadata['mapUnitCache']=sample_cache()
        frames=[copy.deepcopy(source) for _ in range(3)]
        for sequence,frame in enumerate(frames,1):frame.metadata['sequence']=sequence
        active=[1];phase=['idle'];recreated=['idle'];events=[];closed=[];completed=[0];polls=[0];waiting=[0]
        class Shortcut:
            focused=True
            def __init__(self,executable):self.edge=ShortcutEdge()
            def poll(self):
                polls[0]+=1
                if polls[0]>35:raise TimeoutError('Fixture stopped without another shortcut')
                if completed[0]==0:held=polls[0]>=2
                else:
                    waiting[0]+=1
                    # Retain the original hold for three polls, release, then
                    # deliberately press. A continuous hold never starts cycle 2.
                    held=mode=='held' or waiting[0]<=3 or waiting[0]>=6
                self.focused=not (mode=='focus-loss' and waiting[0]>=7)
                return self.edge.update(self.focused,(),held)
        class Recorder:
            def __init__(self,directory):self.request='0'*32
            def heartbeat(self):pass
            def report(self):return {'sequence':active[0]}
            def opening(self,report):return frames[report['sequence']-1]
            def disconnect(self):closed.append(True)
        class Bridge:
            safe_guard=staticmethod(AbortTrialBridge.safe_guard)
            def __init__(self,recorder):self.recorder=recorder
            def status(self):
                guard=dict(gameThread=True,matchingWorld=True,ordinaryEncounter=True,battleActive=True,commandMenu=True,
                    units=4,waitingJobs=0,workingJobs=0,friendlyActions=1,openingHistoryUntouched=True,commandAbortReady=True,
                    sequence=active[0],openingHash=source.metadata['sha256'])
                return dict(available=True,phase=phase[0],guard=guard,message='Fixture',probeTime=int(time.time()),
                    openingBoundaryPolicy='ordinary-first-menu-force-end-task',after={'sha256':'f'*64},
                    nativeTaskExit=dict({k:phase[0]=='manager-cleared' for k in ['endIssued','forceEndFlagObserved',
                        'uiEndIssued','uiClosedObserved','forceEndResultObserved']},uncertain=False),
                    recreationTrial=dict(available=True,phase=recreated[0],message='Fixture'),rearmTrial={'supported':mode!='old-runtime'})
            def prepare(self,report):events.append(('exit-prepare',active[0]));phase[0]='armed';return frames[active[0]-1]
            def progression(self,status,after=False):return source.progression
            def commit(self,status):events.append(('exit-commit',active[0]));phase[0]='manager-cleared'
        class Recreation:
            def __init__(self,bridge):self.abort=bridge
            def probe(self,state):return state['recreationTrial']
            def prepare(self,state,opening):events.append(('recreate-prepare',active[0]));recreated[0]='armed'
            def commit(self,state):events.append(('recreate-commit',active[0]));recreated[0]='verified';active[0]+=1
            def compare_child(self,state,opening):
                completed[0]+=1;events.append(('compare-child',active[0]));return {'battleRestartImplemented':False}
        class Rearm:
            probe=staticmethod(lambda state:state['rearmTrial'])
            def __init__(self,previous):self.request=None;self.archive=b'fixture-manifest';self.archive_progression=source.progression
            def can_prepare(self,status,child):return recreated[0]=='verified'
            def prepare(self,status,child):
                self.request={'nonce':'d'*32};events.append(('rearm-prepare',active[0]))
            def accepted(self,status):
                events.append(('rearm-accepted',active[0]));phase[0]='idle';recreated[0]='idle';return True
        class Diagnostic:
            def __init__(self,recorder):self.request=None;self.polls=0
            def send(self,report):self.request={'deadline':int(time.time())+8};events.append(('read-child',active[0]))
            def receipt(self):
                self.polls+=1
                if self.polls==1:return None
                frame=test_combat.sample_status('a'*32);observation=test_combat.sample_observation()
                observation['timelineUnitLinks']=test_combat.sample_timeline()
                frame.update(request='b'*32,sequence=active[0],openingHash=source.metadata['sha256'],captureNumber=1,
                             captureTime=int(time.time()),phase='captured',observation=observation)
                return frame
        error=None
        with tempfile.TemporaryDirectory() as folder:
            out=Path(folder)/'out'
            argv=['trial',str(Path(folder)/'FFRHelperBattlePractice'),'--output',str(out),
                  '--abort-at-opening','--recreate-after-abort','--cycles','2']
            if mode=='diagnostics':argv.append('--capture-child-diagnostics')
            with patch.object(run_abort_trial,'NativeEntryRecorder',Recorder),patch.object(run_abort_trial,'AbortTrialBridge',Bridge),\
                 patch.object(run_abort_trial,'RecreationTrialBridge',Recreation),patch.object(run_abort_trial,'RecreationRearmBridge',Rearm),\
                 patch.object(run_abort_trial,'WindowsPracticeShortcut',Shortcut),patch.object(run_abort_trial.sys,'argv',argv),\
                 patch.object(run_abort_trial,'CombatObservationBridge',Diagnostic),\
                 patch.object(run_abort_trial.time,'sleep'),redirect_stdout(io.StringIO()):
                try:run_abort_trial.main()
                except (RuntimeError,TimeoutError) as exc:error=str(exc)
            if mode in ['good','diagnostics']:
                for cycle in ['cycle-0001','cycle-0002']:
                    self.assertTrue((out/cycle/'recreation-receipt.json').exists())
                    self.assertTrue((out/cycle/'opening.ffropening').exists())
                self.assertEqual((out/'cycle-0001/cycle-manifest.json').read_bytes(),b'fixture-manifest')
                self.assertTrue((out/'cycle-0001/rearm-receipt.json').exists())
                self.assertFalse((out/'cycle-0002/rearm-request.json').exists())
            if mode=='diagnostics':
                for cycle in ['cycle-0001','cycle-0002']:
                    self.assertTrue((out/cycle/'recreated-menu.ffrcombat.json').exists())
        self.assertEqual(closed,[True]);return events,error

    def test_two_cycles_require_new_released_edge_and_preserve_separate_evidence(self):
        events,error=self.run_repeated_trial();self.assertIsNone(error)
        self.assertEqual(events,[('exit-prepare',1),('exit-commit',1),('recreate-prepare',1),('recreate-commit',1),
            ('compare-child',2),('rearm-prepare',2),('rearm-accepted',2),('exit-prepare',2),('exit-commit',2),
            ('recreate-prepare',2),('recreate-commit',2),('compare-child',3)])

    def test_held_shortcut_cannot_rearm_or_exit_twice(self):
        events,error=self.run_repeated_trial('held');self.assertIn('Fixture stopped',error)
        self.assertEqual(sum(name=='exit-commit' for name,_ in events),1)
        self.assertFalse(any(name.startswith('rearm') for name,_ in events))

    def test_each_diagnostic_is_saved_before_the_next_deliberate_cycle(self):
        events,error=self.run_repeated_trial('diagnostics');self.assertIsNone(error)
        self.assertLess(events.index(('read-child',2)),events.index(('rearm-prepare',2)))
        self.assertEqual(events[-1],('read-child',3))
        self.assertEqual(sum(name=='exit-commit' for name,_ in events),2)

    def test_focus_loss_during_rearm_does_not_start_a_second_abort(self):
        events,error=self.run_repeated_trial('focus-loss');self.assertIn('foreground/child changed',error)
        self.assertEqual(sum(name=='exit-commit' for name,_ in events),1)
        self.assertEqual(events[-1],('rearm-prepare',2))

    def test_legacy_runtime_refuses_multiple_cycles_before_first_abort(self):
        events,error=self.run_repeated_trial('old-runtime');self.assertEqual(events,[])
        self.assertIn('no abort request',error)

    def test_invalid_cycle_arguments_do_not_create_output(self):
        for flags in [['--cycles','0'],['--cycles','101'],['--cycles','2'],['--cycles','2','--abort-at-opening'],['--capture-child-diagnostics']]:
            with tempfile.TemporaryDirectory() as folder:
                out=Path(folder)/'out'
                with patch.object(run_abort_trial.sys,'argv',['trial','FFRHelperBattlePractice','--output',str(out)]+flags),\
                     redirect_stdout(io.StringIO()),patch('sys.stderr',io.StringIO()),self.assertRaises(SystemExit):
                    run_abort_trial.main()
                self.assertFalse(out.exists())

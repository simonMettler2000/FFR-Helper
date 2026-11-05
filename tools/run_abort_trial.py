"""Probe or explicitly run guarded untouched-opening trials. No restoration."""
import argparse
import json
from pathlib import Path
import sys
import time
import uuid

sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from ffr_helper.abort_trial import AbortTrialBridge,compare_progression
from ffr_helper.recreation_trial import RecreationTrialBridge,validate_source
from ffr_helper.recreation_rearm import RecreationRearmBridge
from ffr_helper.combat import CombatObservationBridge,export_combat
from ffr_helper.opening import export_opening
from ffr_helper.runtime import NativeEntryRecorder
from ffr_helper.storage import atomic_write
from ffr_helper.practice_input import WindowsPracticeShortcut, ManualIntent, CONTROLLER_SHORTCUT, KEYBOARD_SHORTCUT
from ffr_helper.legacy_input import LegacyBinding


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory',type=Path,help='Separately installed FFRHelperBattlePractice folder')
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--duration',type=int,default=300)
    parser.add_argument('--cycles',type=int,default=1,help='Maximum deliberate untouched-opening cycles; each needs a new released shortcut (default 1)')
    parser.add_argument('--abort-at-opening',action='store_true',help='Explicitly request one abort at the first ordinary command menu')
    parser.add_argument('--recreate-after-abort',action='store_true',help='Separate experiment: recreate the same setup from the verified unchanged current field; requires manual abort')
    parser.add_argument('--capture-child-diagnostics',action='store_true',help='Read and retain the recreated menu combat/timeline links before another cycle; requires recreation')
    parser.add_argument('--trigger',choices=['manual','automatic'],default='manual',
                        help='Default: deliberate foreground controller/keyboard shortcut; automatic is for explicitly selected diagnostics')
    parser.add_argument('--legacy-controller',type=Path,help='Locally calibrated legacy-controller .ffrinput.json binding')
    args=parser.parse_args()
    if not 1<=args.duration<=900:parser.error('--duration must be between 1 and 900 seconds')
    if not 1<=args.cycles<=100:parser.error('--cycles must be between 1 and 100')
    if args.directory.name.casefold()!='ffrhelperbattlepractice':
        parser.error('Choose the separate experimental practice component.')
    if args.legacy_controller and (not args.abort_at_opening or args.trigger!='manual'):
        parser.error('--legacy-controller requires an explicit manual abort trial')
    if args.recreate_after_abort and (not args.abort_at_opening or args.trigger!='manual'):
        parser.error('--recreate-after-abort requires --abort-at-opening and --trigger manual')
    if args.cycles>1 and not args.recreate_after_abort:
        parser.error('--cycles greater than 1 requires explicit manual --recreate-after-abort')
    if args.capture_child_diagnostics and not args.recreate_after_abort:
        parser.error('--capture-child-diagnostics requires --recreate-after-abort')
    args.output.mkdir(parents=True,exist_ok=False)
    cycle_number=1;cycle_output=args.output if args.cycles==1 else args.output/'cycle-0001'
    if cycle_output!=args.output:cycle_output.mkdir(exist_ok=False)
    recorder=NativeEntryRecorder(args.directory);bridge=AbortTrialBridge(recorder)
    recreation=RecreationTrialBridge(bridge) if args.recreate_after_abort else None
    shortcut=None;intent=ManualIntent();activation=None;next_heartbeat=0
    if args.abort_at_opening and args.trigger=='manual':
        executable=args.directory.parents[2]/'FFRS-Win64-Shipping.exe'
        shortcut=(WindowsPracticeShortcut(executable,LegacyBinding.load(args.legacy_controller)) if args.legacy_controller
                  else WindowsPracticeShortcut(executable))
    deadline=time.monotonic()+args.duration;pending=False;prepared=False;committed=False;last=None;recovery=None
    target_sequence=None;opening=None;exit_recorded=False;recreation_committed=False;child_requested=False
    rearm=None;child=None;child_diagnostic=None
    try:
        print('Opening-abort trial: '+('explicit abort requested' if args.abort_at_opening else 'probe only; no abort will be sent'),flush=True)
        if shortcut:
            print(f'Manual trigger only, with FFR focused: {CONTROLLER_SHORTCUT}, or {KEYBOARD_SHORTCUT}. Release before pressing; excluded/unsafe presses are discarded.',flush=True)
        while time.monotonic()<deadline:
            tick=time.monotonic()
            if tick>=next_heartbeat:
                recorder.heartbeat();next_heartbeat=tick+0.25
            pressed=shortcut.poll() if shortcut else None
            if child_diagnostic is not None and time.time()>child_diagnostic.request['deadline']+1:
                raise RuntimeError('Child diagnostic expired; no read or gameplay request will be resent.')
            state=bridge.status()
            if state is None:
                intent.clear()
                if pressed:print('Shortcut discarded: no fresh matching runtime status. Release and press again at a supported menu.',flush=True)
                time.sleep(0.02 if shortcut else 0.25);continue
            guard=state['guard']
            if child_diagnostic is not None:
                intent.clear()
                if pressed:print('Shortcut discarded while retaining the child diagnostic. Release and press again when ready.',flush=True)
                diagnostic=child_diagnostic.receipt()
                if diagnostic is None:
                    if time.time()>child_diagnostic.request['deadline']+1:
                        raise RuntimeError('Child diagnostic expired; no read or gameplay request will be resent.')
                else:
                    atomic_write(cycle_output/'child-diagnostic-receipt.json',json.dumps(diagnostic,indent=2).encode('utf8'))
                    if diagnostic['phase']!='captured':
                        raise RuntimeError('Child diagnostic was refused or unavailable: '+diagnostic['message'])
                    export_combat(cycle_output/'recreated-menu.ffrcombat.json',diagnostic)
                    links=diagnostic['observation'].get('timelineUnitLinks')
                    if links is None:
                        raise RuntimeError('Child combat captured but timeline links are unavailable; no further cycle will be requested.')
                    party_ids=set(child.metadata['entry']['friendliesUnitIdList'])
                    party_indices={i for i,unit in enumerate(diagnostic['observation']['units']) if unit['raw']['unitId'] in party_ids}
                    if (not party_indices or party_indices.intersection(links['unitsWithoutCurrentViewModel'])
                            or any(link['cachedUnit'] in party_indices and not link['sourceMatchesCached'] for link in links['links'])):
                        raise RuntimeError('Child party view-model links are missing or stale; evidence saved and no further cycle will be requested.')
                    print('Read-only child combat and timeline links saved. Actual portrait placement still needs visual confirmation.',flush=True)
                    child_diagnostic=None
                    if cycle_number>=args.cycles:return
                    rearm=RecreationRearmBridge(recreation)
                    print(f'Cycle {cycle_number}/{args.cycles} evidence saved. Waiting for release and a NEW foreground shortcut at this untouched child menu.',flush=True)
                time.sleep(0.02);continue
            if rearm is not None:
                # Keep the same input sampler: a held shortcut is never a new
                # edge. Rearming only resets helper bookkeeping, not gameplay.
                source=(child.metadata['sequence'],child.metadata['sha256'])
                ready=bridge.safe_guard(state,*source)
                if rearm.request is None:
                    ready=ready and rearm.can_prepare(state,child)
                    if pressed:
                        intent.press(pressed,source,ready,tick)
                        if not ready:print('Shortcut discarded: verified child/archive is not ready. Release and press again.',flush=True)
                    if intent.matches(source,ready,shortcut.focused,tick):
                        rearm.prepare(state,child)
                        atomic_write(cycle_output/'cycle-manifest.json',rearm.archive)
                        atomic_write(cycle_output/'cycle-progression.gvas',rearm.archive_progression)
                        atomic_write(cycle_output/'rearm-request.json',json.dumps(rearm.request,indent=2).encode('utf8'))
                        print('New deliberate shortcut received; verifying the next-cycle bookkeeping acknowledgment.',flush=True)
                else:
                    if not intent.matches(source,ready,shortcut.focused,tick):
                        raise RuntimeError('Rearm intent expired or the foreground/child changed; no new abort requested.')
                    if rearm.accepted(state):
                        atomic_write(cycle_output/'rearm-receipt.json',json.dumps(state,indent=2).encode('utf8'))
                        cycle_number+=1;cycle_output=args.output/f'cycle-{cycle_number:04d}'
                        cycle_output.mkdir(exist_ok=False)
                        bridge=AbortTrialBridge(recorder);recreation=RecreationTrialBridge(bridge)
                        pending=prepared=committed=exit_recorded=recreation_committed=child_requested=False
                        activation=recovery=target_sequence=opening=last=None
                        rearm=None
                        print(f'Cycle {cycle_number}/{args.cycles}: new transaction ready; checking the retained deliberate intent.',flush=True)
                time.sleep(0.02);continue
            if recreation and not prepared:
                # The DLL publishes an initial frame before its asynchronous
                # hook/profile initialization completes. No shortcut intent may
                # survive that frame, and no prepare can be sent from it.
                if not state['available']:
                    intent.clear();pending=False;target_sequence=None
                    time.sleep(0.02 if shortcut else 0.25);continue
                probe=recreation.probe(state)
                if not probe['available'] or probe['phase']!='idle':
                    raise RuntimeError('Current-field recreation is unavailable or already used; no abort request was sent.')
                if args.cycles>1 and not RecreationRearmBridge.probe(state)['supported']:
                    raise RuntimeError('This runtime does not support verified-child rearm; no abort request was sent.')
            summary=(state['available'],state['phase'],guard.get('battleActive'),guard.get('commandMenu'),
                     guard.get('friendlyActions'),guard.get('waitingJobs'),guard.get('workingJobs'),
                     guard.get('openingHistoryUntouched'),guard.get('commandAbortReady'))
            if summary!=last:
                print(f'Available={summary[0]}; phase={summary[1]}; battle={summary[2]}; command menu={summary[3]}; '
                      f'party turn beginnings={summary[4]}; jobs={summary[5]}/{summary[6]}; '
                      f'untouched history={summary[7]}; native task exit ready={summary[8]}. {state["message"]}',flush=True);last=summary
            if (args.abort_at_opening and not prepared and state['phase']=='idle'
                    and guard.get('ordinaryEncounter') is True and guard.get('commandMenu') is True):
                if state.get('openingBoundaryPolicy')!='ordinary-first-menu-force-end-task':
                    raise RuntimeError('Unsupported opening-boundary policy; no abort request was sent.')
                sequence,digest=guard.get('sequence'),guard.get('openingHash')
                ready=(type(sequence) is int and sequence>0 and isinstance(digest,str) and len(digest)==64
                        and bridge.safe_guard(state,sequence,digest))
                if pressed:
                    intent.press(pressed,(sequence,digest),ready,tick)
                    if ready:print('Manual shortcut accepted for this opening; verifying its capture before preparation.',flush=True)
                    else:print('Shortcut discarded: the current command-menu guard is not ready. Release and press again.',flush=True)
                authorized=not shortcut or intent.matches((sequence,digest),ready,shortcut.focused,tick)
                if shortcut and not authorized:
                    pending=False;target_sequence=None
                if ready and authorized:
                    if target_sequence is None:
                        target_sequence=sequence
                    elif sequence!=target_sequence:
                        raise RuntimeError('The selected encounter changed; no abort will be requested.')
                    if not pending:
                        recorder.request=uuid.uuid4().hex;pending=True
                    report=recorder.report()
                    if report is not None:
                        if recreation:
                            candidate=recorder.opening(report)
                            if candidate is None:raise RuntimeError('Complete opening is unavailable; no abort requested.')
                            validate_source(candidate)
                        activation=dict(mode=args.trigger,shortcut=intent.shortcut if shortcut else None,sequence=sequence)
                        intent.clear()
                        opening=bridge.prepare(report);prepared=True
                        if recreation:validate_source(opening)
                        export_opening(cycle_output/'opening.ffropening',opening)
                        atomic_write(cycle_output/'activation.json',json.dumps(activation,indent=2).encode('utf8'))
                        print('Prepare sent; verifying a durable progression-only recovery file before commit.',flush=True)
            elif shortcut and not prepared:
                intent.clear();pending=False
                if pressed:print('Shortcut discarded: wait at the untouched first command menu of a normal encounter. Release and press again there.',flush=True)
            if prepared and not committed and state['phase']=='armed':
                if shortcut and not shortcut.focused:
                    raise RuntimeError('FFR lost foreground before commit; no abort commit was sent. Do not retry this prepared launch.')
                recovery=bridge.progression(state)
                atomic_write(cycle_output/'before-abort.gvas',recovery)
                bridge.commit(state);committed=True
                print('One abort commit sent. No retries or restoration commands will be sent.',flush=True)
            if prepared and state['phase'] in ['rejected','timed-out']:
                raise RuntimeError(state['message'])
            if committed and not exit_recorded and state['phase']=='manager-cleared' and state.get('after',{}).get('sha256'):
                after=bridge.progression(state,after=True)
                atomic_write(cycle_output/'after-abort.gvas',after)
                result=compare_progression(recovery,after)
                result['nativeTaskExitUsed']=state.get('nativeTaskExit',{}).get('uncertain') is False and all(state.get('nativeTaskExit',{}).get(k) is True for k in ['endIssued','forceEndFlagObserved','uiEndIssued','uiClosedObserved','forceEndResultObserved'])
                result['openingHistoryUntouchedAfterCleanup']=state.get('guard',{}).get('openingHistoryUntouched') is True
                atomic_write(cycle_output/'comparison.json',json.dumps(result,indent=2).encode('utf8'))
                atomic_write(cycle_output/'abort-receipt.json',json.dumps(state,indent=2).encode('utf8'))
                print(json.dumps(result,indent=2),flush=True)
                exit_recorded=True
                if not recreation:
                    print('Native cleanup observed. Confirm the actual field return; this is not battle restart or save-state restoration.',flush=True)
                    return
                if not (result['allProgressionBytesUnchanged'] and result['nativeTaskExitUsed']
                        and result['openingHistoryUntouchedAfterCleanup'] and recovery==opening.progression):
                    raise RuntimeError('Exit/progression comparison failed; no recreation will be requested.')
                if not shortcut.focused:
                    raise RuntimeError('FFR lost foreground before recreation preparation; no entry requested.')
                recreation.prepare(state,opening)
                print('Separate current-field recreation prepared; no progression, effects or RNG will be restored.',flush=True)
            if recreation and exit_recorded:
                probe=recreation.probe(state)
                if probe['phase'] in ['refused','uncertain']:
                    raise RuntimeError(probe['message'])
                if probe['phase']=='armed' and not recreation_committed:
                    if not shortcut.focused:
                        raise RuntimeError('FFR lost foreground before recreation commit; no entry requested.')
                    recreation.commit(state);recreation_committed=True
                    print('One consuming entry commit sent; no retries.',flush=True)
                if probe['phase']=='verified' and recreation_committed:
                    if not child_requested:
                        recorder.request=uuid.uuid4().hex;child_requested=True
                    report=recorder.report()
                    if report is not None:
                        child=recorder.opening(report)
                        if child is None:raise RuntimeError('Verified child opening is unavailable; no retry.')
                        comparison=recreation.compare_child(state,child)
                        export_opening(cycle_output/'recreated-opening.ffropening',child)
                        atomic_write(cycle_output/'recreation-receipt.json',json.dumps(state,indent=2).encode('utf8'))
                        atomic_write(cycle_output/'recreation-comparison.json',json.dumps(comparison,indent=2).encode('utf8'))
                        print(json.dumps(comparison,indent=2),flush=True)
                        print('Current-field recreation observed. Confirm the actual new command menu; post-action rollback and full battle restart remain incomplete.',flush=True)
                        if args.capture_child_diagnostics:
                            child_diagnostic=CombatObservationBridge(recorder);child_diagnostic.send(report)
                            print('One read-only child diagnostic requested; no further gameplay cycle until its evidence is retained.',flush=True)
                            time.sleep(0.02);continue
                        if cycle_number>=args.cycles:
                            return
                        intent.clear();rearm=RecreationRearmBridge(recreation)
                        print(f'Cycle {cycle_number}/{args.cycles} evidence saved. Waiting for release and a NEW foreground shortcut at this untouched child menu.',flush=True)
            time.sleep(0.02 if shortcut else 0.25)
        if args.abort_at_opening:
            raise TimeoutError('Trial timed out. No automatic retry will be performed.')
        print('Probe finished; no abort was requested.',flush=True)
    except BaseException:
        # Keep a bound terminal/uncertain receipt for diagnosis without another
        # game command. Diagnostic I/O must never mask the original failure.
        try:
            terminal=bridge.status()
            if terminal is not None:
                atomic_write(cycle_output/'terminal-status.json',json.dumps(terminal,indent=2).encode('utf8'))
        except (OSError,ValueError,TypeError):pass
        raise
    finally:
        recorder.disconnect()


if __name__=='__main__':
    main()

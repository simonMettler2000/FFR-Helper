"""Probe or explicitly run one experimental opening-abort trial. No restoration."""
import argparse
import json
from pathlib import Path
import sys
import time
import uuid

sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from ffr_helper.abort_trial import AbortTrialBridge,compare_progression
from ffr_helper.opening import export_opening
from ffr_helper.runtime import NativeEntryRecorder
from ffr_helper.storage import atomic_write
from ffr_helper.practice_input import WindowsPracticeShortcut, ManualIntent, CONTROLLER_SHORTCUT, KEYBOARD_SHORTCUT


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory',type=Path,help='Separately installed FFRHelperBattlePractice folder')
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--duration',type=int,default=300)
    parser.add_argument('--abort-at-opening',action='store_true',help='Explicitly request one abort at the first ordinary command menu')
    parser.add_argument('--trigger',choices=['manual','automatic'],default='manual',
                        help='Default: deliberate foreground controller/keyboard shortcut; automatic is for explicitly selected diagnostics')
    args=parser.parse_args()
    if not 1<=args.duration<=900:parser.error('--duration must be between 1 and 900 seconds')
    if args.directory.name.casefold()!='ffrhelperbattlepractice':
        parser.error('Choose the separate experimental practice component.')
    args.output.mkdir(parents=True,exist_ok=False)
    recorder=NativeEntryRecorder(args.directory);bridge=AbortTrialBridge(recorder)
    shortcut=None;intent=ManualIntent();activation=None;next_heartbeat=0
    if args.abort_at_opening and args.trigger=='manual':
        shortcut=WindowsPracticeShortcut(args.directory.parents[2]/'FFRS-Win64-Shipping.exe')
    deadline=time.monotonic()+args.duration;pending=False;prepared=False;committed=False;last=None;recovery=None
    target_sequence=None
    try:
        print('Opening-abort trial: '+('explicit abort requested' if args.abort_at_opening else 'probe only; no abort will be sent'),flush=True)
        if shortcut:
            print(f'Manual trigger only, with FFR focused: {CONTROLLER_SHORTCUT}, or {KEYBOARD_SHORTCUT}. Release before pressing; excluded/unsafe presses are discarded.',flush=True)
        while time.monotonic()<deadline:
            tick=time.monotonic()
            if tick>=next_heartbeat:
                recorder.heartbeat();next_heartbeat=tick+0.25
            pressed=shortcut.poll() if shortcut else None
            state=bridge.status()
            if state is None:
                intent.clear()
                if pressed:print('Shortcut discarded: no fresh matching runtime status. Release and press again at a supported menu.',flush=True)
                time.sleep(0.02 if shortcut else 0.25);continue
            guard=state['guard']
            summary=(state['available'],state['phase'],guard.get('battleActive'),guard.get('commandMenu'),
                     guard.get('friendlyActions'),guard.get('waitingJobs'),guard.get('workingJobs'),
                     guard.get('openingHistoryUntouched'),guard.get('commandAbortReady'))
            if summary!=last:
                print(f'Available={summary[0]}; phase={summary[1]}; battle={summary[2]}; command menu={summary[3]}; '
                      f'party turn beginnings={summary[4]}; jobs={summary[5]}/{summary[6]}; '
                      f'untouched history={summary[7]}; native command abort ready={summary[8]}. {state["message"]}',flush=True);last=summary
            if (args.abort_at_opening and not prepared and state['phase']=='idle'
                    and guard.get('ordinaryEncounter') is True and guard.get('commandMenu') is True):
                if state.get('openingBoundaryPolicy')!='ordinary-first-menu-native-command-abort':
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
                        activation=dict(mode=args.trigger,shortcut=intent.shortcut if shortcut else None,sequence=sequence)
                        intent.clear()
                        opening=bridge.prepare(report);prepared=True
                        export_opening(args.output/'opening.ffropening',opening)
                        atomic_write(args.output/'activation.json',json.dumps(activation,indent=2).encode('utf8'))
                        print('Prepare sent; verifying a durable progression-only recovery file before commit.',flush=True)
            elif shortcut and not prepared:
                intent.clear();pending=False
                if pressed:print('Shortcut discarded: wait at the untouched first command menu of a normal encounter. Release and press again there.',flush=True)
            if prepared and not committed and state['phase']=='armed':
                if shortcut and not shortcut.focused:
                    raise RuntimeError('FFR lost foreground before commit; no abort commit was sent. Do not retry this prepared launch.')
                recovery=bridge.progression(state)
                atomic_write(args.output/'before-abort.gvas',recovery)
                bridge.commit(state);committed=True
                print('One abort commit sent. No retries or restoration commands will be sent.',flush=True)
            if prepared and state['phase'] in ['rejected','timed-out']:
                raise RuntimeError(state['message'])
            if committed and state['phase']=='manager-cleared' and state.get('after',{}).get('sha256'):
                after=bridge.progression(state,after=True)
                atomic_write(args.output/'after-abort.gvas',after)
                result=compare_progression(recovery,after)
                result['nativeCommandAbortUsed']=all(state.get('nativeCommandAbort',{}).get(k) is True for k in ['issued','forceEndFlagObserved','commandAbortFlagObserved'])
                result['openingHistoryUntouchedAfterCleanup']=state.get('guard',{}).get('openingHistoryUntouched') is True
                atomic_write(args.output/'comparison.json',json.dumps(result,indent=2).encode('utf8'))
                atomic_write(args.output/'abort-receipt.json',json.dumps(state,indent=2).encode('utf8'))
                print(json.dumps(result,indent=2),flush=True)
                print('Native cleanup observed. Confirm the actual field return; this is not battle restart or save-state restoration.',flush=True)
                return
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
                atomic_write(args.output/'terminal-status.json',json.dumps(terminal,indent=2).encode('utf8'))
        except (OSError,ValueError,TypeError):pass
        raise
    finally:
        recorder.disconnect()


if __name__=='__main__':
    main()

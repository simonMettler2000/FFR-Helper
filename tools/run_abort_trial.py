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


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory',type=Path,help='Separately installed FFRHelperBattlePractice folder')
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--duration',type=int,default=300)
    parser.add_argument('--abort-at-opening',action='store_true',help='Explicitly request one abort at the first ordinary command menu')
    args=parser.parse_args()
    if not 1<=args.duration<=900:parser.error('--duration must be between 1 and 900 seconds')
    if args.directory.name.casefold()!='ffrhelperbattlepractice':
        parser.error('Choose the separate experimental practice component.')
    args.output.mkdir(parents=True,exist_ok=False)
    recorder=NativeEntryRecorder(args.directory);bridge=AbortTrialBridge(recorder)
    deadline=time.monotonic()+args.duration;pending=False;prepared=False;committed=False;last=None;recovery=None
    target_sequence=None
    try:
        print('Opening-abort trial: '+('explicit abort requested' if args.abort_at_opening else 'probe only; no abort will be sent'),flush=True)
        while time.monotonic()<deadline:
            recorder.heartbeat()
            state=bridge.status()
            if state is None:
                time.sleep(0.25);continue
            guard=state['guard']
            summary=(state['available'],state['phase'],guard.get('battleActive'),guard.get('commandMenu'),
                     guard.get('friendlyActions'),guard.get('waitingJobs'),guard.get('workingJobs'))
            if summary!=last:
                print(f'Available={summary[0]}; phase={summary[1]}; battle={summary[2]}; command menu={summary[3]}; '
                      f'party turn beginnings={summary[4]}; jobs={summary[5]}/{summary[6]}. {state["message"]}',flush=True);last=summary
            if (args.abort_at_opening and not prepared and state['phase']=='idle'
                    and guard.get('ordinaryEncounter') is True and guard.get('commandMenu') is True):
                if state.get('openingBoundaryPolicy')!='ordinary-first-menu-event-history':
                    raise RuntimeError('Unsupported opening-boundary policy; no abort request was sent.')
                sequence,digest=guard.get('sequence'),guard.get('openingHash')
                if type(sequence) is int and sequence>0 and isinstance(digest,str) and len(digest)==64:
                    if target_sequence is None:
                        target_sequence=sequence
                    elif sequence!=target_sequence:
                        raise RuntimeError('The selected encounter changed; no abort will be requested.')
                if isinstance(sequence,int) and sequence>0 and isinstance(digest,str) and len(digest)==64 and bridge.safe_guard(state,sequence,digest):
                    if not pending:
                        recorder.request=uuid.uuid4().hex;pending=True
                    report=recorder.report()
                    if report is not None:
                        opening=bridge.prepare(report);prepared=True
                        export_opening(args.output/'opening.ffropening',opening)
                        print('Prepare sent; verifying a durable progression-only recovery file before commit.',flush=True)
            if prepared and not committed and state['phase']=='armed':
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
                atomic_write(args.output/'comparison.json',json.dumps(result,indent=2).encode('utf8'))
                print(json.dumps(result,indent=2),flush=True)
                print('Native cleanup observed. Confirm the actual field return; this is not battle restart or save-state restoration.',flush=True)
                return
            time.sleep(0.25)
        if args.abort_at_opening:
            raise TimeoutError('Trial timed out. No automatic retry will be performed.')
        print('Probe finished; no abort was requested.',flush=True)
    finally:
        recorder.disconnect()


if __name__=='__main__':
    main()

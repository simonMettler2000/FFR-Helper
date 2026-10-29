"""Capture one coherent partial field recovery; no gameplay control or restore."""
import argparse
from pathlib import Path
import sys
import time
import uuid

sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from ffr_helper.opening import export_opening
from ffr_helper.recovery import PartialRecoveryBridge, export_recovery
from ffr_helper.runtime import NativeEntryRecorder


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory',type=Path,help='Separately installed FFRHelperBattlePractice folder')
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--duration',type=int,default=300)
    parser.add_argument('--capture',action='store_true',help='Explicitly capture one cleared field boundary after observing an encounter')
    parser.add_argument('--stop-file',type=Path)
    args=parser.parse_args()
    if args.directory.name.casefold()!='ffrhelperbattlepractice':parser.error('Choose the separate development practice component.')
    if not 1<=args.duration<=900:parser.error('--duration must be between 1 and 900 seconds')
    args.output.mkdir(parents=True,exist_ok=False)
    recorder=NativeEntryRecorder(args.directory);bridge=PartialRecoveryBridge(recorder)
    deadline=time.monotonic()+args.duration;sent=pending=False;last=None
    try:
        while time.monotonic()<deadline:
            if args.stop_file is not None and args.stop_file.exists():return
            recorder.heartbeat();status=bridge.status()
            if status is None:time.sleep(0.25);continue
            ready=bridge.safe_boundary(status,status['boundarySequence'],status['boundaryOpeningHash'])
            summary=status['available'],ready,status['phase']
            if summary!=last:
                print(f'Partial recovery available={summary[0]}; cleared field boundary={summary[1]}; phase={summary[2]}.',flush=True);last=summary
            if args.capture and ready and not sent:
                if not pending:recorder.request=uuid.uuid4().hex;pending=True
                report=recorder.report()
                if report is not None:
                    source=bridge.send(report);export_opening(args.output/'source.ffropening',source);sent=True
            if sent:
                receipt=bridge.receipt()
                if receipt is not None and receipt['phase']!='pending-write':
                    if receipt['phase']!='captured':raise RuntimeError(receipt['message'])
                    capture=bridge.capture_receipt(receipt);export_recovery(args.output/'field.ffrrecovery',capture)
                    print('Verified partial field recovery archived. The helper invoked no restore, clock setter or slot writer.',flush=True);return
            time.sleep(0.25)
        if args.capture:raise TimeoutError('No matching recovery receipt arrived. No automatic retry was sent.')
    finally:recorder.disconnect()


if __name__=='__main__':main()

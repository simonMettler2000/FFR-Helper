"""Compare one detached normalized UObject; never assign the live root or reset clocks."""
import argparse
import json
from pathlib import Path
import sys
import time
import uuid

sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from ffr_helper.normalization_probe import NormalizationProbeBridge
from ffr_helper.opening import export_opening
from ffr_helper.runtime import NativeEntryRecorder
from ffr_helper.storage import atomic_write


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory',type=Path)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--duration',type=int,default=300)
    parser.add_argument('--probe-normalization',action='store_true',help='Explicitly import, change and compare one detached data object')
    parser.add_argument('--stop-file',type=Path)
    args=parser.parse_args()
    if args.directory.name.casefold()!='ffrhelperbattlepractice':parser.error('Choose the separate experimental practice component.')
    if not 1<=args.duration<=900:parser.error('--duration must be between 1 and 900 seconds')
    args.output.mkdir(parents=True,exist_ok=False)
    recorder=NativeEntryRecorder(args.directory);bridge=NormalizationProbeBridge(recorder)
    deadline=time.monotonic()+args.duration;sent=pending=False;last=None
    try:
        while time.monotonic()<deadline:
            if args.stop_file is not None and args.stop_file.exists():return
            recorder.heartbeat();status=bridge.status()
            if status is None:time.sleep(0.25);continue
            probe=status['normalizationProbe'];guard=status['guard']
            ready=bridge.safe_boundary(status,guard.get('sequence'),guard.get('openingHash'))
            summary=probe['available'],probe['loaderReady'],ready,probe['phase']
            if summary!=last:
                print(f'Detached normalization available={summary[0]}; loader={summary[1]}; field boundary={summary[2]}; phase={summary[3]}.',flush=True);last=summary
            if args.probe_normalization and ready and not sent:
                if not pending:recorder.request=uuid.uuid4().hex;pending=True
                report=recorder.report()
                if report is not None:
                    source=bridge.send(report);export_opening(args.output/'opening.ffropening',source);sent=True
            if sent:
                receipt=bridge.receipt()
                if receipt is not None:
                    atomic_write(args.output/'normalization-receipt.json',json.dumps(receipt,indent=2).encode())
                    if not receipt['passed']:raise RuntimeError(receipt['message'])
                    evidence=bridge.evidence(receipt)
                    for key,name in [('expected','normalization-expected.gvas'),('clone','normalization-clone.gvas')]:atomic_write(args.output/name,evidence[key])
                    print('Detached normalization matched the independent plan and preserved live progression. No live load, clock reset or restart was performed.',flush=True);return
            time.sleep(0.25)
        if args.probe_normalization:raise TimeoutError('No matching normalization receipt arrived; no retry was sent.')
    finally:recorder.disconnect()


if __name__=='__main__':main()

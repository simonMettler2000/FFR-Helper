"""Observe or explicitly request one detached native import comparison."""
import argparse
import json
from pathlib import Path
import sys
import time
import uuid

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from ffr_helper.import_probe import ImportProbeBridge
from ffr_helper.opening import export_opening
from ffr_helper.runtime import NativeEntryRecorder
from ffr_helper.storage import atomic_write


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path, help='Separately installed FFRHelperBattlePractice folder')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--duration', type=int, default=300)
    parser.add_argument('--probe-import', action='store_true', help='Create and compare one detached save UObject; never replace the live root')
    args = parser.parse_args()
    if not 1 <= args.duration <= 900:
        parser.error('--duration must be between 1 and 900 seconds')
    if args.directory.name.casefold() != 'ffrhelperbattlepractice':
        parser.error('Choose the separate experimental practice component.')
    args.output.mkdir(parents=True, exist_ok=False)
    recorder = NativeEntryRecorder(args.directory)
    bridge = ImportProbeBridge(recorder)
    deadline = time.monotonic()+args.duration
    pending = sent = False
    target = None
    last = None
    try:
        print('Detached import check: '+('one comparison requested' if args.probe_import else 'observe only'), flush=True)
        while time.monotonic() < deadline:
            recorder.heartbeat()
            status = bridge.status()
            if status is None:
                time.sleep(0.25)
                continue
            probe = status['importProbe']
            summary = probe['available'], probe['loaderReady'], probe['phase'], probe['message']
            if summary != last:
                print(f'Available={summary[0]}; loader initialized={summary[1]}; phase={summary[2]}. {summary[3]}', flush=True)
                last = summary
            guard = status['guard']
            # Preemptive/setup frames can already count party turn beginnings.
            # Select only a supported command menu; never blame those excluded
            # frames on a user action or bind the probe to their encounter.
            if (args.probe_import and not sent and probe['phase'] == 'idle' and probe['loaderReady']
                    and guard.get('ordinaryEncounter') is True and guard.get('commandMenu') is True):
                if probe.get('boundaryPolicy')!='ordinary-idle-menu':
                    raise RuntimeError('The import boundary policy is unsupported; no import request was sent.')
                sequence, digest = guard.get('sequence'), guard.get('openingHash')
                if type(sequence) is int and sequence > 0 and isinstance(digest, str) and len(digest) == 64:
                    if target is None:
                        target = sequence
                    if sequence != target:
                        raise RuntimeError('The selected encounter changed; no import will be requested.')
                    if bridge.safe_guard(status, sequence, digest):
                        if not pending:
                            recorder.request = uuid.uuid4().hex
                            pending = True
                        report = recorder.report()
                        if report is not None:
                            opening = bridge.send(report)
                            sent = True
                            export_opening(args.output/'opening.ffropening', opening)
                            print('One detached import requested. Waiting for its receipt.', flush=True)
            if sent:
                receipt = bridge.receipt()
                if receipt is not None:
                    atomic_write(args.output/'import-receipt.json', json.dumps(receipt, indent=2).encode('utf8'))
                    if not receipt['passed']:
                        raise RuntimeError(receipt['message'])
                    print('Engine import round trip matched, live progression stayed unchanged and temporary object pins were released.', flush=True)
                    print('This verifies detached import only. No battle restart or save-state restore was performed.', flush=True)
                    return
            time.sleep(0.25)
        if args.probe_import:
            raise TimeoutError('The import probe timed out. Check its receipt; no automatic retry will be sent.')
    finally:
        recorder.disconnect()


if __name__ == '__main__':
    main()

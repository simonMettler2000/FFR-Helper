"""Observe or explicitly capture one partial combat frame; no mutation or restore."""
import argparse
from pathlib import Path
import sys
import time
import uuid

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from ffr_helper.combat import CombatObservationBridge, export_combat
from ffr_helper.runtime import NativeEntryRecorder


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path, help='Separately installed FFRHelperBattlePractice folder')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--duration', type=int, default=300)
    parser.add_argument('--capture', action='store_true', help='Explicitly copy one frame at a supported command menu')
    parser.add_argument('--minimum-party-actions', type=int, default=0, help='Wait until the summed party action counter reaches this value')
    parser.add_argument('--stop-file', type=Path, help='Stop and expire the lease when this marker exists')
    args = parser.parse_args()
    if args.directory.name.casefold() != 'ffrhelperbattlepractice':
        parser.error('Choose the separate development practice component.')
    if not 1 <= args.duration <= 900:
        parser.error('--duration must be between 1 and 900 seconds')
    if not 0 <= args.minimum_party_actions <= 100000:
        parser.error('--minimum-party-actions must be between 0 and 100000')
    args.output.mkdir(parents=True, exist_ok=False)
    recorder = NativeEntryRecorder(args.directory)
    bridge = CombatObservationBridge(recorder)
    deadline = time.monotonic()+args.duration
    sent = pending = False
    last = None
    try:
        while time.monotonic() < deadline:
            if args.stop_file is not None and args.stop_file.exists():
                return
            recorder.heartbeat()
            status = bridge.status()
            if status is None:
                time.sleep(0.25)
                continue
            control = bridge.control.status()
            guard = control.get('guard', {}) if control else {}
            sequence, digest = guard.get('sequence'), guard.get('openingHash')
            ready = bridge.safe_boundary(control, sequence, digest)
            summary = status['available'], ready, status['phase']
            if summary != last:
                print(f'Combat observation available={summary[0]}; supported command boundary={summary[1]}; phase={summary[2]}.', flush=True)
                last = summary
            if args.capture and ready and not sent and guard['friendlyActions'] >= args.minimum_party_actions:
                if not pending:
                    recorder.request = uuid.uuid4().hex
                    pending = True
                report = recorder.report()
                if report is not None:
                    bridge.send(report)
                    sent = True
            if sent:
                report = bridge.receipt()
                if report is not None:
                    if report['phase'] != 'captured':
                        raise RuntimeError(report['message'])
                    export_combat(args.output/'command.ffrcombat.json', report)
                    print(f'Captured {len(report["observation"]["units"])} units into a partial diagnostic. No restoration or gameplay mutation occurred.', flush=True)
                    return
            time.sleep(0.25)
        if args.capture:
            raise TimeoutError('No matching combat receipt arrived. No automatic retry was sent.')
    finally:
        recorder.disconnect()


if __name__ == '__main__':
    main()

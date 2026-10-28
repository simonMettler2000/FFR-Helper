"""Collect private read-only observations while a user operates the game."""
import argparse
from datetime import datetime, timezone
from pathlib import Path
import sys
import time
import uuid

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from ffr_helper.runtime import RuntimeRecorder, export_report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('recorder', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--duration', type=int, default=600)
    parser.add_argument('--stop-file', type=Path, help='Optional local marker file for graceful shutdown')
    args = parser.parse_args()
    if not 1 <= args.duration <= 1800:
        parser.error('--duration must be between 1 and 1800 seconds')
    args.output.mkdir(parents=True, exist_ok=True)
    recorder = RuntimeRecorder(args.recorder)
    deadline = time.monotonic()+args.duration
    pending = None
    next_capture = 0
    last_summary = None
    samples = 0
    first_io_error = None
    try:
        print('Read-only observation session started; waiting for the game recorder.', flush=True)
        while time.monotonic() < deadline and not (args.stop_file and args.stop_file.exists()):
            try:
                recorder.heartbeat()
            except OSError:
                if first_io_error is None:
                    first_io_error = time.monotonic()
                    print('Recorder connection is temporarily busy; retrying.', flush=True)
                if time.monotonic()-first_io_error > 10:
                    raise
                time.sleep(0.5)
                continue
            first_io_error = None
            status = recorder.status()
            now = time.monotonic()
            if status and pending is None and now >= next_capture:
                # heartbeat() above already published the previous request.
                # Updating the in-memory request defers its publication to the
                # next heartbeat and avoids a second immediate file replace.
                recorder.request = uuid.uuid4().hex
                pending = recorder.request
                next_capture = now+2
            if pending is not None:
                report = recorder.report()
                if report is not None:
                    stamp = datetime.now(timezone.utc).strftime('%Y%m%d-%H%M%S-%f')
                    export_report(args.output/(stamp+'.ffrruntime.json'), report)
                    samples += 1
                    battle = report['objects'].get('battle', {})
                    ui = report['objects'].get('battleUI', {})
                    active = ui.get('values', {}).get('bDuringBattle')
                    summary = (status is not None, active, len(battle.get('units', [])), len(report['events']))
                    if summary != last_summary:
                        print(f'Observed: battle active={active}; units={summary[2]}; events={summary[3]}; samples={samples}', flush=True)
                        last_summary = summary
                    pending = None
                elif now > next_capture+8:
                    pending = None
                    next_capture = now+2
                    print('Observation request timed out; waiting for a stable game frame.', flush=True)
            if not status and last_summary != 'waiting':
                print('Waiting for a fresh matching recorder heartbeat.', flush=True)
                last_summary = 'waiting'
            time.sleep(0.5)
    except KeyboardInterrupt:
        pass
    finally:
        try:
            recorder.disconnect()
        except OSError:
            print('Disconnect file busy; the short-lived observation lease will expire.', flush=True)
    print(f'Observation finished: {samples} private reports. No restore commands were sent.', flush=True)


if __name__ == '__main__':
    main()

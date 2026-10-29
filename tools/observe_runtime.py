"""Collect private read-only observations while a user operates the game."""
import argparse
from datetime import datetime, timezone
import json
from pathlib import Path
import sys
import time
import uuid

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from ffr_helper.runtime import RuntimeRecorder, NativeEntryRecorder, export_report
from ffr_helper.opening import export_opening
from ffr_helper.decision_history import validate_decision_history
from ffr_helper.menu_frame import validate_menu_frame,menu_frame_is_recent
from ffr_helper.skill_history import validate_skill_history
from ffr_helper.abort_trial import AbortTrialBridge
from ffr_helper.storage import atomic_write


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('recorder', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--duration', type=int, default=600)
    parser.add_argument('--stop-file', type=Path, help='Optional local marker file for graceful shutdown')
    parser.add_argument('--native', action='store_true', help='Connect the experimental native entry observer instead of the Lua recorder')
    parser.add_argument('--practice-guard',action='store_true',help='Read practice readiness/status only; never send an import or abort command')
    args = parser.parse_args()
    if not 1 <= args.duration <= 1800:
        parser.error('--duration must be between 1 and 1800 seconds')
    if args.practice_guard and (not args.native or args.recorder.name.casefold()!='ffrhelperbattlepractice'):
        parser.error('--practice-guard requires the separate native FFRHelperBattlePractice component')
    args.output.mkdir(parents=True, exist_ok=True)
    recorder = (NativeEntryRecorder if args.native else RuntimeRecorder)(args.recorder)
    practice=AbortTrialBridge(recorder) if args.practice_guard else None
    deadline = time.monotonic()+args.duration
    pending = None
    next_capture = 0
    last_summary = None
    samples = 0
    first_io_error = None
    last_opening = None
    last_decision = False
    last_menu = False
    last_menu_summary = None
    last_skill = False
    last_guard = False
    last_guard_summary = None
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
            if practice is not None:
                guarded=practice.status()
                if guarded is not None:
                    if guarded!=last_guard:
                        stamp=datetime.now(timezone.utc).strftime('%Y%m%d-%H%M%S-%f')
                        atomic_write(args.output/(stamp+'.ffrguard.json'),json.dumps(guarded,indent=2).encode('utf8'))
                        last_guard=guarded
                    guard=guarded['guard'];probe=guarded.get('importProbe',{})
                    summary=(guarded['available'],probe.get('available'),probe.get('loaderReady'),guard.get('ordinaryEncounter'),
                        guard.get('commandMenu'),guard.get('friendlyActions'),guard.get('fieldReady'),
                        -2<=time.time()-guarded.get('probeTime',0)<=2)
                    if summary!=last_guard_summary:
                        print(f'Practice readiness only: {summary}; abort, import, loader, ordinary, menu, turn beginnings, field, recent. No commands sent.',flush=True)
                        last_guard_summary=summary
            if args.native and status and 'skillHistory' in status:
                try:
                    history=validate_skill_history(status['skillHistory'])
                    if history!=last_skill:
                        stamp=datetime.now(timezone.utc).strftime('%Y%m%d-%H%M%S-%f')
                        evidence=dict(format='ffr-helper-skill-history-observation',readOnly=True,restorable=False,
                            session=recorder.session,time=status['time'],skillHistory=history)
                        atomic_write(args.output/(stamp+'.ffrskill.json'),json.dumps(evidence,indent=2).encode('utf8'))
                        last_skill=history
                        if history is not None:print(f"Skill.Begin events observed: {history['skillBeginEvents']}; coverage remains unreviewed.",flush=True)
                except (OSError,ValueError,TypeError,KeyError):pass
            if args.native and status and 'menuObservation' in status:
                try:
                    menu=validate_menu_frame(status['menuObservation'])
                    if menu!=last_menu:
                        stamp=datetime.now(timezone.utc).strftime('%Y%m%d-%H%M%S-%f')
                        evidence=dict(format='ffr-helper-menu-frame-observation',readOnly=True,restorable=False,
                            session=recorder.session,time=status['time'],menuObservation=menu)
                        atomic_write(args.output/(stamp+'.ffrmenu.json'),json.dumps(evidence,indent=2).encode('utf8'))
                        last_menu=menu
                    summary=(menu_frame_is_recent(menu),menu['commandMenu'],menu['turnBeginnings']) if menu else None
                    if summary!=last_menu_summary:
                        print(f'Read-only menu frame: {summary}; fields are recency, command menu, turn beginnings. Retry remains disabled.',flush=True)
                        last_menu_summary=summary
                except (OSError,ValueError,TypeError,KeyError):pass
            if args.native and status and 'decisionHistory' in status:
                try:
                    history=validate_decision_history(status['decisionHistory'])
                    if history!=last_decision:
                        stamp=datetime.now(timezone.utc).strftime('%Y%m%d-%H%M%S-%f')
                        evidence=dict(format='ffr-helper-decision-history-observation',readOnly=True,restorable=False,
                            session=recorder.session,time=status['time'],timeSource='observer-worker-not-game-tick',decisionHistory=history)
                        atomic_write(args.output/(stamp+'.ffrdecision.json'),json.dumps(evidence,indent=2).encode('utf8'))
                        last_decision=history
                        if history is not None:
                            print(f"Decision-handler entries observed: {history['handlerEntries']}; command-boundary coverage remains unverified.",flush=True)
                except (OSError,ValueError,TypeError,KeyError):
                    # An optional malformed diagnostic never suppresses capture.
                    pass
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
                    if args.native:
                        try:
                            opening=recorder.opening(report)
                            identity=(opening.metadata['sequence'],opening.metadata['sha256']) if opening else None
                            if opening is not None and identity!=last_opening:
                                export_opening(args.output/(stamp+'.ffropening'),opening)
                                last_opening=identity
                                print(f'Private opening capture verified: {len(opening.progression):,} serialized bytes. Restoration is unavailable.',flush=True)
                        except (OSError,ValueError):
                            print('Serialized opening is not verified yet; collection will retry on the next report.',flush=True)
                    samples += 1
                    battle = report['objects'].get('battle', {})
                    ui = report['objects'].get('battleUI', {})
                    active = ui.get('values', {}).get('bDuringBattle')
                    native_state = report['objects'].get('nativeEntry', {})
                    summary = ((native_state.get('ready'), len(report['events']), native_state.get('captureFailures')) if args.native
                               else (status is not None, active, len(battle.get('units', [])), len(report['events'])))
                    if summary != last_summary:
                        if args.native:
                            note = (status or {}).get('message', 'Waiting for a fresh status heartbeat')
                            print(f'Native entry observer: ready={summary[0]}; entries={summary[1]}; capture failures={summary[2]}; samples={samples}. {note}', flush=True)
                        else:
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

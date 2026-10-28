# Experimental opening-abort trial

This is a separate, mutation-capable development component. It has passed
isolated dummy-process tests but has **not been installed or tested in FFR**.
It does not restart a battle, deserialize a save or restore a save state.

The next implementation milestone is verifying the game's ForceEnd cleanup
without a win, loss or escape. This trial can request that path once at the
first player-command menu of a captured ordinary encounter. It is intended for
a separately approved test with backed-up practice data.

## Scope and boundaries

- Requires the exact game/UE4SS profiles used by the verified capture observer.
  Additional loaded code fingerprints cover the battle tick, world/subsystem
  getters and EndBattle function.
- Uses a separate FFRHelperBattlePractice folder and two distinct channels:
  read-only capture reports and an explicitly mutation-capable abort status.
  The existing capture client cannot send abort requests.
- A request binds the current session, encounter revision, opening hash and
  unique nonce, with an expiry of at most eight seconds.
- The game-thread guard requires a matching current world/map manager, ordinary
  escapable encounter flags, battle UI command phase, selected party unit,
  all captured friendly units present, zero friendly actions and idle job queues.
  Non-default battle-event IDs, bounty/test/preemptive/result-skipping flags and
  disabled escape cause refusal. This does not identify every possible script context.
  These guards remain to be verified in FFR; they are not a completeness claim.
- Preparation serializes progression into a separate recovery buffer. The worker
  commits and reads back a private recovery file before reporting the trial armed.
  The client also checks its hash, GVAS class, engine, footer and required fields.
- Only a matching explicit commit may call the verified EndBattle function.
  State is marked before that call, and there is no automatic resend or retry.
  One attempt per game launch is supported, including rejected attempts.
- Cleanup observation requires the same encounter/session, empty unit/job queues,
  cleared battle-active flag and cleared battle UI flag. A progression snapshot
  is then captured for comparison. The user still needs to confirm actual field
  return. A changed context or unverified cleanup is reported without a retry.
- Recovery files contain progression only. They cannot resume an ongoing battle;
  this trial has no automatic rollback or full battle-state recovery.
- No normal game save is written. Saving anywhere remains uninstalled/deferred.

The ordinary observer build excludes this command/tick-hook code. Packaging
checks refuse to label this DLL as a read-only observer, and refuse to include
mock/fixture binaries. The development archive omits its enabled marker.

## Build and stage

    python tools/fetch_native_dependencies.py
    cmake -S native -B local/native-practice -A x64 -DFFR_BUILD_NATIVE_OBSERVER=ON -DFFR_BUILD_ABORT_TRIAL=ON -DMINHOOK_SOURCE_DIR=<absolute-path>/local/native-deps/minhook-1.3.4
    cmake --build local/native-practice --config Release
    ctest --test-dir local/native-practice -C Release --output-on-failure
    python tools/package_abort_trial.py --dll local/native-practice/Release/FFRHelperBattlePractice.dll

Do not enable this alongside FFRHelperRuntimeNative: both intercept encounter
entry, and the second module refuses an already changed code profile. An opted-in
test installation needs the old native observer disabled while the game is closed.
The Lua recorder can coexist, subject to its loader compatibility fix.

The exact-build signature preparer stages that fix from the user's own executable:

    python tools/prepare_runtime_profile.py --exe <path-to-FFRS-Win64-Shipping.exe>

It verifies the full executable hash, PE architecture, executable-section bounds,
function hash and unique match before generating a symbol-lookup script. It never
overwrites a different existing override. It does not install the staged file or
establish live compatibility; installation/relaunch testing remains pending.

## Test controller

Start this before entering the familiar ordinary encounter so the opening is
captured in the same session. Use a fresh private output directory.

Probe only:

    python tools/run_abort_trial.py <FFRHelperBattlePractice-folder> --output local/abort-probe --duration 300

One explicitly requested abort at the selected encounter's opening:

    python tools/run_abort_trial.py <FFRHelperBattlePractice-folder> --output local/abort-test --duration 300 --abort-at-opening

The client binds the first captured encounter and refuses a changed encounter or
prior party action. It saves the opening and progression recovery before commit,
then compares counters, inventory, EXP/MRP and HP/MP after native cleanup. HP/MP
differences are reported separately because field effects can run after cleanup.
The comparison does not claim that mid-battle state was restored.

After live ForceEnd behavior is understood, the remaining restart work is
deserialization/rebinding and reconstruction through the verified engine allocator.
Full mid-battle states additionally require scheduler, AI, status, jobs, event and
RNG reconstruction. Those operations are not exposed by this trial.

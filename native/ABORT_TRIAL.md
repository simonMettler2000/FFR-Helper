# Experimental opening-abort trial

This is a separate, mutation-capable development component. It has passed
isolated dummy-process tests but has **not been installed or tested in FFR**.
It does not restart a battle, replace live progression or restore a save state.
It now also has a separate, explicit **detached import comparison** described
below. Creating a comparison UObject does not apply it to the running game.

One implementation milestone is verifying the game's ForceEnd cleanup
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

## Detached import comparison

This separate experiment invokes the exact-profile LoadGameFromMemory on the
observer's own retained opening bytes. It cannot accept a file path or bytes from
the client. The native header gate checks GVAS version, engine, save class and
footer; the desktop additionally parses the complete captured object.

The experiment requires the same ordinary first-command boundary as the abort
trial, plus successful UE4SS engine initialization. If the loader's symbol scan
failed, the command refuses to create an object. Its primitive rooting exports
are resolved by exact name and RVA against the already verified loader hash.
No loader C++ containers or SDK object layouts cross the DLL boundary.

The live root remains assigned throughout. Temporary GC roots retain the live
object and detached clone for the synchronous comparison. Existing roots belong
to the engine and are preserved. The clone must have the exact save vtable and
same class as the current save. Success requires all of:

- Clone serialization exactly equals the captured opening bytes.
- The input buffer is unchanged.
- The live save pointer and its before/after serialized bytes are unchanged.
- Every temporary root added by the probe is verified released.

One attempt per process is supported; abort and import probes cannot be combined
in the same launch. Uncertain release retains at most the probe's two objects
until process exit and reports failure. There is no automatic retry or recovery
write. The ordinary read-only observer excludes this code entirely.

Observe availability without issuing an import:

    python tools/run_import_probe.py <FFRHelperBattlePractice-folder> --output local/import-observe --duration 300

Explicitly request one detached import comparison:

    python tools/run_import_probe.py <FFRHelperBattlePractice-folder> --output local/import-test --duration 300 --probe-import

The private receipt binds session, nonce, opening revision and hash. It reports
comparison outcomes without presenting enemy/story details. Isolated DLL fixtures
cover success, uninitialized-loader refusal, clone mismatch, live-data changes,
prior party actions and command replay. **Live FFR verification remains pending.**

## Read-only command-state observations

The practice component also has a separate read-only observation channel. It
does not send abort/import commands, invoke a serializer or change gameplay
values. The native copy runs only at a matching ordinary player-command menu,
with the known world and idle queues. Prior party actions are permitted for this
read-only operation. Pending abort/import experiments block it.

The six-line combat-request protocol carries only session, nonce, expiry,
opening revision and opening hash. It cannot name files, addresses or values to
write. Nonces are consumed before copying; replay never captures a different
moment. Request history and detached output are bounded. Actor references become
capture-local indices, preserving duplicate unit IDs and nested AI targets.

Optional `unitRelations` adds vision/owner links, last damage dealer, ordered
damage sources and active skill targets. Native actor and parameter links are
translated through separate domains to the same local indices. If any referenced
object lies outside the capture, the whole optional graph is null rather than
inventing a null relationship. Other components can still be retained. Old files
with no graph remain valid partial diagnostics. Shared reference limits apply
across AI, timeline, targets and relations; mimic payloads and complete lifecycle
state are still missing.

Optional `continuousEffects` adds a base-component pool and ordered per-unit
effect-index lists. It retains raw life-turn counters (including negative
sentinels), data IDs/types/timing/condition/probability/flag fields, integer
parameters and local source/target unit links. Shared objects and distinct
objects with the same data ID remain distinct concepts. A foreign/unreadable
source, target or object makes the whole graph null; other partial diagnostics
can remain available. Known empty lists are preserved. Object/parameter budgets
and the shared reference limit are checked by native and Python code. Subclass
state, actual application timing and restoration remain unverified; this is not
complete status coverage. The addition is fixture-tested and uninstalled.

Start the controller before entering the encounter so its opening is observed
in the same session. Observe readiness without requesting a frame:

    python tools/observe_combat.py <FFRHelperBattlePractice-folder> --output local/combat-observe --duration 300

Explicitly capture one supported command boundary:

    python tools/observe_combat.py <FFRHelperBattlePractice-folder> --output local/combat-first --capture

Wait for at least one party action before capturing:

    python tools/observe_combat.py <FFRHelperBattlePractice-folder> --output local/combat-after-action --capture --minimum-party-actions 1

Use a fresh output directory. The optional --stop-file marker and Ctrl+C expire
the observation lease on exit. The client prints counts/readiness, without enemy
names. Its `.ffrcombat.json` export is a **partial diagnostic**, not a restore
format. Raw resource/base-stat components may differ from the game's displayed
values: HP has a separate temporary component, MP/stagger getters clamp, and
level calculation applies a multiplier.

Effect durations, resistance maps/modifiers, shared AI state, the main scheduler,
callbacks, RNG, world/lifecycle state and restoration remain missing. Both the
native producer and Python consumer state these gaps and reject completeness
claims. Actual-DLL fixture/consumer tests pass; live FFR verification is pending.

After ForceEnd and detached import behavior are understood, remaining restart
work includes live progression rebinding and verified cache notifications, then
reconstruction through the engine allocator.
Full mid-battle states additionally require scheduler, AI, status, jobs, event and
RNG reconstruction. Those operations are not exposed by this trial.

## Read-only partial field recovery

The separate [partial recovery channel](PARTIAL_RECOVERY.md) copies coherent
progression/field/clock evidence after a captured ordinary encounter. It requires
the actual matching game thread/map, cleared battle UI/manager and empty units,
selection, targets and jobs. It never invokes abort, import, load, reset or a slot
writer. Its worker commits and reads back both fixed files before publishing a
receipt. The Python client validates full GVAS and metadata, checks publication
races and exports only a non-restorable `.ffrrecovery` archive.

Start before entering the encounter and use a fresh output directory:

    python tools/observe_recovery.py <FFRHelperBattlePractice-folder> --output local/recovery-test --capture

The collector never operates the game. It expires its lease on exit and sends
at most one explicit request. Compiled-DLL/production-consumer tests pass; actual
FFR verification remains pending. This adds no restore or automatic rollback.

## Detached normalization comparison

The distinct [detached normalization probe](DETACHED_NORMALIZATION.md) imports
the observer's own opening into a separate rooted save object, changes only its
fixed playtime member under profile/field/clock gates, and compares the full
serialization with independently prepared bytes. It preserves current live
progression, performs no live load/reset/abort/recreation/slot write, and shares
the process-wide one-attempt exclusion with ordinary import and abort testing.
It has a separate explicit CLI mode and receipt. Compiled-DLL/production-consumer
tests pass; installation and actual FFR verification remain pending.

# Experimental opening-abort trial

The source now includes a separate, explicitly selected
[current-field encounter recreation experiment](ENCOUNTER_RECREATION.md).
It requires an unchanged verified opening exit and manual activation. Its first
live preparation refused before entry because the original pre-entry position
differed from the post-exit position. A separately bound, stable post-exit anchor
correction is being tested offline; actual recreation remains unverified.
It restores no progression, position, effects or RNG.
The existing abort-only invocation retains its previous behavior.

**Detached import and the revised opening-exit path are live-verified. Battle
restart and restoration remain unfinished.** The earlier
first-menu check refused because the raw counter counts turn beginnings, not
completed commands. Subsequent supervised tests verified fresh idle-menu samples
and separate manual-command/Skill.Begin histories. The revised abort guard
requires both same-thread histories untouched, with no coverage-loss flags,
alongside the ordinary idle-menu checks. It does not accept a raw threshold of
one. The previous source paired native EndBattle with the game's own
`UI_BtlCmdManager::AbortCommand` to release command selection. This native API
pair was exercised once at an untouched normal menu. Both native flags were
verified, but the command menu remained active even after the user refocused
FFR. The trial timed out, sent no retry, and the user closed that launch.

Read-only inspection then located the actual ordinary input task: its step2
has an empty update callback, so that completion flag cannot advance it.
The current candidate instead calls EndBattle, closes the owned command view
model with native EndCommand, and, only if the same Turn task still waits at
the verified step2, evaluates that task's existing native finish checker once.
The checker selects the game's ForceEnd result task and result mode5. No raw
result/step setter or fake action is used. In the supervised revised test, the
calibrated legacy-controller shortcut triggered this sequence once. The native
checker selected result5 and its result task, then released both battle managers
with empty queues after about 1.03 active seconds. The user confirmed field return
without an escape or result screen. All 466,065 progression bytes, including
counters, inventory, EXP/MRP and HP/MP, remained identical. Read-only post-exit
field-anchor and raw cache components also matched; complete effect graphs and
cache coherence remain unverified. All five original save files retained their
pre-install hashes. The controller completed and expired its lease.

The independent detached-import policy was then exercised in FFR. A temporary
object matched all 466,065 captured bytes; live progression and input remained
unchanged and temporary pins were released. The user confirmed responsiveness.
Detached normalization, live-root replacement and battle restart remain disabled
or unimplemented.

This is a separate, mutation-capable development component. It has passed
isolated dummy-process tests and the supervised detached import. The first live
ForceEnd request stayed pending at the command menu and timed out; a subsequent
user command was followed by field return without a result screen. Fresh read-only
diagnostics verified that delayed cleanup. After-progression was not captured,
so reward/resource equality is unverified. No restoration has run in FFR.
The subsequent scoped-wait experiment sent no request: the real first-menu queue
was empty and its required wait condition was absent. The controller was stopped
and its lease expired. The wait-result detours have been removed from the source.
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
  getters, complete EndBattle, command view-model EndCommand/clear, Turn task
  finish checker/prechecks, class/registry ownership and result-task functions.
  A protected result-task step entry is fingerprinted but its complete protected
  implementation is not claimed to be decompiled.
- Uses a separate FFRHelperBattlePractice folder and two distinct channels:
  read-only capture reports and an explicitly mutation-capable abort status.
  The existing capture client cannot send abort requests.
- A request binds the current session, encounter revision, opening hash and
  unique nonce, with an expiry of at most eight seconds.
- The game-thread guard requires a matching current world/map manager, ordinary
  escapable encounter flags, battle UI command phase, selected party unit,
  all captured friendly units present and idle job queues. Both exact-profile
  histories must cover this opening and thread with zero entries, no foreign
  thread and no overflow; a lease gap or unavailable hook prevents authorization.
  Non-default battle-event IDs, bounty/test/preemptive/result-skipping flags and
  disabled escape cause refusal. This does not identify every possible script context.
  Raw turn beginnings are reported separately and never interpreted as commands.
- The command view model must be owned by the root registry under the matching
  derived class key and have the exact native type/open flag. The current Turn
  task must belong to this GameInstance's task manager, have the exact step2,
  callback identities and completed turn-event phase, and return the same World.
  The virtual World method must belong to the game image. The
  battle must be initialized, with no pending ForceEnd/command-completion flag,
  an empty command-request array and no active pause flag. The client requires
  the explicit `ordinary-first-menu-force-end-task` policy. The matching talk
  subsystem must be quiet. No attack or
  player-command record is submitted; no raw game field or wait result is written.
- Preparation serializes progression into a separate recovery buffer. The worker
  commits and reads back a private recovery file before reporting the trial armed.
  The client also checks its hash, GVAS class, engine, footer and required fields.
- Only a matching explicit commit may call the verified EndBattle function.
  After its flag is read back, native EndCommand closes the verified view model.
  Code identity, idle jobs, source/history and the exact retained task/step/VM
  identities are checked again before invoking the finish checker once. If the
  callback changed task/readiness, only observation continues; no later retry
  is queued. The existing finish checker retains its own event prechecks and
  may refuse to select the expected result task; the helper does not override
  them. State is marked before each
  call, and there is no automatic resend or retry. Flag readback is evidence of
  the request only; field cleanup and progression comparison remain separate.
  One attempt per game launch is supported, including rejected attempts.
- Cleanup observation requires the same encounter/session, empty unit/job queues,
  cleared battle-active flag and cleared battle UI flag. Both released managers
  are accepted only when both pointers are null and the inactive context matches.
  A progression snapshot
  is then captured for comparison. The user still needs to confirm actual field
  return. A changed context or unverified cleanup is reported without a retry.
  Cleanup has a 15-second active-game-tick budget, with each delta capped at
  250ms; background wall time cannot consume it. Request expiry remains eight
  wall-clock seconds. The final receipt separately verifies ForceEnd result5.
- Recovery files contain progression only. They cannot resume an ongoing battle;
  this trial has no automatic rollback or full battle-state recovery.
- No normal game save is written. Saving anywhere remains uninstalled/deferred.
- A timed-out ForceEnd may still be pending. Close that test launch before further
  gameplay; do not resend the request or assume the flag was cleared.

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
establish live compatibility by itself. The generated constructor lookup was
separately installed and verified on 29 October; it resolved the scan timeout and
the same save loaded with the loader alone and with the read-only native module.

## Test controller

Start this before entering the familiar ordinary encounter so the opening is
captured in the same session. Use a fresh private output directory.

Probe only:

    python tools/run_abort_trial.py <FFRHelperBattlePractice-folder> --output local/abort-probe --duration 300

One explicitly requested abort at the selected encounter's opening, with a
deliberate shortcut (the default):

    python tools/run_abort_trial.py <FFRHelperBattlePractice-folder> --output local/abort-test --duration 300 --abort-at-opening

With FFR focused, release all shortcut buttons and press **R1 + Square + D-pad
Down** once (XInput **RB + X + D-pad Down**), or **Ctrl + Shift + F8**. The helper
reads those combinations only; it does not inject, suppress or remap game input.
Controller support uses Windows XInput/Steam Input. A legacy WinMM controller can
use a deliberately calibrated binding by adding `--legacy-controller
<local-controller.ffrinput.json>`. Its capability hash, slot, exact two-button
mask and D-pad Down must match; button indices are not guessed. Calibration of
the tested controller succeeded in a read-only supervised check. The new reader
and safe edge behavior pass fixture tests. The calibrated legacy shortcut also
triggered the successful revised exit described above. The keyboard fallback remains available. The exact foreground executable path
must match the game beside the supplied mod folder.

Holding buttons at startup, focus return or an observed controller reconnection
does not trigger a request. A press in a preemptive/surprise encounter, at an
unsupported boundary or without fresh status is discarded. It never queues an
abort for a later encounter. A valid press grants at most two seconds to collect
the matching opening report, bound to that exact source; losing readiness,
foreground or source cancels that intent. Losing foreground after preparation
also prevents commit. Waiting alone performs no abort in this default mode.
The same durable recovery, native guards and one-attempt limit still apply.

For a separately selected automatic diagnostic only, add `--trigger automatic`.
The shortcut currently requests the **abort prerequisite**, not a completed
battle-restart operation. The Windows API initialization was checked locally;
the keyboard shortcut subsequently activated one supervised native pair in FFR.
The tested controller was visible through legacy WinMM and absent from XInput.
Its button/POV combination was calibrated read-only. The calibrated legacy reader
is implemented and recognizes that device; the revised test verified exit
activation through it. Detecting the keyboard press did not establish successful
battle exit; both flags were set while the menu remained active.

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

The experiment requires an ordinary idle command menu and successful UE4SS
engine initialization. Earlier actions do not exclude this detached comparison:
it does not restore the opening or apply the clone. The game thread, world,
active encounter, selected friendly unit, empty queues, opening identity, lease
and request deadline are rechecked during comparison. The client requires a
game-thread sample no more than two seconds old. This is a separate policy,
not a reinterpretation of the turn-beginning counter as completed commands.
If the loader's symbol scan
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
complete status coverage. The graph is fixture-tested but has not been captured
in FFR; the practice module is disabled.

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

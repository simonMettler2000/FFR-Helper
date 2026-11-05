# Native battle restart and save-state research

Target: the user's existing Steam installation, English build **24250053**, UE
**5.6.1**. A virtual-machine backend is outside this work. Development and live
testing of **Saving anywhere** are deferred until the other requested features
are implemented, unless the user changes that instruction. Do not use the
field-save adapter as a prerequisite for this recorder.

## Current status

The offline editor and ordinary-save checkpoints work independently. **Native
battle-opening restart and mid-battle save-state restoration are not implemented.**
The development build adds an optional read-only recorder and a Runtime research
tab. This is a tool for the remaining implementation, not a replacement for it.
Loader/recorder installation is opt-in, with the game closed. The read-only
recorder has now been verified through an overworld–battle–overworld cycle on
the target build. Restoration has not been tested or implemented.

## Resume handoff — 29 October 2026

**5 November offline continuation:** work resumed at the user's request; live
testing is unavailable before 13:00 Europe/Zurich. UE4SS remains disabled and no
installed component, save or gameplay was changed. The development source now
has explicit verified-child rearm and a persistent `--cycles N` manual controller,
with immutable session/generation evidence and separate per-cycle desktop output.
The compiled mock-host DLL completed three cycles; negative cases refused changed
history/progression/task/archive data and a reused nonce. This is uninstalled and
not live verified. The installed one-shot behavior described below is unchanged.
See [the protocol notes](../native/ENCOUNTER_RECREATION.md#repeated-untouched-opening-cycles-offline-development-5-november).
Static UI inspection now ties portrait placement to `GetRelation` and the unit-VM
parameter link. A bounded read-only diagnostic distinguishes current/null/stale
links without invoking a getter; see [the investigation notes](TIMELINE_DIAGNOSTICS.md).
The memory-reader diagnostic has eleven synthetic checks. The native combat
channel now includes optional capture-local timeline links, including a settled
verified child menu. Its copy invokes no new engine API; the UI pointer comes
from the existing guard probe. The controller can retain each child's combat
report with `--capture-child-diagnostics` and refuses another cycle on missing or
stale party links, read failure or expiry. A real compiled-DLL fixture captures
the child and then completes the remaining cycles without another import call.
There is still no recreated **FFR** link/widget sample.

The full Python suite ran 224 tests (223 passed, one skipped). All 128 native
regression cases passed, followed by seven focused checks after final hash-bucket
validation. A [partial backup reader](NATIVE_BACKUP.md) also preserves duplicate
unit rows and compares retained resources without applying them; seven synthetic
checks pass. The two formerly unknown backup floats are depth-of-field settings,
not RNG or clock. Timeline correctness, post-action rollback and full save states
remain open. All prepared changes remain uninstalled.

At that handoff, work was paused for the user's shutdown. Resume
battle-restart development when the user returns; do not start gameplay or a
mutation controller merely because the helper is installed. Read the private
`local/practice-next-test-plan.json` first for exact installed hashes, evidence
paths and remaining test boundaries. The source installed for the last test is
`2c116d7f8030eba892830851bd35504b7be9925b`; the installed files remain unchanged.
The private draft remains PR #1 on `codex/native-runtime-state`.

**A same-setup encounter was recreated successfully once.** The supervised
keyboard-triggered trial exited an untouched normal opening, verified identical
466,065-byte progression, invoked one consuming map entry, independently captured
the matching child, and verified its fresh command menu and unchanged current
progression/root. The user confirmed the encounter restarted and its menu
responded. All five original save hashes matched their backups at the test
boundary. This establishes current-field recreation, not post-action rollback.

Two issues remain immediately actionable:

- The recreated fight placed party portraits below the timeline alongside enemy
  portraits. A naturally entered **normal** encounter was then inspected and
  showed party portraits above and enemies below. An earlier preemptive sample
  was insufficient by itself; the normal sample now confirms the discrepancy.
  Its effect on combat behavior is unknown. Trace team-side/icon view-model
  state and battle-end/start cleanup before calling this merely cosmetic.
- The test controller deliberately completed and disconnected after one verified
  recreation; its native binding is also one-shot. The user's later controller
  and keyboard presses therefore did nothing. Repeatable manual restart is not
  implemented. Add an explicitly verified new transaction per deliberate press;
  never reset an uncertain operation or replay an old request.

The read-only comparison recorder finished with 367 private reports, disconnected
and expired its lease. No helper mutation controller is armed. The game process
was absent at the shutdown handoff check. Keep the approved installed components
unchanged until a tested update is ready and the game is confirmed closed.

Next work: investigate the timeline discrepancy; implement and test verified
transaction rearming and repeated manual activation; then implement rollback
after commands/items/resources change. Existing opening-only guards must remain
honest: do not report acted history as untouched. Boss/scripted support and full
mid-battle states still require separate work. Full cache/effect/clock/RNG restore
is unfinished. The normal-load external-effects gate remains closed; the native
gameover retry route consumes an item and must not be invoked as a free retry.
Saving anywhere stays last and uninstalled. Revisit `REDISTRIBUTION.md` before
bundling UE4SS or shipping automatic install/uninstall support.

Local validation remains 112 passing native cases and 185 Python tests run
(184 passed, one existing skip). No code changed during this handoff. CI runners
remain blocked by the account billing/spending limit. Private captures, saves,
process pointers and game data must stay outside Git.

## Installation review and requested status indicator — 30 October 2026

**Latest state:** later on 30 October, the user requested normal story play with
UE4SS disabled. With the game closed, the verified proxy was renamed from
`dwmapi.dll` to `dwmapi.dll.ffr-helper-disabled`; its hash is unchanged. UE4SS and
all mod files/markers remain preserved but cannot load through that disabled
proxy. Saves were untouched. Before testing resumes, inspect the private local
plan's `currentLoaderState`; do not silently re-enable the loader. The following
review describes the installation before that reversible disable action.

Read-only inspection confirmed that the approved loader and practice DLL remain
installed with the same hashes as the last supervised test. UE4SS is enabled via
the proxy loader; `FFRHelperBattlePractice : 1` and its enabled marker are present.
The Lua recorder and separate read-only native recorder are disabled and retained.
The game was closed, no helper controller/recorder process was running, and all
installed modules' observation leases and abort requests were expired. No installed
file was changed during this review. Enabled/loading is distinct from an armed
mutation request: the current hooks still load with the game, but a controller's
fresh lease and separately guarded explicit request are needed for an experiment.

Ordinary story progression does not itself invalidate the mod's exact-build
bindings. Future testing needs fresh backups/opening captures from that progress;
old test snapshots are historical evidence. Experimental loaded hooks still carry
compatibility/crash risk even while unarmed. Prefer disabling the loader before
unmodded story play; stopping the desktop helper alone does not unload UE4SS.
No claim of guaranteed safe normal play or complete uninstallation is made.

Requested for a future implementation: small green **FFR Helper Active** text in
the game's rendered viewport, above the command-menu area during battle and in a
consistent unobtrusive position outside battle. Keep it visible while the runtime
module is initialized even if the desktop app disconnects; distinguish loaded,
ready, connected and armed states rather than implying every unfinished feature
works. A failed/unsupported initialization must show an honest warning state.
The standard build should not offer a hide toggle if this is intended as visible
practice disclosure. Rendering must not capture input or change gameplay.

Text rendering is substantially smaller work than restoration. Integrating the
widget with this packaged game's viewport, safe object lifetime, resolution/UI
scaling, map/battle/menu transitions and actual capture output still needs an
implementation and supervised verification. Unreal's
[viewport widget support](https://dev.epicgames.com/documentation/unreal-engine/BlueprintAPI/UserInterface/Viewport/AddtoViewport)
provides a candidate mechanism; no FFR-specific overlay binding is implemented.
Do not introduce a raw graphics hook solely to avoid that lifecycle audit.

This is a disclosure watermark, **not anti-cheat attestation**: a modified build
can remove it, a recording can hide it, and edited saves can later be used without
the module. Its absence cannot prove an unmodified run or clean save. Keep this
limitation explicit in the UI documentation and any speedrunning claims.
Battle-restart development remains paused while the user asks these questions.

## Current command-menu exit candidate

The latest supervised native EndBattle + UI-manager AbortCommand pair set both
flags but did not leave the untouched command menu. No automatic retry occurred.
Read-only inspection found the actual Turn task at input step2 with an empty
update callback. A separate natural-flee trace recorded normal task progression
and field return with zero read failures; that escape used result2, not the
ForceEnd result task, and does not verify the helper exit.

The new practice-only candidate requests EndBattle, closes the owned command
view model through native EndCommand, and evaluates the existing finish checker
once only while the identical Turn task/step and idle source/history still match.
It requires completed turn-event processing, quiet talk, empty requests/jobs and
exact loaded code/ownership checks. No raw step/result setter or fake command is
used. It records native result5 and result-task selection separately from field
cleanup and progression comparison. A changed callback context never schedules
a later mutation. The cleanup watchdog uses capped active game-tick delta;
request expiry still uses wall time.

Compiled dummy-process cases exercise the real candidate DLL, including derived
registry ownership, prepared VM/task swaps, wrong World/step, incomplete turn
events, busy talk, queued work, callback task/job/code drift, failed UI closure,
checker refusal/wrong result, once-only game-thread calls and released managers.
All89 native CTests passed for that installed build; its170-test Python suite passed with one existing
skip. The revised candidate was then installed with explicit approval and five
verified save backups. In a normal untouched opening, its entire task/VM/code
guard passed. The calibrated legacy-controller shortcut triggered one commit;
native EndBattle, command-view-model closure and the same task's finish checker
all succeeded. The expected result5/task were observed, then both battle managers
were released with empty queues after about 1.03 active seconds. The user
confirmed actual field return without a reward or escape screen.

The complete before/after 466,065-byte progression objects matched, including
HP/MP, EXP/MRP, inventory and counters. Both command/skill histories remained
untouched. A separate query/read-only post-exit check matched the captured field
position/direction and raw map-unit cache components. Complete effect graphs
and overall cache coherence remain unverified. All five original save files
retain their pre-install hashes. The controller completed and expired its lease.
This establishes the tested opening-exit prerequisite. Full battle restart and
progression rebinding remain unfinished. Same-setup recreation subsequently
succeeded in the separate experiment below. Saving anywhere remains deferred and
uninstalled.

## Current-field encounter recreation

The separate
[current-field recreation experiment](../native/ENCOUNTER_RECREATION.md).
Its practice-only Windows adapter verifies the complete consuming entry,
allocator/copy/destructor profile and the observer's actual hook/trampoline.
A distinct prepare/commit transaction requires the original untouched exit,
durable unchanged progression, same live root and partial field/cache evidence.
It transfers owned engine input once and independently verifies a fresh child
opening/menu, including current progression at that menu. The controller retains
both receipts and requires foreground manual activation. Nonzero current field
effects are left to normal engine entry; no effect/cache setter is involved.
The older restoration core's refusal of unsupported effects remains intact.
The first installed test loaded normally and verified another unchanged exit,
then refused recreation preparation before any entry call. Raw cache, map and
direction matched; position differed by about 47 native units. The user confirmed
released inputs and no movement. All five original save hashes stayed unchanged.
That controller stopped without an entry call; this describes an earlier failed
preparation, not the later successful pose-corrected trial.

The subsequently installed correction retains the original pre-entry anchor, verifies a
separate post-exit anchor over three game ticks, then requires that exact target
through commit/allocation/entry and in the independent child capture. It keeps
the original map and all existing progression/cache/ownership checks. Both
position and direction belong to the separate stable post-exit sample: the
position-only correction refused before entry because direction also differed.
That second exit again preserved all progression bytes and all five original
save hashes. Native preflight failures now identify the exact failed condition.
Receipts report original-anchor equality separately and explicitly deny position
restoration. This remains a prerequisite for full retry; post-action rollback
and RNG/effect restoration are still missing.

Offline validation for the position/direction correction: all 112 native tests
passed, including an exit-pose shift and post-preparation drift refusal with
specific error messages. The 185-test Python suite passes with one existing skip.
These checks use isolated fixtures. A later approved supervised trial then
verified actual entry, the matching independent child and its first menu, with
user confirmation. The timeline discrepancy and one-use limit are recorded in
the resume handoff above; full battle restart remains incomplete.

## Live recorder findings

The loader registered all three requested hooks. However, none of those
reflected notifications fired during the tested ordinary encounter. They cannot
serve as the sole capture trigger. Direct native calls and Blueprint overrides
are possible bypass paths; hook registration alone does not prove coverage.

Polling detected the actual battle, read all 12 runtime units in that encounter,
and captured HP, MP, stagger, levels, AI plans and counters. A user-issued command
produced corresponding HP/stagger/counter changes. Dead enemies remained in the
unit collection through the end of that encounter. Working/waiting job counts
varied during actions and reached zero at the sampled command menus.

On returning to exploration, the battle flag cleared, the acting-unit focus
reset, and the battle-unit collection emptied. This verifies observation of the
tested lifecycle; it does not establish a safe restore boundary or complete
state coverage. Empty job queues alone are not a sufficient restore condition.

The first collection run exposed a Windows file-sharing race between the Lua
reader and an atomic desktop lease replacement. Bounded retries preserve the
complete destination rather than truncating it. Regression tests cover transient
and persistent locks; the resumed live collection stayed connected through the
tested battle. The full-host running-game guard also correctly detected the game.
Restricted AppContainer process lists cannot establish that the host game is
closed; live-slot writes now fail closed in that context. The normal profile
save directory is also considered when LOCALAPPDATA is redirected.

All raw reports, save backups and user-specific values stay outside Git. No game
progression/resource setters, disk save/load calls or restore operations were invoked.

## Native capture prototype after the live test

An [experimental C++ entry observer](../native/README.md) now builds independently
of the Lua recorder. It uses exact binary/prologue fingerprints and a single
native detour to copy encounter setup before forwarding the original call. It
has passed an isolated executable/DLL harness, including mismatch refusal and
lease expiry. It has now been installed with explicit approval and verified
across three live ordinary encounters. It does not implement restoration.
It serializes the current progression object in memory
at the same entry point, frees the game's temporary allocation and publishes a
bounded, hashed GVAS sidecar. The desktop/CLI can validate and export it as a
non-restorable `.ffropening` archive. Broader state coverage and RNG remain
outstanding alongside the reconstruction/restore path.

The native fixture verifies that serialization completes before the original
encounter call, the original still executes once, and engine-owned output is
freed on success/failure. Python tests retain EXP, MRP, HP/MP, inventory and unknown
fields through archive export/import. These tests establish the capture/storage
mechanism. The subsequent live test established the following observed behavior:

- All three encounter entries produced independently verified GVAS archives with
  no encounter-copy or progression-serialization failures.
- All current party members were present, with saved HP/MP, EXP, inventory and
  per-character vision MRP values. Every payload passed a byte-identical codec
  round trip, in addition to its reported size/hash/build checks.
- Executing a command left the first opening's bytes and metadata unchanged.
- Fleeing, then entering another encounter, produced a new opening with an
  incremented escape counter, changed resources and unchanged EXP/MRP.
- Winning the next battle and collecting rewards produced a third opening with
  increased party EXP, positive vision MRP changes and an incremented win counter.
  Both earlier opening archives retained their original hashes.
- The native recorder produced 350 private reports. Both collection leases were
  explicitly expired after the test; no abort or restoration was invoked.

The auxiliary Lua recorder did not initialize on this launch: UE4SS's engine
pattern scan timed out locating `StaticConstructObject_Internal`. It produced no
fresh reports. The native observer initialized independently and remained active,
so this run does not provide a simultaneous Lua comparison of battle-only fields.
That loader-initialization issue was addressed in the separate 29 October test
below. No signature override or additional hook was installed during the earlier
three-encounter test.

## Loader recovery and updated capture test, 29 October 2026

A later launch repeatedly failed to resolve `StaticConstructObject_Internal` and
ended with a fatal pattern-scan timeout after 201 attempts. The user reported a
loading loop. With the added proxy loader temporarily renamed, the same save
loaded normally. After exact executable/function fingerprint checks, the
prepared custom signature was installed using UE4SS's documented lookup API.
The log then resolved the constructor at the expected RVA, completed engine
initialization and entered its event loop. The same save also loaded with UE4SS
alone and both helper modules disabled.

With the game closed and current saves backed up, the user approved installing
the read-only native observer from source revision `85e888f`. The Lua recorder
remained disabled; the practice component and Saving anywhere remained absent.
The user again reached exploration normally. The first ordinary encounter
produced a validated opening with partial field/clock/cache/CRT/engine-global
metadata and zero capture failure counters. After one user-issued command, its
progression bytes and every metadata field remained unchanged. Cached HP-base
and MP values matched corresponding saved fields in that opening; this is a
component-level observation, not proof of complete cache coherence.

This verifies the constructor lookup and the tested capture transport, not
restoration or complete RNG/world coverage. Independent displayed-clock,
position, concurrency and broader lifecycle checks remain outstanding. The
private records are under `local/load-loop-20261029-1146/`; they contain no
helper-triggered save writes, abort, live-root assignment or restore command.
The reward/next-encounter check produced a second validated opening containing
positive EXP and vision-MRP changes. Both captures retained every optional
component and all capture failure counters stayed zero. The CRT state changed
between these openings; the engine-global seed did not. This does not establish
which gameplay actions use the latter generator.

The separate practice module was then installed with explicit import-only
approval. The first preemptive encounter was excluded. A normal first-command
menu also reported a party counter of one before any user command, so the client
refused without writing an import/abort request. Static instruction analysis
located the increment immediately after `Delegate.Battle.Unit.Turn.Begin`:
this counter cannot be treated as a count of completed user commands. No engine
import, abort, normalization or live-state restore was performed. Fresh idle-menu
sampling also remains to be verified; the subsequent read-only frame request
timed out rather than accepting a stale boundary.

The practice component was disabled with the game closed and the tested
read-only native setup restored. All observation leases were expired. The FFR
source profile at that checkpoint set `opening_boundary_reviewed=false` and refused practice
tick-hook installation, including import/abort/normalization dispatch, until the
boundary is corrected and reviewed. Controlled fixture profiles alone retain
the test path. Do not substitute a threshold of one or waive freshness to make
this test pass. The read-only entry observer is independent of that gate.

### Decision-handler history prepared offline

Further inspection located the native target of the reflected
`CPP_BtlDelEventCmd::OnExecuteDecideCommand` handler. A new read-only detour counts
entries into that handler, before forwarding the original arguments/call exactly
once. The complete chained-unwind function envelope is fingerprinted after the
game identity check. No decision data, object pointer or gameplay value is copied
or changed. Null/rejected handler invocations also count; these are deliberately
called **handler entries**, not completed commands.

The journal starts before serialization at native encounter entry and is bound
to that opening sequence and native thread. Its bounded counter is separate from
the immutable opening. Calls from another thread set an explicit coverage flag;
overflow is flagged rather than wrapped. A lease gap, new session, failed entry
or reconnect without a new encounter leaves history unavailable. A mismatched
handler fingerprint disables only this optional journal, preserving progression
capture. Shutdown disables the detour and retains its pinned trampoline.

`observer-status.json` exposes this optional `decisionHistory`. The native
collector validates and saves changes as private `.ffrdecision.json` evidence.
Its timestamp is explicitly the observer worker's publication time, **not a
fresh game-thread sample**. Every record declares coverage, command-boundary and
game-thread freshness verification false. Zero entries cannot authorize import,
abort, retry or restoration. The supported practice gate remains false.

Compiled-DLL fixtures verify original arguments/Win32 error/once-only forwarding,
zero then one handler entry, foreign-thread flags, unchanged source progression,
lease-gap/new-session invalidation, new-encounter reset, code restoration and
optional profile mismatch. This update is prepared locally, **not installed or
verified in FFR**. The next supervised test should establish whether the handler
stays at zero through the first menu and menu cancellation, then advances after
a user command, while separately verifying the menu's fresh game-thread state.
Alternative native/Blueprint/automatic action paths still need coverage review.

### Independent read-only menu-frame sampling

The ordinary recorder now has a separate, optional manager-tick sampler. It is
excluded from the practice build, so it cannot share or enable the experimental
dispatch hook. The full tick envelope and read-only getter fingerprints are
checked before installation. Sampling runs after the original tick at most ten
times per second, on the encounter's original thread and matching world/map.
It records bounded UI/selection/unit/job state and labels the raw counter
`turnBeginnings`. No abort, import, serializer, setter or root assignment is
invoked by the sampler.

The optional `menuObservation` has its own game-thread `sampleTime`, separate
from the worker's status-publication time. Its strict consumer can report a
sample as recent, while coverage remains explicitly unreviewed and non-atomic.
A recent menu sample is not permission to retry. Invalid context/layout or a
profile mismatch leaves this component null without suppressing opening capture;
new encounters, sessions and lease gaps invalidate the cached frame.

The native collector saves private `.ffrmenu.json` records. Compiled-DLL tests
verify a first-menu frame with one turn beginning but zero decision-handler
entries, a later decision, and unchanged source progression. They stop the fake
game ticks while keeping the observer worker alive: the frame becomes stale
despite fresh status heartbeats, then becomes recent after ticks resume. Other
checks cover malformed jobs, optional profile mismatch, unchanged call arguments
and Win32 error behavior, and restored code at shutdown. This source passes the
full 63-case native suite and 143 Python tests. Actual FFR menu sampling and
command-handler coverage still need supervised verification; practice remains
disabled.

### Supervised menu/history test and independent import policy

The installed source31d read-only recorder completed three opening captures with
zero capture failures. Normal first-menu and cancelled-submenu samples kept the
decision-handler count at zero; one user command advanced it to one. The next
encounter reset history to zero. Opening metadata and progression bytes stayed
unchanged through menu cancellation and commands. Fresh game-thread timestamps
were distinguishable from stale frames while the observer worker kept publishing.

A surprise encounter included automatic attacks and party counters while the
manual decision-handler count stayed zero. It was correctly excluded as an
ordinary encounter. Zero handler entries are therefore not proof of an untouched
battle. The next source adds an optional exact-callsite `Skill.Begin` event
counter, with bounded/session/thread flags and no event payload reads. Its live
coverage remains unverified.

In the overworld, a read-only host-memory diagnostic found both owned battle
managers released (null), inactive battle/UI flags and the exact manager class.
The updated sampler and practice guard accept this specific inactive case while
refusing a single missing manager or active flags. The field tick behavior still
requires live verification; the diagnostic alone does not prove tick coverage.

Detached import now has an independent `ordinary-idle-menu` policy. This permits
comparison after earlier turns because the clone is never applied. It rechecks
the actual game thread, world, selected friendly unit, empty queues, current
opening/session, deadline and initialized loader throughout the operation.
The client retains its two-second game-thread freshness requirement. Opening
abort and normalization remain disabled; their review flags are separate. The
normal-load adapter remains excluded and live progression is never rebound.

The updated source passes 148 Python tests and all 68 native cases, including
post-action detached import, import-only refusal of abort/normalization, released
manager handling, and optional skill-hook mismatch/session/thread behavior.
These are isolated tests. No engine import, abort or live-state restoration has
yet run in FFR. Supervised detached import is the next implementation milestone.

### Detached import verified in FFR; controlled abort prepared

The supervised source0e6678b test verified Skill.Begin zero at a normal first menu
and one after one user command, alongside the manual-handler count. On returning
to the overworld, actual ticks reported matching thread/world, inactive battle/UI,
both owned managers released and zero units/jobs. Two earlier preemptive encounters
were excluded. These observations verify these tested paths, not complete action
or state coverage.

The read-only collector was stopped before starting a fresh one-shot import
session. At the next ordinary first menu, native LoadGameFromMemory created a
separate object which serialized to exactly the opening's 466,065 bytes. Input
and live progression were unchanged; all temporary pins were verified released.
The bound receipt passed and the user confirmed a responsive game. No abort,
normalization, live-root assignment, save-slot write or restoration was performed.

The next source enables the controlled opening-abort trial with a new explicit
`ordinary-first-menu-event-history` policy. Both same-opening/thread histories
must be available and untouched, without foreign-thread or overflow flags.
Fresh native idle-menu/context/queue checks remain required at prepare and commit.
Raw turn beginnings are only reported; a value of one is not used as a substitute
for completed actions. The entire 112-byte EndBattle envelope is fingerprinted,
including the flag write beyond its old prefix check. Normalization stays disabled.
This is a prepared experiment, not a verified abort or a battle restart.

Validation now passes 151 Python tests and all 72 native cases. The added native
cases refuse manual/skill/foreign-thread activity and an incorrect complete
EndBattle fingerprint, and accept inactive teardown with both managers released.
The CLI lifecycle test skips preemptive/setup frames before exactly one durable
prepare/commit. A full-run fixture receipt-copy race was corrected by using
Windows delete-sharing reads and bounded nonempty terminal readback; the fresh
complete run passed. At that checkpoint, no FFR ForceEnd call had been made.

### First ForceEnd test: deferred behind command input

With explicit installation/one-call approval, source18df14a loaded normally and
sent one ForceEnd request after durable recovery at a normal first command menu.
The command menu stayed active until the trial's 15-second cleanup timeout.
The user then reported a self-attack followed by a fade to the overworld, with
neither escape nor battle results. A separate read-only continuation of that
terminal trial's owned session verified fresh matching thread/world, inactive
battle flags, both managers released and zero units/jobs. It sent no command.
The terminal result was not relabelled as a successful immediate abort. No
after-progression snapshot was captured, so rewards/resources remain unverified.

Static tracing shows that the pending-command check returns before the ForceEnd
flag is processed. The follow-up source adds a practice-only, exact-callsite wait
gate: it observes the original result, then may return false at each of two
verified callsites at most once during a committed own abort, on the captured
thread with a fresh, revalidated context from the actual manager tick, with
the matching delegate/ForceEnd flag, ordinary idle menu, empty queues and untouched
histories. The complete gate, enclosing state and earlier pending-command helper
are fingerprinted, and both direct call targets are verified. Callback nesting
inside manager Tick is not assumed. Other callers/results are forwarded unchanged.
Preparation now additionally requires a fresh observation of that wait gate.
No player command record, UI value, queue, skill, resource or live root is written.
The follow-up has not been installed or live-verified. On timeout, ForceEnd may
remain pending; end the test launch before further gameplay rather than retrying.

The corrected source passes 151 Python tests and all 74 native cases. Its dummy
state path reproduces the two early wait checks: exactly one ForceEnd and two
scoped wait results reach cleanup with zero manual-command or skill events.
Unrelated calls preserve their result/error behavior. Wrong delegate ownership
leaves the wait and pending flag unchanged; profile mismatch disables abort.
Released-manager cleanup, durable recovery, expiry and exactly-once behavior are
also checked. Preparation validates the wait owner and bounded request array
before queuing ForceEnd. These are isolated results; corrected live verification
is still pending.

The CLI now retains a success or terminal diagnostic receipt, compares all
progression bytes as well as named resources/rewards, and records whether the
wait override and untouched history were observed. None of these receipts claims
restoration or reconstruction.

### Live wait guard held; native command-abort API identified

Source8341b19 loaded normally, but its fresh wait guard stayed false at the
untouched normal menu. No prepare, commit or ForceEnd request was sent. The
verified controller launcher/worker pair was stopped and the owned observation
lease expired. A bounded host-memory diagnostic used query/read-only rights,
with no game function calls or writes. It found an initialized active battle,
empty command-request array, zero jobs and no pending ForceEnd/completion flags.
The scoped wait checks were therefore not the supported boundary for this menu.

Static reflection/exec tracing identified the game's own
`UI_BtlCmdManager::AbortCommand` at RVA 0x5136860. Its complete 69-byte body resolves
the current World/battle and sets the separate command-completion flag. Both
native input-wait states watch that flag; the real decision handler also sets it
after input. The game's 43-byte command-manager getter returns the owned UI
object at UI subsystem+0x70, and the native VM cancellation path calls this API.
The generated function index is a discovery aid; its name/getter associations
must be checked against actual reflection registration/exec targets.

The next source removes the wait-result detours. It requires an untouched normal
idle menu, the observed initialized/unpaused flags, no existing end/completion
request, an empty command queue, owned UI manager and matching World through the
same virtual getter used by the native API. Only executable code in the verified
game image is accepted for that virtual getter. Complete API/getter/state-reader
fingerprints gate availability and are checked again at commit. EndBattle runs
once; only a read-back end flag permits one native AbortCommand call. Both calls
are marked before invocation, and their flags are recorded separately from actual
cleanup. No attack, command record, raw flag setter, wait override, root assignment
or slot write is exposed. This API pair is not yet installed or live-verified.

The native API source passes 151 Python tests and all 76 native cases. The dummy
state holds ForceEnd until command selection is released, then verifies exactly
one EndBattle and one native command-abort call on the captured thread, both flag
readbacks, zero manual/skill events and ordinary/released-manager cleanup.
Wrong-World ownership after preparation, queued commands, existing completion
flags, event-history changes, profile mismatch and replay are refused before
additional game calls. The virtual World getter is exercised through an actual
dummy vtable and validated executable-image callback. These remain isolated
tests; actual API-pair cleanup and progression equality require the next live test.

## In-memory progression and teardown findings

On this exact binary, the ordinary encrypted save writer calls the engine's
in-memory GVAS serializer at RVA `0x3cba5d0`, passing the save object and an empty
16-byte TArray. It releases the returned buffer through RVA `0x12331f0` on both
success and failure. The save-object getter at RVA `0x51173d0` traverses the game's
live world/subsystem chain and returns the save manager's `m_pGameSaveData`.
The prototype checks exact loaded code fingerprints for all three functions.
It calls only this in-memory serialization path, without the encrypted slot writer,
date/playtime updater, SaveData.Update delegate or disk-save function.

Further read-only tracing shows that `EndBattle`'s manager flag is consumed by a
battle-state path which selects `eBtlResultType::ForceEnd` (value 5), rather than
Win/Lose/Escape. One result handler bypasses the ordinary party-resource writeback
and win/escape counter branches for ForceEnd. This is a promising abort path, but
does not establish that every transition, event, achievement, autosave or reward
side effect is bypassed. No ForceEnd operation has been invoked by the helper.
The full state-machine teardown and reconstruction remain unverified.

The native memory loader was located separately at RVA 0x3caf450, and normal
loading additionally replaces the save manager object pointer and invokes game
delegates. Merely deserializing an object does not rebind live managers, stop
queued combat or reconstruct field actors. The prototype does not expose that
loader or pointer replacement as a restore command.

## Restart implementation after capture verification

A later supervised test used a deliberate foreground keyboard shortcut rather
than automatic activation. The helper accepted Ctrl + Shift + F8, committed the
durable recovery and invoked EndBattle plus native UI AbortCommand exactly once.
Both pending flags read back as set. The ordinary command menu remained active
after the user refocused the game, and the trial became terminal without an
after-progression snapshot. The user closed the launch; no second call, restore
or encounter recreation occurred. The flags alone do not release the actual
active command state. Backgrounding also stopped game-thread samples while the
worker continued publishing, exposing the need for a cleanup watchdog based on
active game time rather than paused wall time. Prepare/commit expiry must remain
short and bound to wall time. The requested controller chord was not detected
because that device exposed legacy WinMM input with no XInput connection;
keyboard detection was verified, legacy controller support remains unfinished.

The portable reconstruction core now rebuilds every captured encounter field,
including nested passives and UTF-16 strings, using supplied engine allocation
and release functions. It never passes captured vector pointers into the game.
Failure-injection tests cover each allocation boundary, detached copying,
Unicode validation, ownership transfer and exactly-once consuming destruction.
The verified engine allocator entry is RVA 0x12406c0 with size/alignment arguments;
integration with actual native encounter recreation remains pending.

Following the encounter consumer's on-disk jump resolved the input-ownership
question for this executable. The map-entry path forwards the descriptor to a
routine that deep-copies its four integer arrays, nested passive arrays, BGM and
scalar fields into the battle manager. On return, the map entry destroys the
original BGM, each nested passive allocation, the passive container and all four
integer arrays through the same engine release function. The downstream battle
consumer alone does not perform that terminal input cleanup. A reconstruction
adapter must therefore transfer allocations once to the verified map entry,
validate the complete entry/copy/destructor/allocator chain, and never free
transferred buffers after an uncertain invocation. Full function envelopes and
the forwarding target were fingerprinted privately. This is a static ownership
review; actual encounter recreation and repeated retries remain untested.

An unbound `EncounterReentry` controller now prepares a one-shot, source-preserving
recreation experiment. It retains its own source, requires the same progression
root and byte-identical GVAS, exact captured field position/direction and matching
captured party-cache components, and refuses nonzero cached effects because the
cache capture does not include their complete state. It checks two current
snapshots around engine-buffer allocation and rechecks the field and complete
consuming-entry profile before invocation. Refusal and uncertain native outcomes
are terminal; a potentially consumed descriptor is never helper-freed or retried.
The consuming input lives through the native call, while the retained source
remains unchanged. Allocation failures, progression/resource/cache/context/code
drift, reentrancy and consumption/exception lifetime paths pass focused portable
tests. **This controller is disconnected from both game DLLs.** It does not
restore changed progression, reset RNG or establish complete cache/world
coherence. Its returned state only means the supplied native callback returned;
it never claims a verified new encounter or functioning battle restart.

The separately compiled [abort trial](../native/ABORT_TRIAL.md) adds a profiled
battle-tick dispatcher and one EndBattle request, with session/revision/hash/nonce
binding, expiry, a durable progression-only recovery gate and a second explicit
commit. It checks a command-menu boundary, a raw turn-begin counter, queued jobs
and unsupported encounter flags. The supported FFR profile now disables dispatch
because that counter does not establish completion of user commands. Cleanup observation includes
the matching context and cleared battle UI/unit/job state. Isolated DLL fixtures
verify the complete file/dispatcher path and exactly-once behavior, including
rejection after a prior synthetic action. The live import-only guard refused
before any request; no FFR abort or import operation ran.

Profiled layout evidence includes the manager's secondary tick interface at
offset 0x30, active flag at 0x1a9, unit/job managers at 0x48/0x40, unit parameter
pointer at 0x838, unit ID at 0xd0 and turn-begin counter at 0x4dc. The UI phase is at
0x50; the generated bDuringBattle setter writes the byte at 0x80. These reads
are constrained to the exact binary and game thread. Empty queues alone are
not accepted as sufficient evidence of a command menu or field return.

There is still no live progression rebinding, complete abort rollback, battle
recreation or mid-battle restoration. The trial's recovery file represents
progression, not the running battle. The ordinary observer excludes this command
path, and packaging checks refuse to label the trial DLL as read-only.

## Detached import implementation

The separate practice build now exposes an explicit import-only comparison,
mutually exclusive with the abort trial. The command names only an observed
session/revision/hash; it cannot supply file paths, arbitrary bytes, addresses or
replacement values. It uses the observer's retained engine-produced opening.
The native type gate validates the GVAS header/class/footer before calling the
profiled memory loader. The desktop also validates the complete opening object.

It keeps the current progression object assigned, creates a separate save UObject
through LoadGameFromMemory, retains both objects with the exact loader's rooting
API, then compares the clone serialization with the captured opening. Success
also requires the original live pointer and serialized bytes to remain unchanged,
the borrowed input to remain unchanged, and temporary roots to be released.
Roots already owned by the engine are preserved. No live-root assignment, load
delegate, EndBattle, encounter reconstruction or disk-slot write is invoked.

This probe refuses an uninitialized UE4SS engine, including the symbol-scan
failure observed on the earlier native capture launch. Export names/RVAs and full
module hashes bind its primitive rooting calls; no SDK containers cross the DLL
boundary. One attempt is supported per process, with no automatic command retry.
Isolated real-DLL fixtures cover success, loader refusal, mismatched clones,
changed live bytes and prior synthetic actions. Live import has not run; the
supported FFR practice dispatch is disabled pending boundary/freshness review.

## Remaining restore-state evidence gaps

The three live opening GVAS captures contained identical saved map coordinates
despite separate encounters. The root's map data therefore cannot be assumed to
describe the current field position. A full field restore needs independently
captured live location/orientation, map/streaming context and world-state coverage.
The reflected MapManager encounter-location field alone has not been established
as a complete field snapshot or safe restore target.

The development observer now adds an optional partial field anchor at native
entry. It checks that the encounter context resolves back to the current map
manager, calls the exact-profile player/map-ID getters, and copies the player's
root-component world position and map-operation direction. Those reads match the
native map-save preparation path; the direction enum includes None and eight
compass directions. The probe does not call save preparation, setters or the slot
writer. Null objects, unreadable pointers and invalid/non-finite values leave the
anchor unavailable while preserving encounter/progression capture.

The serialized anchor is explicitly position/direction-only and non-restorable.
It contains no object pointers and round-trips in new `.ffropening` archives;
older archives without an anchor still load. Python rejects invalid coordinates,
unknown anchor fields and claims of complete state. Portable tests and the actual
observer DLL in a dummy process verify deep copying, refusal and transport.
This new anchor has not been verified against a live FFR player position.

Normal native loading assigns the deserialized root and broadcasts SaveData.Update,
as well as load-start/end notifications and additional cache setup. A direct
pointer swap would bypass those consumers. Tracing these handlers, their derived
unit/quest/world caches and rollback behavior remains necessary before rebinding.
Old/new UObject lifetime must stay valid throughout any future transaction.
The detached import probe establishes a testable prerequisite, not restoration.

## Loader compatibility profile

The function missed by UE4SS's scan was independently located at RVA 0x15bce60
through native object-construction call sites. A preparer now verifies the full
game hash, x64 PE layout, executable section, function fingerprint and unique
64-byte match before generating the supported UE4SS symbol-lookup override.
The function bytes are extracted from the user's own executable and are not
distributed in the repository. The generated lookup has no game calls or writes.
It was installed and verified in the separate 29 October loader-only and
read-only recorder launches described above.

The generator follows the official UE4SS Register/OnMatchFound interface:
[compatibility guide](https://github.com/UE4SS/UE4SS/blob/master/Guides/FixingBrokenAOBs.md).

## Verified static findings

The installed executable's generated reflection describes `SaveData_Battle` as a
4-byte structure containing only `gameSpeedStage`. The ordinary save therefore
cannot represent an ongoing battle. The encrypted save codec preserves the full
ordinary save; this does not make the checkpoint a running-game save state.

`CPP_BFL_ColosseumRankBattle::ExecuteBattleFromSaveData` uses the saved
colosseum fields. It must not be treated as a general restart API.

`CPP_DBG_BFL_Battle::SetBattleLoopTest` reaches a return-only implementation in
this shipping build. A reflected debug function name does not imply working code.

`CPP_BFL_Battle::EndBattle` requests a battle end by setting a manager flag. It
does not itself restore party resources, undo rewards, reset AI, or reconstruct
an encounter. Calling it alone cannot implement restart.

`CPP_BFL_SaveData::ApplySaveGameData` updates saved date/playtime and writes save
data. It is **not a snapshot import function** and must not be called as one.

The map manager's `Encount` path passes `BATTLE_ENTRY_DATA` into the encounter
setup flow. This structure includes party/enemy IDs and placements, passives,
escape rules, stage/group/event IDs, preemption and other encounter flags.
Capturing only an enemy group ID would lose encounter setup.

Reflected battle objects expose only part of their native state. For example,
the timeline scheduler reflects its unit-manager pointer and additional timeline,
but not a complete serialization of its scheduler. The battle job manager holds
waiting/working object queues; recreating their counts cannot recreate those jobs.

These findings came from read-only inspection. Function interception and runtime
object availability still need live verification; direct C++ calls may bypass a
reflected UFunction hook.

## Recorder boundaries

### Detached native combat-component core

A portable read-only capture core now copies verified native combat components
into owned values. The separate practice build exposes it through an explicit
read-only request channel on the profiled game-thread dispatcher. No successful
live command-state capture has been obtained. The installed practice module is
disabled and the source profile blocks dispatch pending boundary/freshness review.
The dispatcher requires the matching
world and actual ordinary player-command boundary; empty queues and an active
flag alone are insufficient. Unlike the abort experiment, observation supports
command menus after prior party actions.

The core captures raw unit resources/base stats/equipment, separate temporary HP,
action/think/damage/break counters, selected/target unit references, the additional
timeline list, basic AI skill plans and nested skill-stack targets. It retains
duplicate unit IDs as separate entries. All actor references are translated to
capture-local unit indices; null references use -1. An unresolved non-null target,
duplicate actor/parameter, wrong owner or invalid collection causes refusal.
Neither pointers nor shallow engine containers leave the core.

An optional capture-local relation graph now translates vision/owner links, the
last damage dealer, the ordered list of damage sources and active skill targets.
Actor references and parameter references use separate pointer domains before
translation to the same unit indices. Duplicate IDs, repeated links and nulls
are preserved. Unrepresented reserve/vision/source objects, wrong domains,
unreadable data or an exhausted shared reference budget make the whole optional
graph null; other partial components remain available. Null/omitted graphs do
not imply that those relationships are empty. Old diagnostics remain accepted.
Mimic command payloads and complete ownership/lifecycle reconstruction remain
uncovered. Portable and compiled-DLL tests verify domain translation, duplicate
identity, replay/deep-copy preservation, optional failure and combined bounds.
This graph has not been captured or verified in FFR; the practice module is disabled.

These are raw stored components, **not displayed or fully calculated stats**.
The native HP getter adds base HP at +0x10c to temporary HP at +0x268. MP and
stagger getters clamp the underlying +0x110/+0x114 values against calculated maxima;
the level getter applies a multiplier to the stored level at +0x100. Normalizing
those values during capture would lose state. Raw maxima and base stats come from
the reflected UNIT_PARAMETER embedded at +0xc8 and do not include every runtime
modifier.

Static evidence also establishes duplication ID +0x550, turn-begin count +0x4dc
(retained under the legacy raw `actionCount` field),
think count +0x4fc, received-damage count +0x500 and break count +0x504. Reflected
AI skill-stack entries are 24 bytes, with skill ID at 0 and a nested actor array
at 8. The additional timeline array also contains battle actors. Per-array and
aggregate reference/stack limits bound detached output.

Continuous-effect counts remain available independently. A newer optional base
effect graph copies raw life-turn counters, effect IDs/types, timing bytes,
probabilities, activation conditions, status IDs, flags and integer parameter
lists. Native getter thunks corroborate the embedded effect-data layout and
life-turn/status offsets; reflected fields establish source/target parameter
links. A capture-local effect pool preserves distinct objects with identical
effect IDs, shared identities, repeated entries and ordered per-unit lists.
Null source/target parameters use -1. Unsupported references, unreadable objects,
invalid flags or exceeded bounds make the whole optional graph null while
retaining other partial components. Known empty lists are distinct from null
coverage. Shared reference limits and aggregate object/parameter budgets apply.

These are base components and raw counters, not a complete status snapshot or
verified application timing. Effect subclass/unreflected state, the main native
scheduler, callbacks, additional AI state, RNG and lifecycle reconstruction remain
explicitly missing. The JSON is read-only, partial and non-restorable. Tests cover
duplicate-ID identity mapping, nested targets, preserved raw values outside UI
clamps, deep copying, ownership checks, dangling targets and aggregate bounds.
Additional portable and compiled-DLL tests verify effect identity pools, raw
negative durations, source/target domains, replay preservation and optional
coverage failure. Legacy diagnostics remain accepted. The effect graph has not
been captured in FFR; the practice module is disabled.

The transport binds a fresh session, nonce, opening sequence/hash and an
eight-second deadline. A nonce is consumed before copying, and replay cannot move
the same request to a later frame. At most 128 request identities are retained
per process. Pending abort/import operations block capture. The copy performs
bounded memory reads and owned allocations without game calls, serialization or
setters. Only its latest detached report is retained.

The client validates the full schema, reference graph, raw-value types, aggregate
bounds, game identity and receipt freshness. `.ffrcombat.json` exports stay
separate from ordinary saves and snapshots. Isolated compiled-DLL fixtures verify
capture before/after prior actions, replay preservation, new-nonce replacement,
queued-work/stale-opening refusal and zero abort/import/serialization calls from
the combat request. Their actual JSON is also checked by the production Python
parser using only a test-scoped dummy-executable identity substitution.

The separate `FFRHelperRuntime` UE4SS mod:

- Collects encounter-opening/end notifications where reflected hooks execute.
- Reads available unit HP/MP/stagger/LB, levels, AI fields and turn counts.
- Reads job/timeline and saved-progression collection counts for coverage checks.
- Filters class defaults and refuses to select ambiguous live subsystem instances.
- Stores detached Lua primitives rather than live Unreal object references.
- Limits history to 32 events and output to 512 KiB.
- Observes only while the desktop supplies a fresh, session-specific lease.
- Never invokes save/load functions or setters, overrides a hook return, changes
  function parameters, constructs actors, or writes arbitrary native memory.

Its `.ffrruntime.json` reports are explicitly `diagnostic-observation`,
`readOnly: true`, `restorable: false`. The app does not accept them as `.sav` or
`.ffrsnapshot` files. They contain numeric IDs from the current encounter, so
detailed reports are not displayed automatically in the UI.

Lua engine calls are protected and executed on the game thread. Python rejects
oversized, stale, mismatched-session, mismatched-request, non-finite or misleadingly
restorable output. None of this establishes live game compatibility by itself.

## Battle-opening restart: acceptance requirements

The separate [partial recovery prerequisite](../native/PARTIAL_RECOVERY.md) now
copies two matching serialized progression objects with three stable field/clock
samples and repeated root/readiness checks. Its explicit read-only practice
request, worker readback/hash gate, strict Python consumer and `.ffrrecovery`
archive now pass compiled-DLL/consumer tests. Pending jobs, stale sources, changed
maps, sampling drift, publication races and write failures cause refusal. The
channel has not been exercised live and remains disabled with practice dispatch.
It is component
evidence, not an atomic world snapshot, rollback or restorable battle state.

### Native load prerequisites verified offline

The new scoped load router and progression-rebind controller are callback-driven
cores. They are **not connected to FFR, a runtime command or the restore UI**.
The router permits one validity check and one in-memory slot-reader substitution
for a private slot, exact manager, actual thread and exact native return addresses.
Unrelated calls retain their original path. Closing the scope removes the borrowed
target and prevents reuse. Its owner must retain both save objects through the
verified engine rooting API for the entire synchronous operation.

An isolated Windows executable verifies real MinHook detours around simulated
native load functions. The simulated native function owns its pointer assignment
and cache notifications; the detours substitute only its input. Wrong callers,
threads and managers pass through. This proves routing mechanics, not the actual
game's load delegates, cache consistency or world side effects.

The rebind core requires durable, read-back recovery bytes matching the current
root, imports and pins a separate target, verifies its bytes and input, and checks
the original progression and field preconditions again immediately before its
injected native-load callback. It marks the attempt before calling, never retries
or rolls back automatically, and compares the resulting root, bytes and context.
By default, every attempted application retains at most its two owned GC pins
until process exit, including a byte-matching result. Old caches may still refer
to the previous root, and the engine may have added its own root flag while the
helper's flag was set. Matching progression does not prove either ownership path.
Intentional retention and its final root-flag checks are reported separately from
failed pin release. Pre-existing engine roots are preserved. An explicit release
policy exists for independently verified ownership paths and controlled tests;
it must not become a client option for the unverified FFR experiment.
Tests cover rejected prerequisites,
changed recovery/target/input, callback exceptions, changed roots/bytes/context,
pin-release failure and the one-attempt rule.

A Windows native-load adapter now compiles separately into the isolated test
executable. Its exact FFR profile records the whole normal-load/validity/reader
function bodies and both direct return callsites, rather than only their entry
fingerprints. It verifies the disk build, loaded bodies, direct call targets and
module bounds before creating its two detours. It pins its code module and retains
the owner/disabled trampolines until process exit. Only its own hooks are disabled.
The shipping no-op that receives the slot argument is also fingerprinted and
checked again immediately before application, so a changed slot consumer cannot
silently reinterpret the private sentinel as an ordinary index.

Application requires the captured game thread, a fresh caller-supplied field
gate, exact manager/save vtables and the expected original root at manager +0x60.
The reader detour can write only a checked, aligned, writable out parameter on the
current thread's stack. Unrelated calls preserve their original path and Win32
error input. The game-owned normal-load function performs the root assignment.
One application attempt is allowed, including rejected attempts; an uncertain
postcondition cannot trigger another load or rollback.

The new compiled tests use this actual adapter with simulated native functions
and the pinned rebind controller. They exercise unrelated caller/manager/thread
activity while the scope is active, build/body/callsite refusal, wrong thread/
context/class, and context loss after native assignment with retained GC pins.
**Neither observer nor practice DLL includes this adapter yet.** Its FFR profile
has not been executed in the game. Runtime integration still requires verified
target/recovery clocks and field gates; actual load delegates, caches and world
effects remain live-test requirements.

Further static tracing identifies a concrete external effect in the ordinary
normal-load path: after `Load.End`, it reconciles saved achievement flags with
the online subsystem and can invoke the native achievement-unlock path. Byte
equality of progression cannot prove that platform state stayed unchanged.
`SaveData.Update` and `Load.End` also dispatch registered delegates whose complete
cache/world effects remain unverified. No such path has been invoked by the
helper in FFR. The supported load profile now explicitly marks external effects
unreviewed; the isolated adapter refuses initialization for that profile before
creating hooks. Controlled fixture profiles alone mark their dummy effects
reviewed. Future runtime integration requires a concrete review/containment of
these paths as well as the existing root, field, clock and recovery gates.

Static inspection also identifies a separate map-unit parameter cache owned by
the map manager. Its native lookup searches existing parameter objects rather
than deriving fresh values from the current save root. The verified constructor,
reflected owner/list fields and teardown path establish its layout; teardown
marks/releases cached objects and must not be treated as a generic refresh API.
This is another reason why matching root bytes or retaining both roots does not
prove gameplay cache consistency. Indirect delegate/Blueprint refresh behavior
remains unverified.

Further static review identified the examined storage routine through the
reflected `FullRecoveryMapUnit` registration. It adds large HP/MP amounts, clamps
resources and reloads cached values. It must be excluded from exact restoration:
it would heal the party instead of preserving the captured resource values.
No helper has called this routine.

A bounded read-only named-delegate snapshot parser is now available for the
remaining cache audit. Its layout follows the exact-build registration,
dispatch and script-broadcast implementations: sparse hash chains with holes,
multicast arrays and weak object/function-name references. It refuses active
dispatch/deferred changes, corrupt/cyclic chains, aggregate limits and changes
in payloads even when root pointers/counts match. Name decoding copies the
existing pool; it performs no lookup/initialization game call. Unit tests cover
sparse holes, stale bindings, malformed counts, cycles, payload drift, wide/
numbered names and replaced name blocks.

The private host reader collected all three load/save notification binding sets
at a stable exploration boundary, using query/read-only process access and
code/build checks: 11 live bindings were found. The update set includes chat,
backlog, mount, unit-view-model, quest and scenario consumers; load-start includes
system, talk and timer consumers. No callback was invoked by the reader. Binding presence alone
does not establish complete consumer coverage or callback side effects. The
normal-load adapter's external-effects gate remains false; no restore command
has been enabled by this parser.

New opening metadata can include an optional read-only `mapUnitCache`: bounded
raw inherited parameter components, equipment/passives and effect counts from
that profiled storage vtable. Distinct objects with duplicate unit IDs retain
separate indices. Storage/list identity is rechecked; invalid data or exhausted
bounds yields null metadata and a separate failure counter while preserving the
opening/progression. This is explicitly `coherentWithProgression: false` and
non-restorable: it is an independent diagnostic read, not proof that serialized
progression contains every current field resource. Old archives remain valid.
The offline comparator can report cache-component equality independently.

Portable and compiled-DLL/production-consumer tests verify detached raw values,
duplicate IDs, changed storage refusal, budgets and optional failure without
suppressing progression. Large retained encounter histories now omit oldest
events from a report when necessary to reserve room for opening metadata; the
report states `eventsOmittedForSize` and remains within its size limit. No cache
refresh/clear, root mutation or extra game function is invoked by this copy.
The 29 October test captured this metadata, preserved it after one command and
matched saved HP-base/MP fields. Broader comparison with live field resources and
full cache coherence remains outstanding.

### Live playtime basis capture, limited live verification

The saved `Player.PlayTime` value was unchanged across the three live opening
captures, despite elapsed wall time. Static inspection identifies a separate
`CPP_PlayTimeSubsystem`: the normal save-preparation path adds its elapsed seconds
to the saved float, then resets the live clock. The normal load path also resets
that clock. Calling save preparation would write an ordinary slot, so the observer
does not call it.

The newer observer now optionally copies the saved float and the live subsystem's
`StartSeconds`, `SuspendedStartSeconds` and `TotalSuspendedSeconds`, sampling the
game's own seconds source. Exact loaded getter fingerprints gate the calls. It
checks object headers, stable root/clock identities, unchanged raw fields, finite
saved seconds and bounded, consistent clock arithmetic. It calls no reset, pause,
resume or save-preparation function. Failure produces null metadata and a separate
counter while preserving the encounter/progression capture.

The native elapsed getter computes sample minus start minus completed suspension.
It does not subtract an ongoing suspended interval until resume; the capture
retains its start separately and labels the elapsed definition explicitly. Do not
present this as verified active-play duration. The strict Python consumer checks
the schema/arithmetic and requires the saved float to match `Player.PlayTime` in
the same GVAS payload. Old archives without this optional metadata still load.

Portable and compiled-DLL fixtures verify detached values, ongoing suspension,
changed-root/clock refusal, malformed clocks, production-consumer parsing and
non-suppression of progression when the clock is unavailable. The 29 October
openings verified detached clock/rounding metadata and saved-float correspondence;
displayed-clock and suspension lifecycle comparisons remain outstanding.
The partial recovery channel also captures a
fresh clock basis. Playtime restoration remains outstanding, alongside live
field/world restoration. The absence of
a property from a serialized object is not evidence that every possible non-default
value is omitted; no broader serialization-loss claim is made here.

The latest observer also records the native thread's MXCSR rounding control when
it is unchanged across the clock read. Legacy diagnostics without that optional
field remain valid. An offline Python/C++ planner now previews normal-load
playtime normalization only for known nearest-even rounding, no ongoing suspension
and bounded normal saved values. It reproduces the two separate binary32 stages:
integer elapsed seconds are rounded to float first, then added to the saved float
with another nearest-even rounding. A single high-precision sum can give a different
answer at tie boundaries.

The Python planner preserves the original opening and prepares distinct prospective
bytes changing only `Player.PlayTime`. Its `.ffrplaytime.json` export contains a
small non-restorable receipt with source/target hashes and values, never a game
slot or target GVAS file. No native command accepts these bytes. Unknown rounding,
ongoing suspension, subnormal values and overflow are refused. Tests cover exact
tie cases, 2,000 native two-stage comparisons, source/unknown-field preservation
and format separation. A separate experimental detached-object probe now applies
the plan to an owned clone and rechecks current rounding. A fresh recovery basis
is captured by the read-only recovery channel. Actual clock reset/restoration
remains outstanding. The detached-clone probe and recovery capture have not been
exercised in FFR; optional opening-clock capture has the limited verification above.

For a new opening with supported clock metadata:

```sh
python tools/plan_opening_playtime.py local/opening.ffropening --output local/opening.ffrplaytime.json
```

A bounded native tagged-property walker now locates the canonical
`Player.PlayTime` float through the supported GVAS header and nested property
types. It refuses duplicate/ambiguous names, unsupported flags/types, truncation
and missing fields rather than using a substring or client-supplied offset. Its
location/value matched the independent full decoder on all three private
engine-produced captures. A pure preparer changes exactly those four prospective
bytes using the two-stage float32 plan and preserves the source.

The callback-driven normalized-clone probe now verifies a separate rooted import
before a fixed-member write, compares its serialization to that exact prospective
payload, and checks unchanged input and live progression. It permits one attempt
and retains owned roots if live identity changes or becomes uncertain. Tests use
separate current/source values, a fixed native-layout member, wrong clones,
incorrect writes, original corruption, exceptions and cleanup failures.
The separately named practice build now connects this core to a distinct explicit
`probe-normalize` request and `normalizationProbe` receipt. Its fixed-member writer
requires a separate rooted matching clone, the original scalar bits, a writable
page, supported template/destruction flags, initialized loader, strict cleared
field boundary and source/current nearest-even clocks without ongoing suspension.
The process-wide attempt excludes later import/abort actions, including after a
refusal. Worker publication commits and reads back fixed expected/clone GVAS
files; the production Python consumer compares their complete bytes with an
independent source-preserving plan and checks receipt races. Compiled-DLL fixtures
cover success with different source/current progression, uninitialized loader,
stale source, template refusal, evidence write failure and replay/exclusion.
This path awaits actual engine verification and is disabled with practice dispatch. See
[detached normalization](../native/DETACHED_NORMALIZATION.md). It performs no live
load, clock reset or battle recreation; the ordinary observer excludes the path.

### Partial Windows CRT RNG capture

Static inspection identifies a weighted-skill selector calling the game's
imported Windows CRT `rand`. The new observer can optionally read that stream's
existing per-thread/fiber state at encounter entry without advancing it. An exact
system DLL hash, loaded `rand`/PTD getter bodies and the game's import target gate
the feature. Existing FLS slots and a verified state-selection layout locate the
seed; the observer never calls `rand`, `srand`, the allocating PTD getter, a slot
allocator/setter or an RNG setter. It preserves Win32 error state. The public
[FlsGetValue API](https://learn.microsoft.com/en-us/windows/win32/api/fibersapi/nf-fibersapi-flsgetvalue)
only reads an existing calling-fiber slot value.

Both reads around progression serialization must agree on thread, selected
state object/index and seed. Unknown CRT versions, getter selections, missing
thread state or changed samples leave `crtRandState` null without suppressing
progression. Exported metadata contains the unsigned 32-bit seed and selected
state index, no addresses, and explicitly claims only the current game thread's
CRT stream. Other threads and engine/plugin RNG sources remain uncovered.

Portable tests and the compiled observer's own dummy executable verify bounds,
selection, detached metadata and optional failure. An additional Windows test
reads this same profiled system CRT in its own executable and checks the
observed state against controlled draws; unsupported system versions skip that
test. A separate private helper-process audit checked 4,128 transitions and
distinct concurrent-thread state. These are isolated-process tests, not live FFR
verification. The 29 October openings separately verified seed availability,
detachment after a command and a changed CRT seed in the next encounter. Actual
thread/draw-order coverage remains unverified. No CRT binary is distributed;
seed restoration or complete gameplay RNG coverage is not implemented.

### Partial engine-global RNG capture

Static inspection confirms a separate engine-global 32-bit generator,
initialized independently beside the CRT seed initialization. Its draw updates
that separate seed with a multiply/add recurrence and constructs a fractional
float from its lower 23 bits. The current CRT capture does **not** cover this
state. The new independent `engineGlobalRngState` optionally copies its raw seed
without calling the draw or initialization functions. Exact game identity and
complete loaded fingerprints of both routines gate the fixed-member read;
fingerprints are rechecked around each two-read sample. Address overflow,
alignment, unreadable/changing data and changed code are refused.

Samples before and after progression copying must match. This does **not** prove
that no intervening draws occurred, or that other threads were quiescent. The
metadata explicitly sets `atomicSnapshot: false` and leaves other RNG sources
uncovered; it exports no address. Any optional failure leaves null metadata and
a separate failure count while retaining progression. Portable tests preserve
unsigned/zero seeds and refuse drift/bounds errors. The real DLL fixture changes
its own seed during serialization and patches its never-called dummy draw code
after initialization; both cases retain progression and refuse the RNG sample.
Independent Python checks enforce the coverage limits and preserve old archives.

Direct engine callers are present; actual combat/thread use, indirect or inlined
callers and per-instance generators remain unverified. No runtime draw,
initializer, setter, game suspension or seed restore is added. The 29 October
openings captured this optional seed; it was unchanged between those encounters,
so that test does not establish actual combat use. Even matching CRT and engine-global samples
cannot establish complete or atomic RNG restoration.

### Remaining restart acceptance requirements

Before enabling a restart button, an implementation must demonstrate:

1. Capture occurs before party resources or encounter flags change, including
   scripted battles and preemptive encounters.
2. Full encounter setup and pre-battle progression/resources are deep-copied.
3. Restore cancels the current combat through a verified teardown path, including
   queued effects and UI/event jobs, without recording a win/loss/escape.
4. Repeated restart restores consumables, HP/MP/LB, party/equipment, EXP/MRP,
   counters, drops/steals and encounter-specific state exactly.
5. Story events, quest counters, achievements and rewards are not duplicated.
6. The original opening remains unchanged across repeated restarts; changing
   battles invalidates or explicitly replaces it.
7. RNG behavior is documented and verified, whether restoring the same seed/state
   or intentionally starting with fresh randomness.

## Mid-battle save states: additional acceptance requirements

A full gameplay restore must also account for native scheduler positions and
turn order; queued actions and targets; buffs/status durations; stagger/chain/LB;
enemy AI counters, phases and skill stacks; dead/despawned/summoned units; pending
jobs, callbacks and latent events; world/story state; inventory and progression;
and every gameplay RNG source and draw order.

Stable object identities and lifecycle reconstruction are required. Copying raw
UObject memory, shallow-copying pointers or exporting only reflected fields is
not an acceptable restore implementation. Saving CPU process memory alone also
does not restore external GPU, OS-handle and engine-thread state.

The intended native approach reconstructs supported gameplay state through
verified game/engine paths. It must not be described as capturing literally all
CPU/GPU process state. Unsupported states must be rejected explicitly, not
silently reduced to a normal save or partial combat restore.

Snapshots will need game-build/schema identity, completeness metadata, bounded
parsing, integrity checks, and a verified pre-restore recovery point. They must
remain separate from diagnostic reports and ordinary-save checkpoints.

## Future live verification

The desktop's **Runtime research → Inspect saved capture** can review an existing
`.ffropening` without a game connection or working-save change. It validates the
full archive/progression, then shows partial field/clock/cache/RNG availability
without encounter IDs, coordinates, seed values or progression contents. Missing
legacy metadata stays unavailable. Presence never establishes completeness or
live verification. The equivalent read-only command is:

```sh
python tools/inspect_opening.py local/source.ffropening
```

An offline opening-component comparator now validates every bounded encounter
entry field, compares the complete serialized progression including unknown
properties, and optionally compares the independent normalized playtime
candidate and partial field anchors. It reports changed field names without
printing encounter values. Legacy clocks yield an unavailable normalization
comparison rather than invented elapsed time. A repeated or byte-matching
capture cannot prove an actual restart, independent provenance, full resources,
RNG, world restoration or absence of external effects; these gaps stay explicit
even when every compared component matches. Its `.ffrcomparison.json` export is
a small diagnostic receipt with no GVAS payload or native command.

```sh
python tools/compare_openings.py local/source.ffropening local/observed.ffropening --output local/check.ffrcomparison.json
```

The three-encounter native test is complete. Native capture and replacement,
preservation of earlier openings, escape counters and victory EXP/MRP updates were
observed in ordinary encounters. Further coverage must include scripted/preemptive
variants and a simultaneous comparison with live battle resources. The previous
Lua test established basic combat observation, but its initialization failed on
the native test launch as described above.
Do not enable mutation or restore hooks merely because recorder tests pass.
Subsequent tests need repeated retries, deaths/summons/statuses and interrupted
restores on disposable practice data before any completeness claim.

For development sessions where the user operates the game, the optional command
line collector maintains the observer lease and writes private timestamped reports:

```sh
python tools/observe_runtime.py /path/to/FFRHelperRuntime --output local/observations --duration 600
```

It prints only availability, battle-active state and collection counts. It never
launches or controls the game, installs a loader or sends restore commands. At
timeout or Ctrl+C it expires the lease. Keep its output outside source control.
For scripted shutdown, pass `--stop-file local/stop-observer` and create that
local marker when collection is complete; this also expires the lease cleanly.

## Platform references

- [UE4SS RegisterHook semantics](https://docs.ue4ss.com/dev/lua-api/global-functions/registerhook.html): hooks require loaded functions; callbacks can override return values, which this recorder never does.
- [UE4SS UObject access](https://docs.ue4ss.com/dev/lua-api/classes/uobject.html) and [TArray traversal](https://docs.ue4ss.com/dev/lua-api/classes/tarray.html).
- [Epic SaveGameToMemory](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/UGameplayStatics/SaveGameToMemory): serialization concerns a SaveGame object, not a complete running world.
- [Microsoft process snapshotting](https://learn.microsoft.com/en-us/previous-versions/windows/desktop/proc_snap/overview-of-process-snapshotting): diagnostic snapshot/query facilities; not a general game rewind facility.
- [CRIU GPU checkpointing](https://criu.org/GPU_Checkpointing): GPU resources live outside ordinary process memory and require special support.

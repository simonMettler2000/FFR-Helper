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
That loader-initialization issue remains to be addressed separately. No signature
override or additional hook was installed during this live test.

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

The portable reconstruction core now rebuilds every captured encounter field,
including nested passives and UTF-16 strings, using supplied engine allocation
and release functions. It never passes captured vector pointers into the game.
Failure-injection tests cover each allocation boundary, detached copying,
Unicode validation, ownership transfer and exactly-once consuming destruction.
The verified engine allocator entry is RVA 0x12406c0 with size/alignment arguments;
integration with actual native encounter recreation remains pending.

The separately compiled [abort trial](../native/ABORT_TRIAL.md) adds a profiled
battle-tick dispatcher and one EndBattle request, with session/revision/hash/nonce
binding, expiry, a durable progression-only recovery gate and a second explicit
commit. It checks the native command-menu boundary and rejects prior friendly
actions, queued jobs and unsupported encounter flags. Cleanup observation includes
the matching context and cleared battle UI/unit/job state. Isolated DLL fixtures
verify the complete file/dispatcher path and exactly-once behavior, including
rejection after a prior action. It has not been installed or tested in FFR.

Profiled layout evidence includes the manager's secondary tick interface at
offset 0x30, active flag at 0x1a9, unit/job managers at 0x48/0x40, unit parameter
pointer at 0x838, unit ID at 0xd0 and action counter at 0x4dc. The UI phase is at
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
failure observed on the last native capture launch. Export names/RVAs and full
module hashes bind its primitive rooting calls; no SDK containers cross the DLL
boundary. One attempt is supported per process, with no automatic command retry.
Isolated real-DLL fixtures cover success, loader refusal, mismatched clones,
changed live bytes and prior friendly actions. Live import remains unverified.

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
It is staged locally and has not been installed or verified in a fresh launch.

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
into owned values. It is **not wired to a runtime command or installed in FFR**.
The future caller must establish the game thread, matching world and actual
command-menu boundary; empty queues and an active flag alone are insufficient.

The core captures raw unit resources/base stats/equipment, separate temporary HP,
action/think/damage/break counters, selected/target unit references, the additional
timeline list, basic AI skill plans and nested skill-stack targets. It retains
duplicate unit IDs as separate entries. All actor references are translated to
capture-local unit indices; null references use -1. An unresolved non-null target,
duplicate actor/parameter, wrong owner or invalid collection causes refusal.
Neither pointers nor shallow engine containers leave the core.

These are raw stored components, **not displayed or fully calculated stats**.
The native HP getter adds base HP at +0x10c to temporary HP at +0x268. MP and
stagger getters clamp the underlying +0x110/+0x114 values against calculated maxima;
the level getter applies a multiplier to the stored level at +0x100. Normalizing
those values during capture would lose state. Raw maxima and base stats come from
the reflected UNIT_PARAMETER embedded at +0xc8 and do not include every runtime
modifier.

Static evidence also establishes duplication ID +0x550, action count +0x4dc,
think count +0x4fc, received-damage count +0x500 and break count +0x504. Reflected
AI skill-stack entries are 24 bytes, with skill ID at 0 and a nested actor array
at 8. The additional timeline array also contains battle actors. Per-array and
aggregate reference/stack limits bound detached output.

Continuous-effect count is recorded, but effect objects/durations, the main native
scheduler, callbacks, additional AI state, RNG and lifecycle reconstruction are
explicitly missing. The JSON is read-only, partial and non-restorable. Tests cover
duplicate-ID identity mapping, nested targets, preserved raw values outside UI
clamps, deep copying, ownership checks, dangling targets and aggregate bounds.

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

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

All raw reports, save backups and user-specific values stay outside Git. No game
progression/resource setters, save/load calls or restore operations were invoked.

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

First confirm recorder compatibility and observations in an already-visited
field and early encounter.
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

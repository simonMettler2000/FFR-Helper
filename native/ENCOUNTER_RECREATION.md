# Current-field encounter recreation experiment

This is a prerequisite for battle restart. The installed position/direction
correction successfully recreated one untouched normal encounter: the native
entry, matching independent child, fresh menu and unchanged progression were
verified, and the user confirmed the restarted encounter and responsive menu.
The user also reported incorrectly positioned party timeline portraits; a
natural normal-encounter comparison confirmed that discrepancy. Repeat shortcut
use is deliberately inactive in that installed one-shot build. The uninstalled
5 November development changes add guarded repeated cycles, verified only in
isolated fixtures so far.
See the [resume handoff](../docs/NATIVE_STATE.md#resume-handoff--29-october-2026).
This experiment does not restore progression, resources,
effects, clock or RNG, and does not implement retry after an action.

The separate practice DLL can request the same ordinary encounter setup once
after its verified, untouched opening-exit trial. The read-only observer excludes
this command. It uses the engine's current field state, including existing
effects; it never reconstructs effects from partial cache metadata. The stricter
unbound restoration/reentry core still refuses unsupported effects.

Preparation requires the same captured source, session, game thread, map and
live progression root. Both durable exit files and the current complete
progression must match the original bytes. Battle managers must be released,
queues empty, UI inactive and both command/skill histories untouched. The recorded
raw map-cache components must match the source. The original pre-entry position
remains immutable evidence. A separate post-exit `entryFieldAnchor` must retain
the original map, with both position and direction exactly stable for three game ticks.
That target is bound through commit, allocation and entry; later drift refuses
the operation. The child capture must match this prepared anchor. Receipts
separately report whether it equals the original anchor and explicitly set
`worldPositionRestored=false`. Every failed native preflight condition now names
the failed check, including identity, expiry, progression, map, prepared pose or
raw cache; no additional mutation is performed to diagnose it.
No tolerance or position setter is used. Partial
component equality does not prove complete effect or cache coherence.

A separate nonce and eight-second preparation deadline bind one explicit
commit. Before and after allocating input, the adapter rechecks the transaction,
loaded code, thread and ownership profile. The engine allocator constructs an
owned copy of all arrays, nested passives and BGM. The profiled consuming map
entry deep-copies these and destroys its input. Ownership transfers once before
invocation; an uncertain call never triggers a retry or helper double-free.
The adapter recognizes only the observer's own installed MinHook relay and
retained trampoline. Clients cannot supply paths, native pointers or RVAs.

Native return alone is insufficient. The observer must independently capture a
child at the next sequence, with matching setup/progression/context, then observe
a fresh untouched ordinary command menu. Current progression and root must still
match there. A capped 30-active-game-second budget limits this verification.
The Python controller independently validates and exports that child, compares
recorded field/cache components and retains both exit and recreation receipts.
The user must confirm the actual new encounter and responsive menu.

For a separately approved supervised test, with FFR initially closed for any
DLL update, use a unique private output directory:

```sh
python tools/run_abort_trial.py /path/to/FFRHelperBattlePractice --output local/recreation-test-unique --duration 900 --abort-at-opening --recreate-after-abort
```

Manual activation is required: release the buttons/keys, then press the supported
foreground shortcut once at an untouched normal first command menu. A calibrated
legacy controller can be supplied through `--legacy-controller`. No timer
activates this experiment. Foreground loss before either commit refuses it.
Old exit-only DLLs are refused before any exit request. Surprise, preemptive,
scripted or already acted encounters are excluded. A terminal or uncertain result
requires diagnosis and a fresh supervised launch, never another automatic attempt.

Evidence remains explicitly `battleRestartImplemented=false`,
`progressionRestored=false`, `rngRestored=false`, `effectGraphsVerified=false`
and `fullCacheCoherenceVerified=false`. Original save slots are never written.
Post-action rollback, live progression rebinding and complete cache/effect/RNG
restoration remain outstanding. Saving anywhere stays last.

## Repeated untouched-opening cycles (offline development, 5 November)

The prepared source adds `--cycles N` (1–100; default 1). Values above 1 require
explicit manual abort plus recreation, and a runtime that advertises the new
verified-child rearm protocol. The existing installed DLL does not support it.
Do not install this build or start a controller without the next supervised test.

The controller stays connected after a verified child and waits for a **new
released foreground shortcut edge**. Holding the previous combination cannot
request another cycle. A new short-lived nonce first requests bookkeeping rearm;
the acknowledgment must match the retained child, session, progression hash and
native generation. Only then may a fresh abort prepare/commit proceed. Foreground
loss, expired intent, changed history/context, uncertain I/O or a refused receipt
stops the controller. It never resends a potentially completed request.

Native rearm requires a returned, consuming, cleaned-up entry and an independently
verified untouched child. Code/allocator/hook fingerprints, exact current
progression/root, command task, queues and both command/skill histories are checked
again. Neither a rejected nor an uncertain/timed-out transaction can reset.
Generation counters bind each native invocation and stale acknowledgments cannot
authorize another one. Rearm itself calls no game mutation routine.

Before rearm, the runtime must commit and read back immutable `manifest.json` and
`progression.gvas` files beneath
`connection/cycle-history/<session>/generation-N/`. Existing different files are
never overwritten. The client independently compares them to both retained
openings and request identities, then exports a copy into the completed cycle's
private output directory. Session separation prevents a new launch from colliding
with an earlier generation 1. Multi-cycle output uses `cycle-0001`, `cycle-0002`,
and so on; prior receipts and openings remain intact.

The actual compiled test DLL completed three sequential cycles with balanced
engine-owned allocations. Separate fixture cases refused changed command history,
progression, task step, manifest contents and a reused nonce. Python checks cover
receipt/archive validation, uncertain writes, two-cycle orchestration, held keys,
foreground loss and legacy DLL rejection. These mock-host results do **not**
verify timeline layout, repeated input or battle behavior in FFR. The timeline
discrepancy and post-action rollback remain blockers for calling this battle restart.

For the next separately reviewed supervised run, add
`--capture-child-diagnostics --cycles 2`. This retains an independent read-only
combat/timeline snapshot at each verified child before accepting another deliberate
press. Missing/stale party view-model links, unavailable evidence or read expiry
stop the controller; no read or gameplay command is resent. New reports include
raw stored parameter relation bytes; unavailable bytes or a party byte differing
from Friendlies also stop before rearm, after retaining the report. These values
are copied without executing the relation getter. A successful link
snapshot does not verify the actual rendered portraits, so visual confirmation
is still required before the second press. This option and channel extension are
also uninstalled and have not been tested in FFR.

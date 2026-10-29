# Current-field encounter recreation experiment

This is a prerequisite for battle restart. It has isolated test coverage and
**has not been tested in FFR**. It does not restore progression, resources,
effects, clock or RNG, and does not implement retry after an action.

The separate practice DLL can request the same ordinary encounter setup once
after its verified, untouched opening-exit trial. The read-only observer excludes
this command. It uses the engine's current field state, including existing
effects; it never reconstructs effects from partial cache metadata. The stricter
unbound restoration/reentry core still refuses unsupported effects.

Preparation requires the same captured source, session, game thread, map and
live progression root. Both durable exit files and the current complete
progression must match the original bytes. Battle managers must be released,
queues empty, UI inactive and both command/skill histories untouched. The typed
field anchor and recorded raw map-cache components must also match. Partial
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
Post-action rollback, live progression rebinding, complete cache/effect/RNG
restoration and repeatable retry remain outstanding. Saving anywhere stays last.

# The game's pre-battle backup

Read-only inspection found a game-owned `BTL_BEFORE_STATR_INFO` with raw unit
parameters, inventory, event flags, money and two graphics values. Its existence
does not establish a callable free retry or a complete save state. The native
retry route currently identified consumes an item and remains unbound.

The 5 November audit traced the battle-start producer copying current inventory,
event flags and money into that backup. It also identified the two trailing
floats through their native console-variable getters:

- `r.DOF.Kernel.MaxForegroundRadius`
- `r.DOF.Kernel.MaxBackgroundRadius`

These are depth-of-field settings. They provide no RNG or playtime coverage.
Result cleanup uses corresponding graphics helpers; no such helper was invoked
during this audit. Some earlier offset-only search results were stack locals or
unrelated structures, so their numeric offsets alone were not treated as proof.

`ffr_helper.battle_backup.snapshot_battle_backup` prepares the next read-only
comparison. Given a caller-verified exact-build battle/root context and bounded
reader, it copies raw unit resource/stat components, equipment/passive ID arrays,
inventory, event-flag hashes and money. It separately compares current inventory,
flags and money with the retained backup and rereads the captured graph to detect
drift. It neither opens a process nor calls a getter, constructor, loader or setter.

Every unit row is preserved in order, including duplicate IDs. An earlier private
sample contained repeated party IDs; their lifecycle/meaning is unresolved. A
restore must not silently choose the first row or discard duplicates. The new
diagnostic will show whether their resource values agree, without assuming they
are interchangeable.

Resistance maps, text/native object graphs, derived effects, complete unit state,
RNG and verified restore semantics remain uncovered. Pointer-bearing native
records are not portable checkpoint bytes. The report explicitly says
`restorable=false`, `deepUnitParametersVerified=false` and `rngVerified=false`.
Seven synthetic-memory tests cover resource changes, duplicate rows, nested-array
drift, failed reads and typed bounds. This expanded diagnostic has no live test yet.

Before considering the native rollback path, compare a fresh ordinary opening,
one completed command and the following command menu. Verify retained resource
rows against independent progression/combat captures, then finish auditing the
retry task's unit teardown, event subscribers and snapshot lifecycle. Keep normal
load's external-effects gate closed and Saving anywhere deferred. No native
retry, item-cost routine or restore is authorized merely by this documentation.

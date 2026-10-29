# Partial recovery capture prerequisite

The copying core, explicit read-only practice-runtime channel, durable file
publication, strict Python consumer and `.ffrrecovery` archive are implemented
and fixture-tested. **The recovery capture has not been exercised in FFR.** Fresh
idle-menu and released-manager field boundaries have been observed in supervised
tests. No recovery request has been sent and no restore command or GUI control
is connected.
This complements the abort experiment's separate
durable progression-only file; neither format can resume a battle.

`partial_recovery.hpp` requires a supported field boundary and the expected map
ID. The native owner checks the captured game thread, same world/map,
cleared battle manager/UI/units/targets/jobs and no pending mutation. It retains
only detached progression bytes, a partial field anchor and a playtime basis.

Two serializations must be byte-identical and pass the private captured-GVAS
header gate. Three field/clock samples must remain coherent across those calls;
root identity and readiness are rechecked. Clock bases and rounding control must
stay unchanged, while sampled seconds may advance monotonically by at most two
seconds across the entire operation. Field position/direction/map must match.
The core invokes no root assignment, reset, save preparation, slot writer, abort
or encounter reconstruction. Engine-buffer allocation/free remains the existing
in-memory serialization wrapper's responsibility.

This detects drift in the copied components. It is **not an atomic world snapshot**.
Camera, streamed actors, events, RNG, native callbacks and a complete restore path
remain missing. Ongoing suspension can be retained as evidence, but the separate
normalization planner refuses to apply assumptions to it.

## Capture protocol and validation

The six-line `FFR_HELPER_PARTIAL_RECOVERY_V1` request carries session, nonce,
expiry, opening revision and opening hash. It accepts no address, payload, path,
slot or value to write. A request is consumed before copying, even on refusal;
replay preserves the earlier result. History is bounded to 128 identities per
process and only the latest detached result is retained. The default observer
does not contain this channel.

Typed opening field/clock metadata, matching map ID and actual thread/world are
required. Selected unit, targets, units and jobs must be empty with battle UI and
manager cleared. Pending abort/import work blocks capture. A completed own abort
may be observed after these field checks pass; this does not prove a full rollback.

The worker atomically writes fixed `recovery-progression.gvas` and
`recovery-metadata.json`, verifies both files by reading them back, then advertises
a committed descriptor in `recovery-report.json`. Write failure produces no
committed receipt and never resnapshots the game automatically. Metadata carries
the source identity, partial field/clock data, coherence counts and explicit gaps.

Python validates the complete GVAS, its saved playtime correspondence, hashes,
schema/types/bounds, source identity and fixed filenames. The live bridge rechecks
the receipt after the files to reject publication races. The archive preserves
the exact metadata/progression bytes with exactly two bounded ZIP entries. It
cannot be exported under a game-save/checkpoint/opening extension. Missing or
ambiguous serialized `Player.PlayTime` is currently refused rather than guessing
that an omitted field has its default value.

Compiled-DLL fixtures use original synthetic GVAS data and the production Python
parser. They verify fresh values versus the opening, replay preservation, another
explicit capture, pending jobs/stale openings/changed maps, serialization drift,
write failure and buffer release. They invoke no abort, import or extra encounter.

## Later live test

Start the same recorder session before entering an ordinary encounter. Finish or
flee through the normal game controls, then capture one supported field boundary:

```sh
python tools/observe_recovery.py <FFRHelperBattlePractice-folder> --output local/recovery-test --duration 600 --capture
```

Without `--capture`, the command only observes availability. Use a new output
directory. Ctrl+C, timeout and `--stop-file` expire the lease. The command never
launches or controls gameplay. Detailed game data is not printed automatically.

Actual field/clock/display agreement remains unverified. Normalized clone
application and native load/cache/clock/world verification remain subsequent
experiments. This component is not a full save state or an automatic recovery path.

Neither installed observer nor original save slots are changed by this work.
Saving anywhere remains deferred and is not a prerequisite.

# Partial recovery capture prerequisite

Only the callback-driven copying core and portable tests are implemented here.
**No runtime recovery request, durable bundle, restore command or GUI control is
connected yet.** This is distinct from the existing abort experiment's durable
progression-only file, which cannot resume a battle.

`partial_recovery.hpp` requires a supported field boundary and the expected map
ID. Its future native owner must prove the captured game thread, same world/map,
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

## Remaining integration

1. Add a separate explicit read-only request in the uninstalled practice build,
   bound to fresh session/nonce/deadline and the retained opening revision/hash.
   No address, payload, path or value to write may come from that request.
2. Carry the retained opening's typed field/clock metadata. Require matching map
   ID and actual thread/world, empty selected/target state and idle jobs. Pending
   abort/import work must block capture. Completed teardown needs its own checks.
3. Copy through the existing bounded serialization/free and profiled read-only
   getter paths. Consume a request identity before capture; replay must not take
   another frame. Retain only the latest result with bounded request history.
4. Have the worker atomically persist and read back fixed progression/metadata
   files, verify both hashes, then publish a matching receipt. A failed write must
   never become a committed recovery point.
5. Validate the complete GVAS and metadata in Python, including saved playtime
   correspondence, source identity, freshness and fixed filenames. Read the receipt
   again after the files to reject publication races.
6. Export only a separately named non-restorable diagnostic archive, with bounded
   ZIP entries and explicit missing coverage. Never accept it as an ordinary save,
   checkpoint or full gameplay state.
7. Verify field captures after actual encounter exit and clock/display agreement
   in a later opted-in live test. Normalized clone application and native load/cache/
   clock/world verification remain subsequent experiments.

Neither installed observer nor original save slots are changed by this work.
Saving anywhere remains deferred and is not a prerequisite.

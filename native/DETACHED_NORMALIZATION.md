# Detached playtime normalization prerequisite

The optional `FFRHelperBattlePractice` component can explicitly compare one
detached normalized progression object. It is **uninstalled and not tested in
FFR**. This prerequisite does not restore a save state or restart a battle.
The ordinary read-only observer excludes its request and writer paths.

Normal loading resets the live playtime subsystem. The saved float alone does
not include unsaved elapsed time. A retained opening therefore needs supported
clock metadata before it can become a candidate for a later normal-load path.
The original opening remains unchanged.

Start the collector before an ordinary encounter, then exit that encounter
normally. Observe readiness without requesting an import or write:

    python tools/run_normalization_probe.py <FFRHelperBattlePractice-folder> --output local/normalization-observe

The explicit experimental command is:

    python tools/run_normalization_probe.py <FFRHelperBattlePractice-folder> --output local/normalization-test --probe-normalization

Use a fresh output directory. Ctrl+C, timeout and an optional `--stop-file`
expire the observer lease. The collector does not operate the game. A later
installation/live test needs separate approval; this document does not authorize
installation.

## Boundaries and comparison

The existing seven-line abort protocol has a distinct `probe-normalize` phase.
It carries only session, nonce, expiry, opening revision and opening hash. It
accepts no payload, path, address, member offset, scalar value or slot number.
Ordinary `probe-import` receipts retain their original meaning. Normalization
uses a separate `normalizationProbe` status object and explicit operation name.

The dispatcher requires the actual captured game thread and world/map, an
ordinary source encounter, cleared battle manager/UI, empty units, selected
unit, targets and jobs, typed source field/clock metadata, initialized verified
loader APIs, and nearest-even source/current rounding. Ongoing suspension is
unsupported. One process-wide import/normalization attempt is consumed before
work, including refusals; it cannot be combined with an abort trial or retried.

The native tagged-property walker prepares prospective bytes by changing only
the canonical serialized `Player.PlayTime` float. It uses separate nearest-even
integer-to-binary32 conversion and binary32 addition. Missing/ambiguous fields,
unsupported tags/flags, subnormal values and overflow cause refusal.

The engine imports the observer's own retained source bytes into a distinct save
object. Exact class/vtable and rooting checks gate a fixed native member at
clone +0x38. The writer refuses the live root, class/archetype/default templates,
destruction states, a changed original value or a non-writable page. It may write
exactly one float on this verified detached object. It never changes the live
root, resets a clock, invokes normal load, aborts/recreates a battle or writes
an ordinary save slot.

Success requires unchanged input, byte-identical source clone before the write,
exact prospective clone bytes afterwards, unchanged current progression before
and after, and verified release of helper-owned temporary roots. Pre-existing
engine roots are preserved. Changed/uncertain live identity retains at most two
owned roots until exit and cannot produce success.

The worker commits two fixed diagnostic files with atomic replacement and
hash/readback checks: `normalization-expected.gvas` and
`normalization-clone.gvas`. Publication failure never retries the engine action.
The strict Python consumer verifies full GVAS, saved float, hashes, receipt
identity/freshness and an independent Python source-preserving normalization
plan. It exports evidence only after these comparisons succeed. These files
are not game slots or resumable snapshots, and no native command accepts them
as input.

## Verification and remaining work

Compiled-DLL fixtures exercise successful normalization of a source distinct
from the current live progression, uninitialized loader, stale source, default
template refusal, evidence write failure, replay and later import/abort refusal.
The actual DLL output is checked by the production Python evidence consumer and
independent planner using original synthetic GVAS in a dummy process. Portable
tests also cover changed roots during the final serialization, incorrect clone
writes, original corruption, exceptions and rooting cleanup.

Actual engine import semantics, field/clock samples and fixed-member behavior
still need a supervised live test. Normal-load/cache rebinding, actual clock
reset, field/world restoration, encounter reconstruction and mid-battle
scheduler/status/AI/RNG restoration remain outstanding. The native load adapter
continues to compile only into an isolated test executable.

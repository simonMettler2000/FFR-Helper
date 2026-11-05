# WIP development handoff — 5 November 2026

This is an unfinished community starting point. Development is intermittent;
contributions are welcome. Keep unsupported operations unavailable rather than
making a successful partial capture look like a complete restore.

## Current implementation and evidence

- The offline editor supports encrypted ordinary saves, total EXP/MRP editing,
  inventory/equipment, route totals and backed-up ordinary-save checkpoints.
- The optional exact-build Windows observer captured ordinary encounters and
  progression changes in supervised tests. Its exports are not restorable states.
- Detached import reproduced a captured progression payload without changing the
  live progression/input; this is not a live-state load.
- A guarded untouched-opening exit reached the field without escape/results.
  One same-setup encounter recreation subsequently reached a responsive menu
  with matching setup/progression. Party timeline portraits appeared on the wrong
  side. The impact on combat remains unknown.
- The new repeatable controller uses released shortcut edges, expiring nonces,
  verified child generations and immutable evidence. A compiled mock-host DLL
  completed three cycles. It has not completed repeated live FFR trials.
- Before another cycle, the client retains child combat evidence and requires
  every opening party ID exactly once with complete matching current source/cache
  links. Missing/duplicated members, stale references, unavailable relation bytes
  or unexpected party relation bytes refuse another cycle. It never resends an
  uncertain native operation.
- The last local suite ran 272 Python cases (271 passed, one optional skip).
  The current native runtime passed 128 isolated cases; the compiled three-cycle
  case was rerun with the stricter client gate. No live verification accompanied
  publication. Native runtime changes after the last live round are uninstalled.

## Next battle-restart work

1. Obtain a fresh untouched **normal** encounter baseline on the exact build;
   exclude surprise/preemptive encounters and automatic/counter actions.
   Establish a responsive settled first command menu before diagnosing bindings.
2. Compare the current actor/parameter/UI registry with timeline entries and
   portrait sources immediately after one separately reviewed recreation. The
   reusable bounded readers are `timeline_diagnostics.py`, `widget_diagnostics.py`
   and `timeline_findings.py`. They accept an independently verified reader/context;
   they neither open a process nor call engine functions. Private development
   collector scripts and personal captures are not part of this publication.
3. Check the cache hypothesis in [TIMELINE_DIAGNOSTICS.md](TIMELINE_DIAGNOSTICS.md).
   The unchanged-entry-identity path skips the observed direct unit-VM lookup.
   Portrait assignment also defers through `DelayUntilNextTick`. Neither finding
   confirms the live cause. Stored anchors do not certify visible pixels.
4. Fix and verify the identified lifecycle/binding cause before repeatedly
   recreating encounters. Stop on bad UI, unavailable evidence or uncertainty.
5. Only then implement and independently verify post-action rollback of resources,
   consumables, EXP/MRP, counters, steals/drops, map caches, effects and RNG.
   Do not reinterpret acted history as untouched to pass existing guards.

The native pre-battle backup is partial. Its retry body copies unit rows but does
not visibly apply that copy in the reviewed body; HP/MP rollback is unproven.
The identified game-over retry route consumes an item and must not be called a
free restart. Ordinary-load binding stays excluded because its external callbacks,
including platform achievement reconciliation, remain unreviewed.

## Save states and other unfinished work

Mid-battle restore additionally needs verified scheduler/turn/AI state, queued
jobs and targets, effects/status durations, stagger/chain/LB, spawned/dead units,
object ownership, event/world state and complete RNG handling. Raw UObject memory
or matching ordinary saves cannot substitute for these lifecycles.

**Saving anywhere remains last and unverified.** Installer automation and an
always-visible “FFR Helper Active” indicator are not implemented. An indicator
would disclose ordinary use, not establish cheat-proof speedrun verification.

## Reproduce and contribute

Use the [README](../README.md) for Python setup and the native [build guide](../native/README.md).
Portable C++ tests can run without a game. Windows DLL/fixture builds need MSVC,
the Windows SDK, CMake and checksum-pinned MinHook source fetched by the included
tool. UE4SS must be downloaded separately from the exact official link in the README.

When changing the native mock host or fingerprint-dependent code, rebuild **all**
fixture variants/profiles before the full CTest suite. Selective builds can leave
old fingerprints in sibling DLLs. Keep production DLLs, fixture DLLs and the
read-only observer/mutation-capable practice component separate.

CI is manual-only and repository Actions are disabled at publication. No paid
runner or paid service is needed to obtain the source or run local tests.
Never commit personal saves, real captures, process addresses, downloaded engine
or loader binaries, credentials, build directories or locally generated logs.

Detailed historical findings and acceptance criteria remain in
[NATIVE_STATE.md](NATIVE_STATE.md), [NATIVE_BACKUP.md](NATIVE_BACKUP.md) and the
native protocol documents. Historical hashes may predate author anonymization;
private evidence paths are references to the original development session.

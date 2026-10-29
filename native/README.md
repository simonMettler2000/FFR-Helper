# Experimental native encounter-entry observer

**Development component, not installed by the desktop app.** This module has
been verified across ordinary FFR encounters on the exact target build, including
two updated captures on 29 October after correcting the loader constructor lookup.
It does not restart battles or restore save states.

The tested ordinary encounter bypassed reflected opening hooks. This prototype
instead observes the native map manager's encounter entry. It deep-copies all
known `BATTLE_ENTRY_DATA` fields before forwarding the original call exactly once.
At that same boundary it calls the verified engine in-memory serializer on the
current progression object, copies the resulting GVAS bytes into its own storage,
and frees the temporary buffer through the game's allocator. Resource/progression
coverage remains incomplete; partial RNG seeds do not represent full combat state. The live
test verified serialized progression changes after fleeing and collecting victory
rewards, with independent archives and no capture failures.

The newer development source also captures an optional **partial field anchor**:
current map ID, live player-root position and map-operation direction. Exact
getter profiles and same-map checks gate the reads. Its detached metadata was
captured in the 29 October test; independent position comparison remains pending.
It does not capture camera,
streaming, actors, events or a complete world state, and cannot restore a position.

New openings can also include an optional **partial playtime basis**: the saved
float, live clock start, ongoing suspension start, completed suspension total and
a sample from the game's seconds source. Profiled read-only getters and coherent
same-root/clock checks gate capture. The elapsed definition subtracts completed
suspension only; an ongoing suspension is recorded separately. The consumer also
checks the saved float against the serialized progression. Invalid clock evidence
becomes null without suppressing the opening. The 29 October captures verified
typed metadata and saved-float correspondence; comparison with the displayed
clock and suspension lifecycle remains pending. It does not reset, restore or
advance playtime.
Stable native-thread MXCSR rounding control is included when available. The
offline normalization planner requires known nearest-even rounding and refuses
ongoing suspension; it never applies a clock change or overwrites the opening.

## Bounds and compatibility

- Requires Windows x64 and exact SHA-256 matches for game build 24250053 and
  UE4SS `v3.0.1-1183-gcdbc0b66`. Other loader builds are refused before host-object
  construction; other game builds produce an unavailable native-capture status.
- Checks the encounter entry, progression getter, serializer and allocator's
  loaded 64-byte code fingerprints before hooking.
  No signature scan or guessed fallback is used.
- Uses a MinHook detour to observe one native entry point. This changes code
  routing; it does **not** write gameplay values. Installation is opt-in.
- Reads input through bounded `ReadProcessMemory` calls within the game process.
  Arrays, nested passives, strings, flags and floating-point values are validated.
- Retains only detached vectors/UTF-8 strings. No engine pointer is exported or
  retained in a captured encounter.
- Defaults to no collection. The independent desktop lease expires within eight
  seconds. It has no mutation, restore, console or arbitrary-code command.
- Keeps at most 32 events and 384 KiB of queued JSON; reports are bounded to
  512 KiB and explicitly `restorable: false`.
- Retains only the latest opening progression, bounded to 16 MiB. The worker
  writes a fixed GVAS sidecar before publishing its size/hash/revision in a report.
  A new/invalid entry invalidates the previous opening. All capture happens
  synchronously at native entry; disk writes happen on the observer worker.
- Field-anchor failure is separate from progression failure. Non-finite/out-of-range
  coordinates, unavailable player components or a changed context produce a null
  anchor. No game object pointer is exported in this metadata.
- The original call still executes if capture fails or throws. Observation never
  changes the original call's arguments or return/exception behavior.
- The module is pinned after enabling its detour. On unload, the detour is
  disabled and its trampoline retained until process exit to protect in-flight
  calls. Restart the game for another native-module load; hot reload is unsupported.

The UE4SS host object is opaque: its constructor and non-deleting destructor are
resolved from the exact verified DLL exports/RVAs. That DLL constructs and
destroys its own object in aligned storage. The module does not transcribe or
replace an SDK vtable, inspect STL members, or pass STL objects across the DLL
boundary. The pinned public host definition uses 192 bytes; 4096 bytes are
reserved. A null factory result is supported by the pinned UE4SS loader.

## Build

MSVC, Windows SDK, Python and CMake are needed for the DLL. The portable copying
core and its tests build with a C++20 compiler on other platforms too.

```sh
python tools/fetch_native_dependencies.py
cmake -S native -B local/native-build -A x64 -DFFR_BUILD_NATIVE_OBSERVER=ON -DMINHOOK_SOURCE_DIR=<absolute-path>/local/native-deps/minhook-1.3.4
cmake --build local/native-build --config Release
ctest --test-dir local/native-build -C Release --output-on-failure
```

The dependency fetch validates the pinned official source archive and refuses
to overwrite altered cached files. No game or loader binaries are distributed.

The DLL is `local/native-build/Release/main.dll`. It is a development recorder;
scripted/preemptive encounter coverage and teardown still need verification.
An opted-in installation uses a separate `FFRHelperRuntimeNative/dlls/main.dll`
directory, an existing `connection/` directory, and an enabled marker under UE4SS
Mods. The approved local test installation used that layout. The desktop
Prepare control installs the Lua recorder only. Connect accepts either recorder
folder and selects the matching protocol identity. The native DLL must be
installed separately for an opted-in test.

The private collector supports the separate producer identity:

```sh
python tools/observe_runtime.py <FFRHelperRuntimeNative-folder> --native --output local/native-observations --duration 600 --stop-file local/stop-native
```

This module does not enable or depend on Saving anywhere.

The next uninstalled source update also provides optional decision-handler entry
history in `observer-status.json`. An exact full-function fingerprint gates a
read-only detour of the native `OnExecuteDecideCommand` target. Counts start at
native encounter entry and remain separate from the immutable opening. Lease
gaps/session changes invalidate history; foreign-thread calls and overflow are
flagged. The collector preserves validated changes in `.ffrdecision.json` files.
These count handler invocations, including rejected/null calls, rather than
completed commands. Coverage and fresh game-thread verification remain false;
this telemetry cannot authorize a retry or practice operation. It has not been
installed or tested in FFR.

The ordinary recorder also provides an independent optional `menuObservation`
from the actual battle-manager tick, with its own sample timestamp. Exact code
profiles, encounter-thread/world matching and bounded reads gate UI/selection,
unit/job counts and raw turn-beginning counts. Sampling is limited to ten times
per second and preserves the original tick's arguments and Win32 error state.
The collector stores `.ffrmenu.json` evidence; worker heartbeats cannot refresh an
old game-frame timestamp. Invalid context or unavailable code makes this optional
component null. It is excluded from the practice build and cannot enable a
practice command. Coverage remains unreviewed and non-atomic; actual FFR behavior
still needs verification.

A separate [opening-abort trial](ABORT_TRIAL.md) is compiled only when explicitly
requested. Its game-thread command path is excluded from the ordinary observer.
It is a mutation-capable experiment. Its first-menu import guard refused before
any request in FFR; no import, abort or restoration operation ran. The FFR source
profile disables practice dispatch pending boundary/freshness review, and the
installed practice module is disabled. It does not yet perform restart or
save-state restoration.

The desktop or CLI verifies the sidecar against the matched report, parses its
GVAS class/engine/property layout and exports a `.ffropening` archive. The archive
contains the detached encounter setup, serialized progression and explicit
missing-state metadata. It cannot be loaded as a game save or ordinary checkpoint.
It is not a battle restart or a restorable save state.

Optional opening `mapUnitCache` metadata copies bounded raw components from the
map manager's separately cached unit parameters. Storage/list identities and a
profiled storage vtable are checked; failure leaves null metadata without
suppressing progression. It preserves duplicate IDs as distinct indices and is
explicitly not coherent with serialized progression or restorable. This helps
investigate cache/resource coverage before any root rebinding. No cache setter,
refresh or clear function is called. The 29 October test verified detached cache
metadata and matching saved HP-base/MP fields; full resource coherence remains
unverified.
Optional `crtRandState` reads only the existing encounter thread's Windows CRT
seed through exact DLL/code/import and FLS-layout checks. It never advances or
sets that RNG, calls its allocating PTD getter or exports addresses. Stable
samples around progression copying are required; unavailable/changed state
becomes null independently. Other threads and RNG sources are uncovered, and
restoration is not implemented. Portable, compiled-fixture and supported-system
helper-process tests alone do not establish live FFR coverage. The 29 October
test captured this seed and preserved the opening after a command; actual stream
use and complete coverage remain unverified. No Windows system DLL is included
in packages.
Independent optional `engineGlobalRngState` copies one separate engine-global
seed after exact loaded draw/initializer code checks. Neither function is
called; no setter or thread suspension is used. Two reads per sample and
matching samples around progression copying reject observed drift. The metadata
explicitly declines atomic/concurrent/full RNG coverage, and unknown or changed
code leaves null metadata while retaining progression. The real DLL's dummy
process tests serialization-time seed drift and a late code mismatch without
invoking its draw/initializer. The 29 October test captured this seed alongside
the CRT state. Actual combat/thread use and concurrent-draw exclusion remain
unverified; matching endpoints are not an atomic snapshot.
When a large retained event history needs truncation for a bounded report,
`eventsOmittedForSize` states how many oldest events were omitted from that report.

## Verification and packaging

The native tests cover the bounded copying/UTF-16/JSON core and run the real DLL
in an isolated Windows fixture executable with a mock host DLL. They verify
actual detour capture, detached data, exactly-once forwarding, preserved arguments
and Win32 error state, malformed-input transparency, lease expiry, code restoration
on unload, game-allocator cleanup on failed capture, binary/report hash binding,
and rejection of mismatched game/loader/serializer profiles. This fixture is not
FFR and does not establish live game compatibility.

The separate live test produced three validated opening archives. The original
remained byte-identical after a user command and subsequent encounters. Escape
and win counters, resources and credited EXP/MRP changes were observed in later
openings. The auxiliary Lua recorder failed UE4SS engine-pattern initialization
on that launch; the independent native capture remained functional. See the
[live findings](../docs/NATIVE_STATE.md). Restore/abort operations were not tested.

CI also builds the portable copying core on Linux and macOS. The development
package tool accepts only production-profile DLLs, includes the required licenses,
and excludes all mock/test binaries. Packages omit `enabled.txt` so extracting a
development archive alone does not opt into running it.

```sh
python tools/package_native_observer.py --dll local/native-build/Release/main.dll
```

## Offline component comparison

For offline component validation, `tools/compare_openings.py` compares two
`.ffropening` archives with complete entry-field checks, full progression bytes,
optional normalized playtime and partial field anchors. It can export only a
non-restorable `.ffrcomparison.json` receipt. Legacy clock metadata is not
inferred, and equality never verifies an actual restart or complete state.

## Source references

- [Exact UE4SS host definition](https://github.com/UE4SS-RE/RE-UE4SS/blob/cdbc0b66/UE4SS/include/Mod/CppUserModBase.hpp).
- [Pinned loader factory/lifecycle](https://github.com/UE4SS-RE/RE-UE4SS/blob/cdbc0b66/UE4SS/src/Mod/CppMod.cpp).
- [UE4SS explanation of direct-call hook bypass](https://github.com/UE4SS-RE/RE-UE4SS/issues/626).
- [MinHook](https://github.com/TsudaKageyu/minhook), licensed under BSD-2-Clause;
  its license must accompany binary distribution.

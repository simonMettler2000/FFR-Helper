# FFR Helper — work in progress

An unfinished practice save editor and runtime research project for
**Final Fantasy Resonance**, intended for speedrunners and route planners.
The current development source is **0.2.0.dev0**. Community contributors are
welcome to continue the work; full battle restart and save states are unfinished.

> **Experimental — not fully tested.** This software can crash or hang the game,
> corrupt saves, or disrupt progression. Keep independent backups and use
> disposable practice copies. Use it at your own risk. It is provided **AS IS**,
> without warranty. To the extent permitted by applicable law, the authors and
> contributors accept no liability for loss or damage arising from its use.
> See the [MIT license](LICENSE) for the full warranty and liability terms.

## Downloads

- [Download the latest WIP source ZIP](https://github.com/simonMettler2000/FFR-Helper/archive/refs/heads/main.zip)
  or clone this repository. This includes the latest research code.
- [Legacy 0.1.0 alpha desktop downloads](https://github.com/simonMettler2000/FFR-Helper/releases/tag/v0.1.0-alpha.1)
  are older Windows/Linux/macOS builds of the offline editor. They do **not**
  include the latest native research. They are experimental, not a tested stable release.
- [Official UE4SS releases](https://github.com/UE4SS-RE/RE-UE4SS/releases)
  are provided by the upstream project. UE4SS is **not included** in this repository
  or the helper's packages. The offline save editor does not require it.

For this exact native prototype, use the independently downloaded
[UE4SS v3.0.1-1183-gcdbc0b66 ZIP](https://github.com/UE4SS-RE/RE-UE4SS/releases/download/experimental/UE4SS_v3.0.1-1183-gcdbc0b66.zip)
from the [official experimental archive](https://github.com/UE4SS-RE/RE-UE4SS/releases/tag/experimental).
That ZIP contains the upstream loader/DLL files. Its SHA-256 is:

```text
e4a47e23e2eeb8379c4fc388d1b1686f01c6c5930d84a2ee4a6a9574b274e681
```

The native component accepts that exact loader build and English Steam game
build **24250053 / Unreal 5.6.1**. A newer UE4SS or game version is not automatically
compatible. Read [the exact-build setup and removal notes](bridge/ue4ss/README.md)
before installing anything. Do not download replacement game DLLs from this project.

## What is implemented

| Area | Current state |
| --- | --- |
| Offline save editor | Open/losslessly edit supported encrypted slots; export copies with backups before replacement. Tested on limited saves, not every game state. |
| EXP / MRP routing | Set total character EXP and per-vision/per-character MRP; forecast progression and apply route totals. |
| Inventory / equipment | Add/remove items, change quantities and compatible owned equipment; edit gil and selected player counters. |
| Named checkpoints | Restore ordinary game-save progression/field position through normal loading. These are not emulator save states. |
| Read-only runtime capture | Limited supervised Windows testing on the exact build; exports partial non-restorable observations. |
| Untouched opening recreation | One supervised same-setup recreation reached a responsive menu. Party timeline portraits were misplaced; combat impact is unknown. |
| Repeated untouched cycles | Guarded rearm, fresh shortcut edges and complete-party diagnostic gates pass isolated fixtures. Not verified repeatedly in FFR. |
| Restart after taking actions | **Unfinished.** Resource/progression/effect rollback is not implemented or verified. |
| Mid-battle / full-world save states | **Unavailable.** Scheduler, AI, object lifetimes, effects and complete RNG restoration remain unresolved. |
| Saving anywhere | Deferred; a separate optional field-save experiment is not live verified. Do not treat it as an available supported feature. |

The desktop save editor/UI is cross-platform. Native game hooks are a separate
Windows x64 prototype; Linux/macOS game-hook compatibility is unverified.
Later item/character choices have a chapter reveal limit (Chapter 3 by default);
owned items and visions remain visible because the opened save already contains them.

The latest offline checkpoint ran **272 Python tests: 271 passed, one skipped**.
The current native runtime passed **128 isolated tests**, and its compiled
three-cycle fixture was rerun with the stricter client party gate. Automated
fixtures do not establish safe live restoration. No new live test accompanied
the public WIP publication.

## For developers

Start with [DEVELOPMENT_HANDOFF.md](docs/DEVELOPMENT_HANDOFF.md) for the remaining
work and [NATIVE_STATE.md](docs/NATIVE_STATE.md) for detailed historical evidence.
The [native recorder](native/README.md) and separate
[practice experiment](native/ENCOUNTER_RECREATION.md) have different capabilities.
The practice DLL can mutate game state; it is opt-in and excluded from the
ordinary observer. No native component is installed automatically by the app.

Personal saves, raw live captures, process addresses, downloaded dependencies and
local research/build outputs are intentionally excluded from Git. The public
source contains synthetic fixtures and the reusable implementation. Some old
research notes reference pre-anonymization commit hashes or private evidence
paths; those are historical context, not files required to run the offline editor.

## Use

1. Open **FFR Helper** and choose a detected `GameSaveData` slot, or browse for it.
2. Edit the working copy. No source file is changed while editing.
3. Use **Export edited copy** to create a separate practice save, or close the
   game and use **Write to selected slot**. A verified backup is created first.
4. Load that slot in the game.

In **Runtime research**, **Inspect saved capture** opens a `.ffropening` diagnostic
offline and summarizes which partial components are present. It checks the
archive and progression first, hides encounter IDs and save values, and never
connects to the game or changes the editor's working save. Missing legacy
components stay unavailable; a capture is not a restorable save state.

To restore a checkpoint, open it, use **Use selected as write target** to
choose a detected slot without replacing the working checkpoint, then write
and load that slot. Steam Cloud may synchronize replaced practice slots; the
helper's own checkpoints remain outside the game's Cloud-synced folder.

Save location on Windows:

```text
%LOCALAPPDATA%/FFRS/Steam/<Steam ID>/GameSaveData*.sav
```

For Proton or other game environments, browse to the corresponding folder.
The app stores its own checkpoints/settings separately under the user's local
application-data directory, in `FFR Helper`.

## EXP and MRP for routing

Enter **total accumulated points**, not the amount remaining until the next
level. Both character and vision curves use the game's incremental costs.
The next-battle reward inputs are forecasts; the Set buttons do not add them
to the stored total. Set the amount your route has **before** the battle and
let the game award that battle's rewards normally.

Character levels are derived from EXP when the game loads a save, so an edit
can affect the level immediately on loading. This version does not postpone
the change specifically until the next victory screen. Seeds and other saved
permanent bonuses are preserved. It does not invent or randomly reroll level
stats. See [format notes](docs/FORMAT.md) for the exact supported representation.

Route reward entries mean amounts **credited to the selected recipient**, after
bonuses/participation rules. The tool does not assume identical EXP/MRP rewards
for every character or vision. Mastery forecasts display native tier indices.

## Development

Contributions are welcome; see [the development handoff](docs/DEVELOPMENT_HANDOFF.md).
Development is intermittent and no support or release timetable is promised.
Automatic CI triggers are disabled; the included workflow is manual-only.
The repository's Actions are disabled at publication to avoid starting hosted
builds. Maintainers can deliberately enable and run them later.

UE4SS is a separate upstream download, not bundled here. Before changing that
policy, revisit the [redistribution notes](docs/REDISTRIBUTION.md).

Python 3.11+ is required. The packaged app does not require a Python installation.

```sh
python -m venv .venv
# Activate .venv using your platform's normal activation command.
python -m pip install -r requirements.txt
python run_helper.py
python -m unittest discover -s tests -v
```

Build on the target operating system:

```sh
python -m pip install -r requirements-build.txt
python tools/build.py
python tools/package.py
```

Optional Lua tests need `lupa==2.8`. The real-save test requires an explicit
`FFR_TEST_SAVE` environment variable pointing to a local save copy. Personal
saves, checkpoints and build outputs are excluded from Git. No personal saves
are included in the repository or packages.

The Linux build targets a modern desktop distribution and needs the usual
Qt/X11 or Wayland system libraries. The macOS package is an unsigned Intel
build; Apple Silicon users can run the Python source natively or use an
appropriate Intel compatibility environment. Runtime
support must be tested on the actual target system; a successful build alone
does not establish game-hook compatibility.

## Data and credits

Inspected against the installed English Steam build **24250053**, Unreal 5.6.1.
This is an unofficial fan tool; game names/data belong to their respective
owners. No game executable, game save, account credential or mod loader is
distributed. [Third-party notices](THIRD_PARTY_NOTICES.md).

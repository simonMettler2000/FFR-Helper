# FFR Helper

An offline desktop practice tool for **Final Fantasy Resonance**, built for
speedrunners and route planners. The stable alpha release is **0.1.0**; this
development branch is **0.2.0.dev0** and adds native-runtime research tools.

## What works in this version

- Open and losslessly edit the game's encrypted save slots.
- Set a character's **total EXP**, with a preview of the level and fixed base
  stats reached after the next battle's credited EXP.
- Set **total vision MRP for a specific vision/character pair**, with a mastery
  tier forecast. Awakening is separate and remains unchanged.
- Add, remove and change item quantities; select owned, compatible weapons,
  armour and accessories.
- Edit gil, successful escapes, battles won and successful steals.
- Capture named, verified checkpoints; restore them to ordinary save slots.
- Build and save route plans from credited EXP/MRP rewards and repeated
  encounters, then apply the calculated totals to a working save.
- Make automatic backups before replacing an existing save file. Replacing
  live slots is blocked while a detected game process is running or an
  AppContainer host prevents reliable process visibility.
- Keep later character/item choices behind a chapter reveal limit (Chapter 3
  by default). Owned items and visions are visible because they are already
  present in the opened save.

The Windows executable is built locally. GitHub Actions also builds Windows,
Linux and macOS packages; select a successful run in **Actions** and download
the matching artifact. The application's UI/save editor is cross-platform.
Actual game runtime/compatibility-layer support is a separate concern.

## Still experimental / unavailable

The development branch adds a **Runtime research** tab and a separate, read-only
[native recorder](bridge/FFRHelperRuntime/README.md) for the existing Steam
installation. It records observations, not restorable save states. See the
[verified findings and remaining acceptance requirements](docs/NATIVE_STATE.md).
The loader and recorder are opt-in; the app does not install a loader
automatically. Read-only observations have been verified through one live
overworld–battle–overworld cycle. Native restoration remains unimplemented.

An experimental [native entry capture](native/README.md) also deep-copies encounter
setup and serializes progression before the native encounter call. It has passed
an isolated Windows harness and a three-encounter live capture test, including
escape and victory EXP/MRP changes. This verifies capture, not restoration. The desktop can
connect its separate recorder folder and export verified `.ffropening` diagnostics;
these archives remain non-restorable. Native capture requires a separate opt-in
installation and never enables Saving anywhere.

Restart development now includes tested encounter reconstruction and a separate
[opening-abort experiment](native/ABORT_TRIAL.md). The experiment can request the
game's ForceEnd path once after a verified recovery write. It has passed dummy
process tests, but has not been installed/tested in FFR and does not restore or
restart battles. It is packaged separately and disabled by default.

Further development/testing of **Saving anywhere** is deferred until battle
restart, native save states and the other requested features are implemented,
unless the user requests otherwise. The existing field-save adapter is unchanged.

The **Saving anywhere** checkbox uses an optional UE4SS field-save adapter.
Its Lua logic and short-lived desktop lease are tested with a mocked engine;
it has **not yet been verified in a live FFR session**. A compatible loader is
required and is not installed automatically. See [the adapter instructions](bridge/README.md).

**Restarting a battle from its opening state is not implemented yet.** Its
control is disabled and labelled accordingly. Complete mid-battle/process
save states are also unavailable. Named checkpoints currently restore the
game's ordinary saved field position and progression through normal loading.

## Use

1. Open **FFR Helper** and choose a detected `GameSaveData` slot, or browse for it.
2. Edit the working copy. No source file is changed while editing.
3. Use **Export edited copy** to create a separate practice save, or close the
   game and use **Write to selected slot**. A verified backup is created first.
4. Load that slot in the game.

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

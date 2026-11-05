# Optional UE4SS prerequisite — exact-build research only

The offline editor needs no game modification. This prerequisite is for opted-in
Windows runtime research. It is not an installer or a working battle-restart feature.

Download upstream files yourself:

- [Official UE4SS releases](https://github.com/UE4SS-RE/RE-UE4SS/releases)
- [Exact compatible ZIP: v3.0.1-1183-gcdbc0b66](https://github.com/UE4SS-RE/RE-UE4SS/releases/download/experimental/UE4SS_v3.0.1-1183-gcdbc0b66.zip)
- [Official experimental archive](https://github.com/UE4SS-RE/RE-UE4SS/releases/tag/experimental)

Verify the ZIP SHA-256 before extracting:

```text
e4a47e23e2eeb8379c4fc388d1b1686f01c6c5930d84a2ee4a6a9574b274e681
```

The native profile expects the upstream `UE4SS.dll` SHA-256:

```text
c273e7228687b647226fd63d7ca65888d313ece6be18996ec1320d00a5efe1f8
```

The supported English game executable SHA-256 is:

```text
b8c588e46ebadbe37feca7bdf2da0ae63a8538099d14a53ec0c2241b086e5ffa
```

Do not substitute the current/latest upstream release or bypass a mismatching
profile. Compatibility with another build must be researched and tested separately.

## Manual research setup

1. Back up independent practice saves and fully close the game.
2. Follow upstream installation instructions for that exact ZIP. Its proxy
   `dwmapi.dll` sits beside `FFRS-Win64-Shipping.exe` in `FFRS/Binaries/Win64`;
   the remaining loader files are under `ue4ss/`. Keep a manifest of added files.
3. Only for the exact executable hash above, copy this folder's
   `UE4SS_Signatures/StaticConstructObject.lua` into
   `ue4ss/UE4SS_Signatures/StaticConstructObject.lua`. It supplies a symbol lookup
   missing during the tested loader initialization. A stock setup previously
   caused a load loop; a loader-only test with this lookup reached gameplay.
4. In the upstream settings, the tested configuration used
   `bUseUObjectArrayCache = false`, `ConsoleEnabled = 0`, `GuiConsoleEnabled = 0`
   and `GuiConsoleVisible = 0`. Do not bundle or overwrite another mod's settings.
5. First verify a loader-only launch on a disposable practice save. Then follow
   the [native recorder instructions](../../native/README.md) for a separately
   built read-only module. Do not enable the mutation-capable practice experiment
   merely because the loader or recorder works.

This source does not supply UE4SS binaries, auto-enable mods or install Saving
anywhere. The constructor lookup was tested on one exact build; that is not a
general compatibility guarantee. Review [redistribution notes](../../docs/REDISTRIBUTION.md)
before changing the separate-download policy.

## Disable or uninstall

Fully close FFR first. Remove only files recorded as added by your installation.

- Remove the helper's own `Mods/FFRHelperRuntime`, `Mods/FFRHelperRuntimeNative`
  and `Mods/FFRHelperBattlePractice` directories if you installed them. Remove
  `Mods/FFRHelper` only if you explicitly installed that separate field-save adapter.
- If UE4SS was installed solely for this helper, move the added proxy and entire
  `ue4ss` directory outside the game installation. A renamed/disabled proxy is
  still an added file and can also be removed.
- If other mods share UE4SS, keep their loader and configuration; remove only the
  helper's entries/files. Do not delete game-shipped DLLs or unrelated mods.

Removing the loader/mods does not revert previously edited saves. Restore a
separate original save backup if that is needed. No save files should be deleted
or replaced merely to uninstall the helper.

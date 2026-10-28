# Read-only native runtime recorder

This separate adapter supports battle-restart/save-state research for the
existing Steam game. It is **not a save-state implementation**, and it has not
been verified in a live FFR session. It does not enable Saving anywhere.

Live testing is currently deferred. These instructions are for when testing
resumes; the app does not install anything automatically.

1. With the game closed, use an independently installed, compatible UE4SS
   loader. The loader is not bundled with FFR Helper.
2. In **Runtime research**, choose **Prepare recorder** and the loader's existing
   `Mods` directory. This creates `FFRHelperRuntime` alongside other mods; an
   existing directory is never overwritten.
3. When ready for testing, launch the game and connect that recorder directory.
   The app waits for a fresh matching heartbeat before enabling capture.
4. Observe a familiar field/battle, then choose **Capture diagnostic report**.
   **Export report** writes `.ffrruntime.json`, containing numeric encounter IDs
   and available runtime values. The app does not show encounter details itself.
5. Disconnect or close FFR Helper to stop observation. Its lease expires within
   eight seconds even if the app exits unexpectedly. No field-save lease is sent.

The recorder has no restore/mutation command, no network listener and no
arbitrary-code request channel. Hooks return nothing and never alter their
arguments. Reports list missing state and are explicitly non-restorable.

It records up to 32 recent notifications in the current desktop session. This
does not prove complete event coverage: native C++ calls may bypass reflected
hooks. Ambiguous subsystem instances are reported without choosing one.

Keep reports private if they contain encounters you do not want to spoil. No
personal report is included in source control or distributed packages.

See [the implementation/acceptance notes](../../docs/NATIVE_STATE.md).

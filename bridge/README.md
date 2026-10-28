# Optional field-save adapter — experimental

This component has **not been verified in a live game session**. It is separate
from the tested offline save editor. Battle-start restart is not implemented.

It requires a UE4SS build compatible with UE 5.6. The loader is not bundled,
downloaded or installed automatically. Use the project's official instructions:
[UE4SS](https://github.com/UE4SS-RE/RE-UE4SS).

1. Close the game. Install a compatible UE4SS loader if you choose to use it.
2. In FFR Helper's **Live practice** tab, choose **Prepare field-save adapter**
   and select that installation's `Mods` folder. This creates only a new
   `FFRHelper` mod directory and does not replace another mod or change the
   loader's global configuration. It creates an `enabled.txt` marker.
3. Launch the game. In the helper choose **Connect adapter folder**, selecting
   the installed `FFRHelper` directory.
4. Enable the experimental field-saving checkbox. In ordinary exploration,
   open the game's normal menu and check whether Save is available. All other
   native menu/event/battle restrictions remain in force.

The adapter finds the exact game-native `IsTouchSavePoint` reflected function
and overrides only that predicate while requested. No function address or
pointer is hard-coded. Its discovery is intentionally strict and fails closed
on an ambiguous match. This still needs live compatibility testing; a matching
function name alone does not prove every save-menu condition is satisfied.

The desktop app supplies a short-lived lease. Turning the checkbox off or
closing/crashing the app disables the override within at most eight seconds.
The mod does not write saves itself, alter story flags, restart encounters,
run arbitrary supplied code, or modify game binaries.

Communication stays in the mod's own `connection` directory. There is no
network listener. On Proton, select the same mod directory from the host app;
Wine and the host then access the same files without translating AppData paths.


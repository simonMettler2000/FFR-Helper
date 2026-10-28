# Supported save format

Inspected against the installed English Steam build **24250053**, Unreal **5.6.1**.

The game wraps GVAS data in an SVHD envelope. The 32-byte header contains its
magic, two 16-bit version fields, a SHA-1 digest of the encrypted payload, and a
reserved 32-bit field. The payload uses the game's AES-256 ECB save-format key
from `DefaultSaveGameUtility.ini`. This is a game-format constant, not a user
account credential. The plaintext has an ordinary GVAS v3 header, complete UE5
property-type descriptors, a serialization-control byte, tagged properties,
the root `None` marker, a zero u32, and 1–16 zero alignment bytes.

The reader preserves all unedited property headers and values. Arrays and
containers are decoded only when needed; opaque/native structures and unknown
fields are retained. Unsupported extensions and invalid checksums are rejected.
An unchanged real save must encode to exactly its original encrypted bytes.
Before export, edited output must decode and re-encode without a byte changing.

Editable data:

- Character `Experience`: total cumulative EXP. There is no independent saved
  level field in `SaveData_UnitParameter`. The game's level getter derives the
  level from EXP, subtracting each row's `RequiredExperience`. Permanent
  `baseParameterList` bonuses are preserved. Setting EXP in a file can affect
  the level as soon as that save is loaded; it does not defer the change until
  a victory animation. The forecast shows the level after an additional reward.
- Vision `synchroPoints`: map of character ID to total MRP for that specific
  vision. Its mastery costs are also incremental. `awakeningPoint` is a separate
  value and is not modified when setting MRP.
- Inventory `ItemId`/`Num`, equipped item IDs, and supported player counters.

The reference contains the game table's mastery **tier indices** (0–9). These
indices are used in forecasts rather than assuming a different display label.
Rewards in the route planner are amounts credited to the selected recipient,
after bonuses and participation rules; the tool does not guess EXP sharing.

Checkpoints capture complete normal saves and their saved field position.
They are not process snapshots or live combat save states. A separate in-game
adapter is required for field save restrictions and battle restart.

Format research also consulted the complete-type-name implementation in the
MIT-licensed [uesave project](https://github.com/trumank/uesave). FFR Helper's
bounded, selective editor is implemented separately in Python.

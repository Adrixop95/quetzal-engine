# Ownership changes in the original: mechanism and town capture (decompilation)

Evidence date: 2026-11-08. **V** = read from the Ghidra decompilation (function addresses only,
no code reproduced), **O** = confirmed by the headless observations in
`ownership_observations.md`, **G** = inference.

## The mechanism: an object's side follows its tactic

- **V** An object (unit or building) keeps a pointer to its tactic (group) at `+0x30` and its
  own side index at `+0x40`. The tactic carries its side in the side structure it points to
  (`tactic +8`, 0x69c bytes per side starting at `0x4dd2f4`).
- **V** `0x43ecf0` (assign tactic): decrements the reference counter of the old tactic, stores
  the new tactic, increments its counter and, when the tactic's side differs from the object's
  `+0x40`, calls the object's virtual `+0x7c` with the new side. That method (`0x43ed50`, set
  side) removes the object from the old side's object list (`0x449ac0`), adds it to the new
  side's list (`0x449ab0`) and writes `+0x40`.
- **V** The only normal route to assign a tactic is **Tactic::AddMember** (`0x454090`, virtual
  slot `0xd4` of the tactic class): it takes a reference, stores the object in the member list
  at `tactic +0xc` with its position, calls the assign function above, recomputes the group
  flag dword (`+0x50`, bit 0 and bit 2 from member virtuals `+0xa0` and `+0xa4`) and may set
  the leader (virtual `+0x78`). Its removal counterpart is `0x4506d0` (slot `0x10`), the tactic
  destructor `0x44f920` clears members through the same assign function.
- **V** Therefore **a side change is always the side effect of adding an object to a tactic of
  another side**. Sources of AddMember calls (27 sites in 24 functions): the scenario loaders for
  units, boats, Johnka and defence objects (`0x430930`, `0x42f560`, `0x42f6e0`, `0x430410`),
  group creation and reassignment commands (`0x448350`, `0x448520`, `0x448b40`, `0x448f60`),
  tactic internals (`0x44fa40`, `0x450710`, `0x452c70`, `0x454820`, `0x457000`), the build think
  step of the computer players (`0x459bb0`, new units join the town's tactic), the ferry think
  (`0x408eb0`, boarding and unloading), spawn (`0x45c930`, `0x409030`) and **town capture**
  (`0x45a920`). A script gift of a group (`giftToSide`) is parsed to command code `0x18` by
  `0x4382c0`; its executor was not found in this pass.

## Town capture

- **V** The check runs from the town tactic's think (`0x45b1e0`, debug label "CheckCapture")
  and the capture itself is `0x45a920` (debug label "CaptureTown"). Both operate on the town
  tactic; the town hall object is `tactic +0xf0`, its anchor tile comes from virtual `+0x94`.
- **V** Radius: **4 tiles, or 8 tiles when the town tactic flag at `+0x15c` is non-zero.**
  Objects within that radius (box search `0x46ff20`) are scanned.
- **V** A **capturer candidate** is a hostile object (its side is not allied with the town's
  side) that is **not airborne** and whose squared tile distance to the anchor is below 5 (about
  2 tiles). The last such object found is the candidate.
- **V** A **defender** is a live object of the town's own side within the radius that is not an
  ordinary unit group (building and defence objects count) or is a unit group that answers true to
  virtual `+0xfc` (armed and able to fight). If at least one defender is present the town is
  not captured.
- **V** If there is a candidate, no defender and the town's helper tactic at `+0x110` does not
  answer true to its `+0xfc`, `0x45a920` runs: every member of the town tactic is moved to a new
  tactic of the capturer's side (units with virtual `+0x7c` set side, others through AddMember),
  the town tactic's side pointer is replaced (`tactic +8`), the town hall's side is set, the side
  object lists are updated, the minimap and fog are refreshed and a capture voice line is
  played. A hostile non-airborne object near a human town also raises the "town under attack"
  alert with a cooldown counter (`+0x180`, limit 0x10).
- **O** Matches the headless observation: the mission 7 outpost flipped to the player's side
  two ticks after the last guard archer left the radius while player units stood about 84 to
  99 px away, and flipped back later when the guards returned and no player unit was near.
- **G** Which tactic flag sets the 8 tile radius (`+0x15c`) is not identified; towns with
  defence towers or larger types are the likely candidates. The delay between "no defender"
  and capture is zero ticks in the code path read (the candidate scan runs in the town tactic
  think, so the practical delay is the think interval).

## For the clone

Replace any scripted or ad hoc capture rule with the radius rule above (4 or 8 tiles, candidate
within about 2 tiles, no defender object or armed group of the town's side inside the radius),
and make a town's side change move every member of the town tactic to a new tactic of the
capturing side. Keep the alert counter and the voice line as presentation.

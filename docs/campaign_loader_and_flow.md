# Campaign missions: launcher, loader chain, per-mission structure, end flow

Read-only. Sources: `work/decomp/all_full.c`, objdump of `game/Aztec.exe`, private files in
`game/Campaign/Maps` (counts and ids only). V = read in code or data, G = inference, not run.
Builds on `campaign_menus_saves.md`, `interface_ingame.md`, `docs/formats.md`,
`docs/campaign_scripts.md`, `docs/script_conditions_flow.md`, `docs/custom_mission_skirmish.md`.
Raw per-mission dump (numbers only): `work/gaps/j1_missions_raw.txt`.

## 1. Launcher and loader chain

### 1.1 Request and dispatch (front-end thread -> game thread)

| Step | Function | What happens | Ev |
|---|---|---|---|
| Request | `0x428f30(path)` | copies path to `0x4c4b58`; increments stack top `0x4c4a14` unless top state is the campaign list (`0x429040`); `0x4c4cc0 = 1` (game running); SetEvent `0x4c4d6c`; SuspendThread front-end `0x4dd154` | V |
| Callers | Start Game on briefing `0x41ad40` (campaign), Accept Mission `0x41a2c0` (custom), Load This `0x41a2e0` (`single%i.sav`) | V |
| Dispatch | game-thread loop (one of the duplicated bodies near corpus line 143740, inside `0x458e40`) | on `0x429060()` true: if `0x429170()` (load flag, set by `0x429180(1)` in Load Game) is false -> new game, else load; then SP/MP by `0x43d210()` (`0x4cc38c` != 0 = network session) | V |

| Wrapper | Used for | Body | Ev |
|---|---|---|---|
| `0x458a40` | SP new (campaign and custom) | `0x43d220(1)`; `0x42ec90(path, 0, 0)`; loop `0x4577b0`; `0x430cb0`; `0x43d220(0)` | V |
| `0x458a70` | SP load | same with `0x42ec90(path, 0, 1)` | V |
| `0x458aa0` | MP new | `0x42ec90(path, 0x43d200(), 0)`; `0x4577b0`; `0x43ca30`; `0x430cb0` | V |
| `0x458ad0` | MP load | `0x42ec90(path, 0, 1)`; `0x4577b0`; `0x43ca30`; `0x430cb0` | V |

`0x43d220(1)` (SP only, V): player count `0x4cc244 = 1`; copies the profile name (`0x429190`) into
player record 0 (`0x4cc248 + 0x10`); `0x4cc394 = 1`; local player index `0x4cc240 = 0`.
`0x43d220(0)` clears `0x4cc394`. Computer predicate `0x43d2c0(i)`: record i (0x30 bytes) has an
empty name (V). So in SP side 0 is the only human; all other sides are computer.
`0x430cb0` is the post-mission teardown (G, not read).

### 1.2 `0x42ec90(path, localSide, isSave)`: file order (V, `LoadGame.cpp`/`loadmap.hpp` asserts)

| # | File / key | Loader | Initialises |
|---|---|---|---|
| 0 | save only | `0x4463e0(path, buf)` | reads the mission path out of the save header, then continues with that `.mp` |
| 1 | `<name>.mp` (ext `0x4ab25c`), `[Globals] MapName` (`0x4ac08c`) | GetPrivateProfileString | base name of terrain files (G: path composition not traced) |
| 2 | `.map` (`0x4ac084`) | `0x431b10` / reuse `0x431cf0` -> `0x515428` | tile grid; error "Can't open a map" |
| 3 | `.rm` (`0x4ac014`) | `0x42aec0` / `0x42af50` -> `0x4d9c18` | resource channels |
| 4 | `.dat` (`0x4abfec`) | kept as `0x4c4d10`; `0x4319e0`/`0x431c10` -> `0x4dd2f8` | passability; `0x46bb70`; music availability `0x43a1f0/0x43a210` |
| 5 | `.spr` (`0x4ac020`) | `0x47a840` -> `0x5113d4` (sprite type 0x44c) | minimap preview |
| 6 | `[Globals] Winds` (def 5, max 20) | `0x470a90` x n -> `0x4d9bbc[]` | wind objects |
| 7 | `TileSet` -> `0x4dd2e8`; `0x418380` (StartTime, daylight); `Music` else `MusicMask` (`0x43a230`/`0x43a210`) | | |
| 8 | new game only (isSave == 0), in this order: | | |
| 8a | `[Globals] Sides`, `[Side#]` | `0x42ffb0(mp, localSide)` | side array `0x4dd2f4` (0x69c each); per side `0x449440` (map size), Name, Race, Color, `StartMoney` (def 100) -> `0x4498f0`; `Neutral` -> `0x447710(-1)`; `DisabledID` `0x4306d0`; defences `0x430410`; cameras `0x430350`; then initial view from `[Side<localSide>] Start_X/Start_Y` (def 2, clamped to the map) |
| 8b | `[Town#]` | `0x430770` | towns and their preplaced objects |
| 8c | `[Team#]` | `0x430880` | team groups (tactics) |
| 8d | `[Boat#]` (type 0x54) | `0x42f560` | Side, X, Y, Up, ScenarioId, path via `0x430930` |
| 8e | `[Paths] Count, i=` | `0x42fc90` | path objects in script pool `0x4cd698` |
| 8f | `[Johnka#]` (type 0x58) | `0x42f6e0` | + `Enabled` |
| 8g | `[Submarine#]` | `0x42f8e0` | |
| 8h | `.scn` (`0x4abfa8`) | `0x4367f0` | triggers (parser as in script docs) |
| 8i | `.vis` (`0x4abfa0`) | `0x430ae0` | fog pages |
| 9 | save only | `0x445570(profileDir, path)` | full pool restore (see campaign_menus_saves.md sec. 3) |
| 10 | always | `0x470780` | post-load pass over `0x515ac4` objects (G: sprite/type binding) |

Notes:
- `.mit` is **not** read by the loader. Only Play Mission `0x419290` reads it (sections Order,
  Objectives, Diary, then briefing state 17). Custom and Load skip `.mit` and the briefing (V).
- `localSide` is pushed from the loader's second argument (V objdump `0x42f3c1` -> `0x4301d0`):
  0 in SP, `0x43d200()` (= `0x4cc240`) in MP new. MP load passes 0 (G: index restored later).
- Not read by the game (V formats.md, rechecked in `0x42ffb0`): `Difficulty`, `OnlyComp`,
  `Notes/Note#`, `Tendency`, `BuildSpeed`, `CurrentTeam`. No difficulty or speed parameter
  comes from the mission; game speed is the profile options block (campaign_menus_saves.md).
- Alliances: no mask key exists in `.mp`; `Neutral=1` sets the side's relation to -1 (all,
  `0x447710`), everything else is done by script (`setFriendlyTo`/`setEnemyTo`, usually in an
  `always` trigger).
- `Type` keys: `[Town#] Type` 0..3 (outpost, village, town, fortress) with Race picks the hall
  type from `0x4bc390`; there is no scenario-level Type key (V `0x430770` per formats.md).

## 2. The 15 campaign missions

The per mission tables (sides, races, starting forces, trigger structure) were read from the
private game data and are kept in the private notes, not in this public file. Summary of what
they showed (structure only): boats appear in missions 2, 5, 7 and 9; johnkas in mission 15;
submarines in missions 2, 9 and 15; the usual defeat condition is "local towns == 0 and units
== 0"; the usual victory is "enemies == 0" or a named side wiped out; mission 1 adds a timed
win, mission 2 a time limit, and missions 4 and 11 depend on the hero Abdula.

## 3. End of mission and return path

- Outcome: script `victory`/`defeat` (`0x401a60`/`0x401aa0`) set side `+0x140` once (2 playing,
  1 won, 0 lost). Each tick `0x4577b0` reads the local side (`0x43d200()`) and copies a non-2
  value to `0x515420` (`0x458226`) (V). **No code reads `0x515420` by absolute address** and the
  store does not branch out of the loop (V objdump), so the outcome alone does not end the
  mission (G: the player sees the result and leaves through the pause menu).
- Leaving: End Mission `0x419e00` = pop overlay; `0x428fa0`; pop again (V). `0x428fa0`: if no end
  overlay flag (`0x4c4cc1/cc3/cc4`, set by the modal helpers near `0x429400`..`0x429630`) is up,
  clears `0x4c4cc0`, else only clears those flags; then MP: `0x43cde0` (net notify), SP: SetEvent
  `0x4c4d6c` (V). The loop sees `0x429060() == 0` (`0x4587aa`) and goes to the tail `0x458850`.
- Tail: campaign win -> progress `0x43f560(max(m+1, progress))`, `final.avi` at >= 16; custom
  (state 6 on stack) -> `win.avi`/`lose.avi`; campaign defeat -> nothing (V, see
  campaign_menus_saves.md). Then `0x430cb0` teardown and `0x43d220(0)`.
- Return: the second pop of `0x419e00` undoes the stack increment of `0x428f30`, landing on the
  state that launched: briefing 17 (campaign), 6 (custom), 4 (load) (V for the arithmetic, G
  for what the user sees). Progress is written before return, so the campaign list rebuilt by
  the state-1 enter hook `0x41b790` shows the next mission (V).
- Custom Mission: state 6 enter hook `0x41b870`; list built by `0x43f620` in browser mode from
  `Maps/*.mp` (`[Globals] Name`, `MapSize`; `MaxPlayers` read only in the MP branch). Accept
  `0x41a2c0` -> `0x428f30(path)` -> `0x458a40`, the same SP path as the campaign, so the human is
  always side 0, there is no side or race picker, and `0x4dd164 = 0` (cleared on entering state 1),
  so no progress change (V).

## 4. Single player vs multiplayer start

| Item | SP (campaign/custom) | MP |
|---|---|---|
| Wrapper | `0x458a40` / `0x458a70` | `0x458aa0` / `0x458ad0`, then `0x43ca30` after the loop |
| Player table | `0x43d220(1)`: one record (profile name), count 1, local 0 | built by the lobby (`0x43c9b0` host start), local index `0x4cc240` from the session |
| Human sides | side 0 only | every side whose player record has a name (`0x43d2c0`) (V predicate, G lobby fill) |
| Initial camera | `[Side0] Start_X/Y` | `[Side<local>] Start_X/Y` (V) |
| Briefing / `.mit` | campaign only | none (G) |
| Movies | mission AVI, win/lose only in custom, final | none (G: `0x429020` false outside state 6) |
| Pause / overlays | `0x4577b0` shows states 14/21/5/15/8 only when `0x43d210()` is false | no overlay menus (V) |
| End | SetEvent | `0x43cde0` network notify (V) |

## 5. Unknowns

| Unknown | Needs live run? |
|---|---|
| Whether anything (UI or timer) ends the mission automatically after the local outcome leaves 2, and what is shown (score screen `0x41b4b0`?) | yes |
| Who sets end flags `0x4c4cc1/cc3/cc4` in the mission path and which roots `0x4c4988/98c/990` are | static, then live |
| Exact path composition of `.map/.rm/.dat/.spr` (MapName vs `.mp` base name) | static |
| Whether SP player records 1.. are cleared before `0x43d220(1)` (otherwise stale MP names could make a side human) | static |
| MP load local index (passes 0) and how `0x43ca30` / `0x430cb0` clean up | static |
| Whether the briefing state 17 is popped back to state 1 after a mission | yes |
| Initial alliance values before `always` triggers run (first tick) | yes |

# File formats

## Res/*.xbe (resource archives; misleading name, these are not Xbox files)
- `u32 N`, then N records of 16 B each: `u32 id, u32 table_offset, u32 lo, u32 hi`.
- A group contains elements with numbers `[lo, hi)`. The table at `table_offset` is `hi-lo` `u32` entries with start offsets (increasing).
- The end of an element is the next offset; the last element ends at the first offset of the next group (or at the end of the file).
- Verified on: Pal, light, Units, Tiles0, fnts, common, Snd (src/tools/xbe.py).
- Group types: 0x44c / 0x454 = images (element header starts with `00 00 ac e0`), 0x515 = animations, Snd 0x460 = WAV files, Pal 0x44f/0x450/0x452/0x453/0x479 = palettes and colour tables.

## Maps/*.map
- `u16 width, u16 height`, then `width*height` values of `u32`. Lower 16 bits = index of the base tile in `Tiles{0,1}.xbe` (type 0x44c, 48x48, range from 1000); upper 16 bits = overlay (index from group 0x454, e.g. 0x132d), 0 = none.
- Terrain drawn from above: tiles 48x48, palette 4, shading level 14 (src/tools/render_map.py).

## Sprite (elements of type 0x44c and 0x454 in archives)
Header: `u32 magic=0xE0AC0000, u16 w, u16 h, u8 flags(+8), u8 +9, u8 +10, u8 +11, u32 +12, i16 ox, i16 oy (+0x14)`, data from +0x18.
Flags (`Sprite::Draw`, FUN_0047a990):
- `0x01` raw bitmap, rows in order from +0x18 (`0x10` = 16-bit pixels, otherwise 8-bit palette indices; `0x04` = shaded variant).
- `0x02` RLE with 8-bit palette indices. From +0x18 an array of rows, 8 B each: `u32 ctrl_offset, u32 pixel_offset`. Control stream: bytes alternating skip/draw, until the sum >= w. Pixels are consecutive bytes = palette index. `0x04`/`0x08` = shaded/semi-transparent versions.
- `0x40` RLE with 16-bit pixels (RGB 565/555): control byte `<0x80` skip n, `0x80-0xBF` skip (n&0x3F), `>=0xC0` draw (n&0x3F) 16-bit pixels.
- The control stream may exceed the width by 1 (seen with w=288): clip to w.
Observed flags: 1, 5, 6, 10, 14, 17, 64.
- NOTE, correction for flag `0x40` (verified in the blitters `FUN_0047f0a0`/`FUN_0047f270` and by the image: cursors, the "Loading" text, fonts): this is NOT a 16-bit colour.
  Control byte `<0x80` skips n pixels; `0x80-0xBF` draws (n&0x3f) pixels, 1 byte each (palette index, full opacity); `>=0xC0` draws (n&0x3f) pixels, 2 bytes each
  (palette index, blend/alpha byte). Palette = header field +0xC (id in Pal.xbe). Decoder: `src/core/sprite_alpha.*` (`decode_alpha_sprite`). `src/core/sprite.cpp`
  decodes these sprites incorrectly (as 565 colour), which gives "outlines" instead of the image. Correct result: 42/448/743 sprites from common/fnts/Units.

## Pal.xbe (palettes)
- Group 0x44f = RGB555 tables (256 x u16), 0x450 = RGB565, 0x479 = 32-bit, 0x452/0x453 = 256x256 tables (blending/transparency).
- Palette element = palette number (index from 1). Size 512 B = one palette, 10240 B = 20 shading levels of 512 B each (level = `param*0x200`), other sizes are variants.
- Terrain tiles (Tiles0, flag 1, 48x48) have natural colours for palette 4 at shading level 14 (in the repo: src/tools/xbe.py + sprite.py). Palettes 5..17 are terrain themes.

## Mission files (`Maps/<name>.*`, verified on 63 maps)
- `.map`: see above. `.dat`: `w*h` bytes, passability map. Verified in `FUN_0046bdb0` (WaveSearch): 0 = blocked (water/edges; both movement classes 0),
  1 = passable only for the first movement class (in mission 4 it coincides with the tree overlays 4909-4913), 2 = passable for both classes. The third table (flight) = always passable.
- `.rm`: `u16 w, u16 h`, then `w*h` cells of `u16` = two bytes: channel A of resource (lower), channel B (upper). The value depends on the type of the base tile
  (1000-1099: 65/0, 1100-1199: 0/65, 1200-1299: 26/26, 0 = none). Read: `FUN_0042acd0` (weighted average of the channels with a 7x7 mask, weights from the type record +0xf0/+0xf4)
  and `FUN_0042ad90` (sums within radius r=4 for AI). The resource name for channels A/B is not confirmed.
- `.vis`: CORRECTION (verified in `FUN_00430ae0`, 49 files): `u16 number_of_pages`, then for each page `u16 w, u16 h, w*h bytes`. Byte = level of
  darkening/fog of the given page (0 = bright, lower = brighter; 152 = fully dark/unexplored, 96 = night level). Loader: `src/core/visfile.*`.
- `.spr`: single sprite (header 0xE0AC0000, flag 1) = map preview at 2 px per tile (180x120 for a 90x60 map), palette indices.
- `.mit`: briefing text in cp1250: sections `~Pamiętnik` (Diary), `~Rozkaz` (Order), `~Cele` (Goals), terminated by a `~~` line (loader: `visfile.hpp` parse_briefing; in the exe code the `.mit` file name was not found, loading not confirmed).
- `.scn`: mission script (text, CRLF). Blocks `trigger(N)` / `conditions` / `actions` / `end`. 63 files examined, 72 line patterns:
  conditions: `always`, `localTime|globalTime == N`, `timer(N) == N`, `side(S).units|towns|enemies == N`, `side(S).unitsInRect(x1,y1,x2,y2) > N`,
  `side(S).unitsOfID(UGI_ABDULA) < N`, `side(S).unitsOfUGIInRect(UGI_x,...)`, `side(S).tactic(N).valid|attacked|idle != N`, `side(S).defence(N).valid == N`,
  `side(S).isEnemy|isFriendly(N) != N`; actions: `trigger(N).enable|disable`, `printString("..")`, `printStringR("..")`, `side(S).victory|defeat`,
  `side(S).addMoney(N)`, `takeMoney(N)`, `setFriendlyTo|setEnemyTo|setNeutralTo(N)`, `cameraActivate(N,N)`, `centerView(x,y)`,
  `side(S).tactic(N).attackTown(N)|giftToSide(S)|setPath(N)|setCycledPath(N)|moveTo(x,y)|joinTactic(N)|kill`, `boat(N).activate`, `johnka(N).enable`,
  `submarine(N).go`, `timer(N).set(N)|start|kill`. `S` is the side number or `local`.
- `.mp`: INI (cp1250). Sections: `[Globals]` (Version, MapName, MapSize=w,h, Name/Name#, Note#/Noter# + Notes/Notesr (mission goals), StartTime, TileSet, Winds, MaxPlayers,
  AVI, Music, MusicMask, Sides, Towns, Teams, Boats, Johnkas, Submarines, CurrentTeam), `[Side#]` (Name, Race 0..2 = Rus/Aztecs/Chinese, Color, Neutral, OnlyComp,
  Difficulty, StartMoney, Start_X/Start_Y, DisabledID=list of types, Defence#=x,y,r?,.., DefenceCount, Camera#, CamerasCount), `[Town#]` (Side, Race, Type, Tendency,
  BuildSpeed, ScenarioID, X, Y, Units, Unit#), `[Team#]` (Side, ScenarioID, Units, Unit#), `[Boat#]`, `[Johnka#]`, `[Submarine#]`, `[Paths]` (Count, #=n,x,y,...), `[Sounds]`.
  Line `Unit#` = `x, y, type, health` (type = index 0..89 of the object type table; health e.g. 6000 for infantry, 17000 for heavy units).
- `Res/strbase.bds`: text string base (`src/tools/strbase.py`): `*STRING { *ID 0xHEX *ORDINAL n *LANGUAGE { *ENGLISH ".." *RUSSIAN ".." } }`, 237 strings,
  ordinal 1..238; the `*RUSSIAN` field actually contains Polish text (cp1250). Contains building names (ordinal 169-193), unit names (194-210) and script error messages.

## Animations (`Res/Units.xbe`, group 0x515)
- Element: `char magic[4]="Anim"`, `u32 version=3`, then an array of `u32` indices of sequence starts (animation states), number of used frames; the header is 0x208 bytes.
- After the header, an array of frames, 32 bytes each (`u32 sprite_id` (0 = none, numbering as in groups 0x44c/0x454 of this archive), several `u32`/`i32` fields
  (delay, x/y offset, markers 0/1/-1), at position +0x1c a time/flag value). Sequences: multiple sequences of 8 frames = 8 directions.
- The file has 416 slots, of which 112 are non-empty. Frame fields: checked statistically, meaning of fields 1-7 to be confirmed when drawing units.

## To be identified
Meaning of the animation frame fields, values of `.rm` and `.vis`, `Music/music.xbe` (RIFF WAV), `Keys.ini`, `*.plr` and `single%i.sav`.


In-game evidence for `.vis` (mission 4, RT_DUMP_DIR dump `shot` vs decoder): the game minimap (1 px = 1 tile, region 638,70 120x90) matches the layer of page 0 in 96.3% of pixels
(dark = 152, bright = 0; patches in the upper-left and lower-right corners with the same shapes). Evidence for `.dat`: in the render of mission 4 tiles, black rock tiles match value 0, tufts of
trees match value 1 (render `src/tools/render_map.py` + `.dat` overlay). `.spr` is 120x90 (1 px/tile, not 2) and has palette header 15; the image requires shading level > 0 (palette level 0 = black; level 10 gives correct colours).

## Mission script `.scn` (parser: `src/core/script.*`, 49/49 files parse)
Case-insensitive (`localtime`, `jointactic`, `Timer`), any whitespace, the end of file may have a 0x1A character. Error messages: `strbase.bds` 72-104 (e.g. 75 "Bad comparison operation").
```
trigger(N)  conditions <condition>*  actions <action>*  end
condition: always | localTime|globalTime CMP N | timer(N) CMP N
  | side(S).enemies|units|towns|tactics|money CMP N | side(S).unitsInRect(x1,y1,x2,y2) CMP N
  | side(S).unitsOfUGIInRect(UGI_x,x1,y1,x2,y2) CMP N | side(S).unitsOfId(UGI_x) CMP N
  | side(S).tactic(N).valid|attacked|idle|locked CMP N | side(S).defence(N).valid CMP N
  | side(S).isFriendly|isNeutral|isEnemy(S) CMP N          CMP: == != < <= > >=   S: number or `local`
action: trigger(N).enable|disable | boat(N).activate | johnka(N).enable|disable | submarine(N).go
  | timer(N).set(V)|start|stop|kill | centerView(x,y) | printString("EN") | printStringR("RU, cp1251")
  | side(S).victory|defeat|buildCommand(UGI,..)|moveCommand(x,y)|cameraActivate(i,v)|cameraDeactivate(i)
    |setFriendlyTo|setNeutralTo|setEnemyTo(S)|addMoney(N)|takeMoney(N)
  | side(S).tactic(N).attackTown(id)|moveTo(x,y)|joinTactic(N)|setPath|setInvertedPath|setCycledPath|setCycledInvertedPath(N)|lock|unlock|kill|giftToSide(S)
```
`printString` = English text, `printStringR` = Russian (cp1251). A string must not contain `"`. UGI names: `src/core/ugi.*` (90 types). Execution semantics (initial trigger enabled state, time units) NOT verified. sscanf results for `(S)`: `%[loca]s` = `local`.

## `.mp` - additions (from reading `LoadGame.cpp`: `FUN_0042ffb0`, `00430770`, `00430880`, `0042fc90`, `0042f560`...)
Keys not read by the game (editor): `Version, Remark, Notes/Note#/Noter#, Tendency, BuildSpeed, Difficulty, OnlyComp, CurrentTeam, EditorX/Y, EndPosition, [Sounds]`.
Read: Globals `Sides, Towns, Teams, Boats, Johnkas, Submarines, Winds` (number of winds, max 20), `TileSet` (0/1: choice of the daylight table), `StartTime` (0..7199, tick of the day;
time key table of 9 fields: time, 3x RGB double, darkness level 6..12, factor 1..3; tables 0x4a8f68 / 0x4a9338), `Music` (if !=0 the track number, otherwise `MusicMask` bitmask), `AVI`, `MaxPlayers` (lobby).
`[Side#]`: `Race, Color, Neutral, StartMoney(def 100), Start_X/Y, Name, DisabledID (list of UGI types), Defence# = scenarioId,x,y,type (or x,y,type), DefenceCount, Camera# = x,y,z, CamerasCount`.
`[Town#]`: `Side, X, Y, Race, Type` (0 outpost, 1 village, 2 town, 3 fortress; building type from table 0x4bc390[Type*3+Race], -1 for Aztecs/Type 3 = fallback to column 1), `ScenarioId, Unit#=x,y,type,hp, Units`.
`[Team#]`: `Side, ScenarioId, Unit#, Units`. `[Boat#]`: `Side,X,Y,Up,ScenarioId` (type 84). `[Johnka#]`: like a boat + `Enabled` (type 88). `[Submarine#]`: `Side,X,Y,ScenarioId`.
`[Paths]`: `Count`, `i = id, x,y, x,y, ...` (id used in `setPath`). `.mp` keys are case-insensitive (`ScenarioID` = `ScenarioId`).


## User files
**`*.plr`** (player profile, 300 B, `FUN_0043e...`/`FUN_0043f...`; loader/writer: `src/core/userfiles.*`): `+0x00 u8 flag`, `+0x01` player name (NUL, remaining bytes are stack garbage),
`+0x44 u32 = 1`, `+0x48 u32 = 0`, from `+0x4c` a 224 B block copied from the global settings (`0x515430`), which the original keeps in the registry
(`HKLM\Software\New Media Generation\The Aztec\...`). Block fields (offset relative to the block): `+0x08` Acknowledgement, `+0x0c` mode (0..3, meaning uncertain), `+0x94` SFXVolume,
`+0x98` MusicVolume, `+0x9c` SpeechVolume, `+0xa0` Frequency (0x2b11=11025, 0x5622=22050, other=44100), `+0xa4` DisplayMode (0..2), `+0xa8` ScrollSpeed, `+0xac` GameSpeed,
`+0xb0` Gamma (double), `+0xb8` ViewAll (byte), `+0xb9`=1, `+0xba` DefaultProvider. The profile is the directory `Saves/<name>/` plus the file `<timeGetTime>.plr` in the game directory
(write: `CreateFileA("%i.plr")`, search `*.plr`). File `109555.plr` has field `+0x44`=15 (older version/garbage).
**`Keys.ini`**: lines `Action key` (spaces/tabs), case-insensitive, the same action may have several keys. Actions (id): add 2, inv 3, filtertype 4, filtercondition 5,
fastscroll 6, viewall 7, temporary 8, center 9, putontop 12, garrison 13, send 14, friend 15, autorepair 20, repair 21, destroy 22, aggressive 23, defencive 24, normal 25,
directcontrol 26, lastcaller 27, toggleinterface 28, scrollleft/right/up/down 38-41, printscreen 45, cheatwin 52. Keys: DirectInput names (`a`-`z`, `f1`-`f12`, `lshift`, `rshift`,
`lcontrol`, `rcontrol`, `lalt`, `ralt`, `space`, `tab`, `enter`, `caps`, `pad0`-`pad9`, `gray+`, `home`, `left`... full table in `userfiles.cpp`, DIK codes). If the file is missing, the game uses the default table.
**`aztec_settings.ini`**: NOT an original file, only the recompiled runtime reads it (`runtime/settings.c`): `resolution=WxH`, `window_mode`, `smooth`, `video`, `mouse_speed`.
**`Saves/singleN.sav` / `multiN.sav`** (write `FUN_004450e0`, read `FUN_004455f0`): `+0 u32`, `+4 u32` (both 0), `+8 char[256]` path to the mission .mp, `+0x108 char[64]` description entered by the player,
`+0x148` SYSTEMTIME (8 x u16), from `+0x158` a raw state stream: view position (2 x u32), 5 x u32 globals (`0x4cc38c/0x4cc240/0x4cc244/0x4cc23c/0x4c5730`), then sections in order:
pool `0x4c1978`, pool `0x5156f8`, number of pages and Side objects (0x69c each, virtual method +4), pool `0x4d9b50`, units `0x4cc3d0`, `0x515a98`, `0x4c1298`, `0x4c4ce0`, `0x4cd698`, `0x4d9b88`;
each object serializes itself via its own virtual method. Only the header is decoded (`parse_save_header`). Two saves of mission 4 made about 25 s apart (RT_SCRIPT: Menu (626,10) -> "Save game" (250,190) -> slot selection
(y = 138+20*slot) -> Save (184,359); slot 1 = `single1.sav`): size 149924 vs 152428 B (objects are added), path in the header `D:\Maps\mission_4.mp`, and the first `u32` of the stream are:
view x,y (298,336 / 298,324), then `0,0,1,4`, then the game tick counter (55 -> 363, difference 308 = 25 s at 12 Hz). The rest (about 54 thousand differing bytes) is object state
(vtable and field pointers, some are raw pointers/garbage); field layout NOT identified. Typing a name via `key DIK` after clicking the slot did not work in my run (description empty).

## Sound (`src/core/audio.*`)
- `Res/Snd.xbe`: group `0x460` elements 1..2013 = WAV files (`sound id`); only 367 are non-empty (empty = same offset as the next one); mixed formats: 44100/22050 Hz, 8/16 bit, mono/stereo.
  Group `0x3e8` elements 1..432 = sound sets: array of `u32` WAV ids (275 entries, all point to existing WAVs); the game picks one of the set at random (`rand`) (`FUN_00401480`,
  `FUN_00401250`: count = size/4). The set id is field [2] of an animation frame (docs/sprites_mapping.md: sound played on the first tick of the frame) and other calls.
  Group `0x6` elements 1202..1731 = texts (cp1250, NUL) of the speech sound captions; displayed when the sound cannot be played (`FUN_004016b0`).
- Positioning: pan = (x - view centre) clipped to +-4000; attenuation by distance from the view centre: <500 px none, 500..800 linear down to -4000, further muted; a positional sound plays only
  when the fog cell value is >= 0x21 (visible to the player). Type 4 = confirmations ("Acknowledgement" in the options), type 0 = global UI.
- `Music/music.xbe` and `Music_cd2.xbe`: one group `0x460` (WAV 44100 Hz stereo 16 bit, 20-32 MB); numbers 1..12; zero-length elements are on the second disc.
  Track mask: bit (n-1) = track n (`FUN_0043a160` checks which exist). `Music=N` in .mp (N != 0) selects exactly track N; otherwise `MusicMask & available`
  (62463 = tracks 1-10 and 13-16). Choice of the next track (`FUN_0043a280`, verified in the code): cursor `0..31` (start 0); when no track plays and mask != 0, it takes the first set bit
  from the cursor (cyclically), plays track `cursor+1` (`FUN_00475010`) and increments the cursor. So the tracks from the mask play in order in a loop, without randomization.

## Interface, fonts, light, palettes
- `Res/light.xbe` group `0x455` (1..71): light masks `u16 w=20, u16 h=20, 400 bytes` of darkness (0 = bright, max 152 = full darkness). Elements 1..16 have increasing strength (max 10..152),
  17..63 empty, 64..71 maximum 152. `FUN_0047d070` applies the mask to the fog layer of the page with a min operation (from the position in tiles). Value 152 = 19*8, so the palette shading level
  is the function `fog byte >> 3` (0..19 = 20 levels of Pal.xbe), but the direction is reversed: palette level 0 = black, higher = brighter (natural tiles about 14), so brightness ~ `19 - (byte>>3)`; not confirmed by a dump.
- `Res/fnts.xbe`: 5 fonts every 300 ids (`FUN_00463...`, `FUN_0047d7f0`): base 0 (in code 1), 300 (digits only 48-57), 600, 900, 1200; glyph of character `c` (cp1250 byte) = `base + c`.
  All use palette 53. Fonts 600/900 (flag 5, raw 8-bit, index 0 = background) and 1200 (flag 0x40) confirmed by image (Polish characters). Base 0 (flag 0x40, small glyphs 4..16 px)
  is also a correct cp1250 font (verified with a grid of all glyphs 33..255: 138=Š, 140=Ś, 163=Ł, 165=Ą...); earlier "garbage" came from an error in my test, not from the format.
- `Res/common.xbe` group `0x44c`: cursors 0 (sword, 12 frames 48x48), 2 (bomb, 12 fr.), 3 (red/white, 9 fr.), 4 (mace 36x36), 5 (hammers, 12 fr. 40x40), 6-13 (scroll arrows
  blocked, red X; hotspot from ox/oy, order N,NW,W,SW,S,SE,E,NE), 14-20 (scroll arrows). Interface: 256 "Start Game", 257 buttons (frame stack),
  258-263 health arcs (green/yellow/red, 6 sizes), 264/270/285/290 journal backgrounds (800x600), 266 coins, 267 bars top/bottom, 273 map, 277-279 icons (mail, no gold, pause),
  280/284 background screens, 281 text "Loading...", 282 Menu button, 283 strips, 289/291 scrolls, 292-298 stone panels, 299 tick, 300 lamp stack, 308-317 selection ellipses (10 sizes x 4 frames),
  318 marker, 319/322-324 bars, 1000-1002 parchment boards, 1536-1538 bars. Palettes: cursors 1, backgrounds 55, texts 48-50, 56-57. Group `0x454` ids 128..135: 8 sprites 48x576 (team colour versions).
- Sprite palette = header field +0xC. Tiles: Tiles0 (TileSet=0) palette 4-7 and 13, Tiles1 (TileSet=1) palette 8-11 and 13. Units: id<4 = team palette
  (`id + 0x10 + colour*4`, docs/sprites_mapping.md), 14, 51, 52, 54. Pal.xbe: ids 1..57, groups 0x44f/0x450/0x479 (palettes), 0x452/0x453 (blend tables).


## Video (`Video/*.avi`)
- All 11 files: Indeo Video 4 (`IV41`), 576x420, 12 fps, PCM audio 22050 Hz (mono; `win.avi`/`lose.avi` stereo). The runtime plays them through an external `ffmpeg` process
  (`runtime/video.c`: separate video pipes `-f rawvideo -pix_fmt bgra` and audio `s16le 22050 mono`, pace from `us_per_frame`, Esc/Space/Enter/mouse interrupts).
- When: `intro.avi` at game start (`FUN_0042d300`: directory from the registry `Video` + `intro.avi`, playback `FUN_00418e90`); key `AVI=` in `[Globals]` of the campaign map plays when the mission is loaded (`FUN_00419110`: video directory + name): mission_1 -> `mission1.avi`, Mission_4 -> `Mission4.avi`, mission_5 -> `mission5.avi`, Mission_10 -> `mission9.avi`, Mission_12 -> `Mission11.avi`,
  Mission_13 -> `Mission12.avi`, Mission_14 -> `Mission13.avi`, the others empty (multiplayer maps and tutorials are also empty); after the mission `win.avi` (victory) or `lose.avi` (`FUN_00458e40`);
  after the 16th campaign mission (counter >= 0x10) `final.avi`. The function `0x4191e0` and nearby, at start-up, opens all Video files (disc presence check, XOR 0x55); it is not the film selection.
  (Reference for the intro function: call in line 17333 of all.c.)

## Cursors and interface (selection of sprites from `common.xbe`)
- Cursor selection: `FUN_00464290` -> sprite id from `common.xbe` 0x44c: outside the game 4 (mace), with a modal window 1 (quill); when scrolling at the edge (`DAT_005159e0` = direction) the table `0x4bc904`
  (4,4,4,4,16,20,4,4,14,4,18,4,15,21,17,19) -> 14 N, 15 NW, 16 W, 17 SW, 18 S, 19 SE, 20 E (21 for NE does not exist in the file); when scrolling is blocked, variants 6..13. Over the view
  `FUN_00447c30` returns class 0..4 (0 enemy, 3 nothing/own, 1/2/4 building modes), and the table `0x4bc944` (0,2,3,4,5,...) converts it to a cursor id (0 sword, 2 bomb, 3, 4 mace, 5 hammers).
  Values 128..135 in this table = animated cursors from group 0x454 (48x576, 12 frames); assignment to modes not checked.
- Object size table `0x4bc890` (rows of 3 `i32`): {selection ellipse 308..317, health arc 263..258, object width threshold 55,67,83,102,125,154,189,231,999}.
- Screen layout 800x600 (mission 4 dump): `Menu` bar (sprite 282) at the top right from x=598, gold counter next to it; minimap in a stone frame (292/293), minimap image in the rectangle
  x=638..758, y=70..160 (1 px = 1 tile); unit portrait column x~722..780, y~218..390; command panel at the bottom (294-297, 254x208/288) from x=546, y=392; windows (save game, briefings)
  on parchment (`1000..1002`, 283). Tooltips not examined.

## Saves: script sequence and stream structure
- Save via menu (recompiled game, 800x600): `click 626 10` (Menu), `click 250 190` ("Save game"), `click 200 <138+20*slot>` (slot 1..9), `click 184 359` ("Save" -> window "Enter name"),
  `key <DIK>`... (e.g. 30=A), `key 28` (Enter) saves `Saves/single<slot>.sav` (slot 0 is the first row). Typing works only AFTER clicking "Save" (checked by dump: the field shows the typed text).
- The simulation is deterministic: two independent runs of mission 4 give a save of the same size (149924 B) and the same tick counter (55) at the same point in the script.
  Saves at tick 55 and 81 (26 ticks later): 149924 vs 150664 B, tick counter in `u32` no. 6 of the stream (from `+0x158`), view x,y = first two `u32`.
- Section order per `FUN_004455f0` (loading), each preceded by a `u32` count of elements: (1) objects of pool `0x4c1978` (`u32` marker, bit 31 = variant; ctor from stream `FUN_00418800`),
  (2) pool from `FUN_004597d0` (`u32` marker with bit 31), (3) `u32` number of pages, then in turn Side objects (0x69c) with their own deserializer (vtable[0]), (4) `u32` count, each: `u32` id, `u32` kind 0..5
  (0 = `FUN_0044eb40`, 2 = `FUN_00408a70`, 3 = `FUN_0044eb40` + `u32`, 4 = `FUN_0042df90`, 5 = `FUN_0044eb40`) - objects of pool `0x4d9b50`, (5) `u32` count, each `u32` id, `u32` size, `FUN_0043db90` (units),
  (6) optional block `FUN_0046f3d0`, (7) pool `0x4c1298`: 7 x `u32` + `double` per element, (8) pool `0x4c4ce0` (`FUN_0042a3b0`), (9) pool `0x4cd698` with kinds 0..n (`FUN_00446770` + `u32`...).
  Fields of individual classes NOT itemized (each class has its own deserializer, ~20 functions, e.g. `0x449e70` for the class with vtable `0x4940ec`, `0x44eb40` the object base with 85 reads).

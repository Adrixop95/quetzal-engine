# Engine rebuild checklist: what is needed

Status: [x] done, [~] partial, [ ] missing.

## Data
- [x] Archives `Res/*.xbe` (format known, decoders in `src/core`)
- [x] Sprites 0x44c/0x454 (flags 1,2,4,8,0x40; `docs/formats.md`)
- [x] Terrain maps `Maps/*.map`
- [x] Palettes `Pal.xbe`
- [ ] Animations (group 0x515)
- [ ] `.spr .vis .rm .dat .scn .mit` (next to a map): identify the fields (fog/visibility, starting objects, scripts)
- [~] `.mp` (INI with map description and starting units): text format, fields to list
- [ ] `strbase.bds` (strings, unit names)
- [ ] `Keys.ini`, `*.plr` (player), `single%i.sav` (saved game)
- [ ] Music `Music.xbe` (RIFF) and `Snd.xbe` (WAV): decoders are in the runtime, missing in `src/core`

## Object model
- [x] Classes from virtual tables: 176 (`docs/classes.md`), names from source paths (58)
- [x] Hierarchy (`tools/class_tree.py`), sizes and field layout from observation (`src/engine/model/objects.hpp`)
- [x] Object factory: 90 types, 11 kinds
- [ ] Meaning of fields other than position/health/animation
- [ ] Meaning of the 70-88 virtual methods of the base class (VisibleThing) and derived classes
- [ ] Type statistics tables (cost, health, speed, range): where they are loaded from
- [ ] Player commands (types 0x12-0x24): full list and data

## Simulation
- [~] Logic tick `Test.cpp`/`0x4577b0` roughly described
- [ ] Pathfinding: `WaveSearch.cpp` (0x46bb70, 0x46bdb0, 0x46cdc0) and `Heapsort.cpp` (0x473030)
- [ ] AI: `Tactic.cpp`, `Defence.cpp`, `Triggers.cpp`, `Conditions.cpp`
- [ ] Economy: `Harvest.cpp`, `Town.cpp`, `TownsInfo.cpp`, `BuildTree.cpp`
- [ ] Combat: `Weapon.cpp`, `SplashDamage.cpp`, projectiles (`Arrow`, `Axe`, `BallExpl`, `FireThrow`, `Cannon`)
- [ ] Fog of war / visibility: `MapExplorer.cpp`, `.vis`
- [ ] Random number generator (lockstep determinism)

## Other
- [~] Renderer (sprites, terrain, shadows, fog): functions `display.cpp`, `sprite.cpp`, `color.cpp`, `View.cpp`
- [ ] Interface (85 widget classes)
- [~] Network: lockstep packet protocol (to be documented from `0x43a410`)
- [ ] 3D sound (`3DSound.cpp`, `snd.cpp`, `mix.cpp`)

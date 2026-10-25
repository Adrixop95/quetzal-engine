# Engine rewrite (C++20): plan and status

Goal: a new engine in clean C++20 that loads the original game data (Aztec Empire, 1999) and behaves the same way,
but with improvements (GPU renderer with interpolation, better pathfinding, wide screens, new features).
The recompiled version (`runtime/` + `work/gen/`) remains the reference for differential tests, not a product.

## Rules
- The code of the new engine is written from scratch, based on observed behavior and reconstructed structures.
  No code generated from the binary, no binary, and no game assets go into the repo.
- The simulation is deterministic (fixed 12 Hz step, no dependency on wall-clock time, floating-point numbers
  only where the original also uses them and with the same precision), because multiplayer is lockstep.
- Each simulation module is built with a differential test against the reference (same input state + same commands =
  same output state, compared by hash).

## Target modules
| Module | Contents | Source of knowledge |
|---|---|---|
| `core` | file formats, .xbe archives, sprites, maps, ini (done in `src/core`) | docs/formats.md |
| `sim` | world, units, buildings, economy, combat, pathfinding, AI, commands | classes from `tools/analyze_classes.py` |
| `render` | GPU renderer (SDL_GPU/OpenGL), interpolation between ticks, fog of war, minimap | drawing functions + `docs/formats.md` |
| `ui` | menus, panel, briefings, dialogs, F9 overlay | original + `runtime/overlay.c` |
| `audio` | mixing, music from CD (xbe), speech | `runtime/dsound.c` |
| `net` | lockstep, IPX/UDP, lobby | `runtime/winsock.c`, protocol from the code |
| `platform` | window, input, saving, settings | `runtime/backend.c` |

## Tools prepared
- `tools/analyze_classes.py`: recovers C++ classes from virtual tables (constructors write the vtable address to `[reg]`):
  `python3 -I tools/analyze_classes.py game/Aztec.exe work/gen/gen_funcs.h work/classes.json`
- `tools/class_fields.py`: field catalog per class (offset, type, number of references) from the decompiler:
  `python3 -I tools/class_fields.py work/classes.json work/decomp/all.c work/classes.md`
- `tools/coverage.py`, `tools/explore.sh`, `tools/monkey_all.sh`, `tools/mp_test.sh`: function coverage and scenario tests.
- Results (`work/`) are derived from the binary and are not committed.

## Findings (2026-10-19)
- The game has 176 classes with virtual tables (no RTTI). A family of about 20 classes with 75-88 virtual methods each,
  sizes 100-250 bytes, inherits from a base class with vtable `0x48f2d8` (75 methods): candidates for units, buildings and world objects.
- The player side (`Side`) is an object of size 0x69c: `+0x640` total spent, `+0x688` pointer to the base object, `+0x690` list,
  `+0x698` gold, `+0x140` victory flag.
- The game draws the scene once per logic tick (12 Hz), the cursor separately (captured, drawn by the backend).

- Object factory (`0x43d5d0`, string "obj_NewUnit"): 90 object types in table `0x4933f8` (kind, parameter), 11 kinds
  (ctors 0x433190, 0x44a350, 0x40fb70, 0x471390, 0x460090, 0x407360, 0x42bda0, 0x41cce0, 0x408410, 0x415f50, 0x42d9b0).
  Kind 2 covers 44 types, kind 0 covers 17 types. Tool: `tools/object_kinds.py`.
- Objects live in pools (allocator `0x4cc3d0`) in the VirtualAlloc region 0x50000000+ and in the heap 0x10000000+.
  The script command `objscan VTABLES OUT` (RT_SCRIPT) lists live objects (address, vtable); `heapdump PATH` saves the heap.
  In mission 1 after 3 s: about 700 objects from 176 known vtables (most numerous 0x490ab8: 148, 0x48f5e0: 96, 0x4932cc: 60).
  Classes are not yet assigned to roles (unit, building, tree, projectile).

- Object pools (class with vtable `0x4953f8`): header `+4` arena base, `+8` end of used area, `+0x10` stride (8 bytes of list header
  + data), `+0x14` capacity, `+0x18` number of live objects. Unit pool: `0x4cc3d0` (stride 0xb8, capacity 10000),
  the others updated in the tick: `0x4cd698` (stride 0x30), `0x5156f8` (0x1c0), `0x4c1978` (0x40).
- Unit object (class `0x4932cc` and derivatives, data from `pool_elem+8`): `+0x24/+0x28` position in world pixels (centre),
  `+0x44/+0x48` tile (tile = 48 px), `+0x6c/+0x70` fixed-point position (px*256), `+0x38/+0x3c` (e.g. 0x1770/0x1770) probably
  the health value, `+0x80` animation direction/state, `+0x88` animation counter, `+0x30` pointer to the animation set.
- Logic tick (`0x4577b0`): processes side commands (types 0x12-0x24 from queues `0x43d120`), then updates the 3 pools through
  virtual methods (+0x60, +0x8, +0x138) and sides (`0x449980`).
- State oracle: `RT_STATE_LOG=file` writes each tick: the number of live objects and an FNV hash over the 4 pools (pointers masked).
  Two independent runs of mission 1 without input agree on 211 of 214 ticks (3 transient differences, unexplained).

- Names from `assert` strings in the exe: 58 source files of the original (`E:\SnakE\Empire\Source\*.cpp`), including Marine (kind 0), Spider (1),
  Building (2), Zeppelin (3), Truck (4), Bird (5), Headtruck (6), Defence (7), Boat (8), Cannon (9), Johnka (10), Arrow, Axe, BallExpl, FireThrow,
  SplashDamage, Weapon, Tactic (AI), Town/TownsInfo, BuildTree, Harvest, Triggers/Conditions, MapExplorer, WaveSearch (pathfinding), Heapsort.
  Functions with names in strings: `wsr_WaveSearch` 0x46c470 (pathfinder: wave across the grid with passability tables),
  `obj_NewUnit` 0x43d5d0, `obj_NewVoidUnit` 0x43e0b0, `wep_NewWeapon` 0x46d900, `wep_CalcKillTime(s)` 0x46fac0/0x46fbd0.
- Class documentation: `docs/classes.md`; layout headers: `src/engine/model/objects.hpp` (generated); checklist: `docs/REBUILD_CHECKLIST.md`.

- Field `+0x50` of each world object = type id (UGI, 0..89); `+0x3c` = max health of the type (e.g. 6000 footman, 5000 rifleman, 3500 archer,
  10000 knight, 36000 bear); `+0x68` = pointer to the static type description record (0x4c bytes, .data from 0x4ac428, filled at start-up).
- `docs/object_types.md`: 90 types (UGI name, class) and 45 packet/command types (PCT_*), including frame commands (MOVE, ATTACK, BUILD, ...).
- Test tools: `tools/make_ztest.py` + `tools/ztest_run.sh` (own test map with units of selected types on the enemy side;
  buildings must be created through the [Town] section, in [Team] they call an empty pointer in `Building.cpp`).

- Structure of the logic tick `0x4577b0` (more fully): (1) side commands, (2) pool update in order: `0x4c1978` (+0x60), `0x4cd698` (+0x8),
  `0x5156f8` (+0x138), sides `0x449980`, `0x4d9b50` (+0x138), units `0x4cc3d0` (+0x120), `0x515a98` (+0x84), weapons `0x4d9bbc[]` (`0x470c10`), `0x4c1298` (+0x8),
  (3) building display lists: for objects from pools `0x4cc3d0`, `0x515a98`, `0x4c1978` a call to `0x47cb30(list 0x4d9c10, position, ..., sprite)`, and for layers
  per side `0x47d070`/`0x47d430` (list `0x4d9c0c`); the sprite of the current frame is returned by the virtual method +0x70/+0x74/+0x80/+0x5c, the position by +0x14.
  Conclusion for the renderer: the scene is built once per tick; smooth movement requires remembering each object's previous position and interpolating when drawing.

## Stages
1. [x] Class and field tools, plan.
2. [ ] Reconstruction of structures: base object class, world object list, tile map, commands.
3. [~] Oracle: a state hash per tick exists (RT_STATE_LOG). Missing: recording of commands per tick (hook on 0x43d120) and pool snapshots for module tests.
4. [ ] GPU renderer with interpolation (reads game state from the recompiled memory, later from `sim`).
5. [ ] `sim` module: pathfinding + collisions, then the rest, each with a differential test.
6. [ ] Coverage of unexecuted functions: scenarios (missions 10-15, editor, skirmish, AI in harder layouts),
      and for functions without a scenario: unit tests calling them directly on state dumps.
7. [ ] Replace modules in the recompilation with the new ones, until nothing from the lifter remains.

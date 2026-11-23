# Engine rewrite (C++20): plan and status

Evidence labels (audit 2026-11-23): V = inspected source fact, with reference
or clone scope stated; M = measurement rechecked from retained captures or an
offline clone probe; G = inference, proposed procedure or uncorroborated
historical report. A clone V does not establish native parity. A marker covers
one paragraph/list item or the entire immediately following table. Unresolved
meanings remain G even inside a table whose codec structure is V.
See [label_audit.md](label_audit.md) for counts and evidence limits.

**G** (historical plan or observation). Goal: a new engine in clean C++20 that loads the original game data (Aztec Empire, 1999) and behaves the same way,
but with improvements (GPU renderer with interpolation, better pathfinding, wide screens, new features).
The recompiled version (`runtime/` + `work/gen/`) remains the reference for differential tests, not a product.

## Rules
- The code of the new engine is written from scratch, based on observed behavior and reconstructed structures.
  No code generated from the binary, no binary, and no game assets go into the repo.
- **G** (historical plan or observation). The simulation is deterministic (fixed 12 Hz step, no dependency on wall-clock time, floating-point numbers
  only where the original also uses them and with the same precision), because multiplayer is lockstep.
- Differential tests against normalized reference observations are a target. Existing engine replay/hash equality checks compare clone runs; raw reference and engine hashes use different representations and cannot establish module parity.

## Target modules
**G** (historical plan or observation).

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
- **G** (historical plan or observation). `tools/analyze_classes.py`: recovers C++ classes from virtual tables (constructors write the vtable address to `[reg]`):
  `python3 -I tools/analyze_classes.py game/Aztec.exe work/gen/gen_funcs.h work/classes.json`
- **G** (historical plan or observation). `tools/class_fields.py`: field catalog per class (offset, type, number of references) from the decompiler:
  `python3 -I tools/class_fields.py work/classes.json work/decomp/all.c work/classes.md`
- `tools/coverage.py`, `tools/explore.sh`, `tools/monkey_all.sh`, `tools/mp_test.sh`: function coverage and scenario tests.
- Results (`work/`) are derived from the binary and are not committed.

## Findings (2026-10-19)
- **G** (historical plan or observation). The game has 176 classes with virtual tables (no RTTI). A family of about 20 classes with 75-88 virtual methods each,
  sizes 100-250 bytes, inherits from a base class with vtable `0x48f2d8` (75 methods): candidates for units, buildings and world objects.
- **V** (reference structure). The player side (`Side`) is an object of size 0x69c: `+0x640` total spent, `+0x688` pointer to the base object, `+0x690` fog-map pointer,
  `+0x698` gold, `+0x140` victory flag. Side `+0x690` is the per-side fog/brightness map pointer, not a generic object list.
- **G** (historical plan or observation). The game draws the scene once per logic tick (12 Hz), the cursor separately (captured, drawn by the backend).

- **V** (reference structure). Object factory (`0x43d5d0`, string "obj_NewUnit"): 90 object types in table `0x4933f8` (kind, parameter), 12 kinds including Camera
  (ctors 0x433190, 0x44a350, 0x40fb70, 0x471390, 0x460090, 0x407360, 0x42bda0, 0x41cce0, 0x408410, 0x415f50, 0x42d9b0).
  Kind 2 covers 44 types, kind 0 covers 17 types; Camera is kind 11, making 12 factory kinds including Camera. Tool: `tools/object_kinds.py`.
- **G** (historical plan or observation). Objects live in pools (allocator `0x4cc3d0`) in the VirtualAlloc region 0x50000000+ and in the heap 0x10000000+.
  The script command `objscan VTABLES OUT` (RT_SCRIPT) lists live objects (address, vtable); `heapdump PATH` saves the heap.
  In mission 1 after 3 s: about 700 objects from 176 known vtables (most numerous 0x490ab8: 148, 0x48f5e0: 96, 0x4932cc: 60).
  Classes are not yet assigned to roles (unit, building, tree, projectile).

- **G** (historical plan or observation). Object pools (class with vtable `0x4953f8`): header `+4` arena base, `+8` end of used area, `+0x10` stride (8 bytes of list header
  + data), `+0x14` capacity, `+0x18` number of live objects. Unit pool: `0x4cc3d0` (stride 0xb8, capacity 10000),
  the others updated in the tick: `0x4cd698` (stride 0x30), `0x5156f8` (0x1c0), `0x4c1978` (0x40).
- **G** (historical plan or observation). Unit object (class `0x4932cc` and derivatives, data from `pool_elem+8`): `+0x24/+0x28` position in world pixels (centre),
  `+0x44/+0x48` tile (tile = 48 px), `+0x6c/+0x70` fixed-point position (px*256), `+0x38/+0x3c` primary/secondary HP (not current/max HP), `+0x80` animation direction/state, `+0x88` animation counter, `+0x30` owning tactic; Marine animation is at `+0x7c`.
- **V** (reference structure). Logic tick (`0x4577b0`): processes side commands (types 0x12-0x24 from queues `0x43d120`), then updates the 3 pools through
  virtual methods (+0x60, +0x8, +0x138) and sides (`0x449980`).
- **G** (historical plan or observation). State oracle: `RT_STATE_LOG=file` writes each tick: the number of live objects and an FNV hash over the 4 pools (pointers masked).
  Two independent runs of mission 1 without input agree on 211 of 214 ticks (3 transient differences, unexplained).

- **G** (historical plan or observation). Names from `assert` strings in the exe: 58 source files of the original (`E:\SnakE\Empire\Source\*.cpp`), including Marine (kind 0), Spider (1),
  Building (2), Zeppelin (3), Truck (4), Bird (5), Headtruck (6), Defence (7), Boat (8), Cannon (9), Johnka (10), Arrow, Axe, BallExpl, FireThrow,
  SplashDamage, Weapon, Tactic (AI), Town/TownsInfo, BuildTree, Harvest, Triggers/Conditions, MapExplorer, WaveSearch (pathfinding), Heapsort.
  Functions with names in strings: `wsr_WaveSearch` 0x46c470 (pathfinder: wave across the grid with passability tables),
  `obj_NewUnit` 0x43d5d0, `obj_NewVoidUnit` 0x43e0b0, `wep_NewWeapon` 0x46d900, `wep_CalcKillTime(s)` 0x46fac0/0x46fbd0.
- Class documentation: `docs/classes.md`; generated model outputs belong under ignored `work/`, not public engine headers; checklist: `docs/REBUILD_CHECKLIST.md`.

- **V** (reference structure). Field `+0x50` of each world object is its UGI type (0..89). Object `+0x3c` is secondary HP. Marine `+0x68` points to its descriptor (0x4c-byte stride, base `0x4ac430`); other kinds have different descriptor offsets and strides. Primary maximum HP is a virtual getter, not this storage word.
- **G** (historical plan or observation). `docs/object_types.md`: 90 types (UGI name, class) and 45 packet/command types (PCT_*), including frame commands (MOVE, ATTACK, BUILD, ...).
- Test tools: `tools/make_ztest.py` + `tools/ztest_run.sh` (own test map with units of selected types on the enemy side;
  buildings must be created through the [Town] section, in [Team] they call an empty pointer in `Building.cpp`).

- **V** (reference structure). Structure of the logic tick `0x4577b0` (more fully): (1) side commands, (2) pool update in order: `0x4c1978` (+0x60), `0x4cd698` (+0x8),
  `0x5156f8` (+0x138), sides `0x449980`, `0x4d9b50` (+0x138), units `0x4cc3d0` (+0x120), `0x515a98` (+0x84), weapons `0x4d9bbc[]` (`0x470c10`), `0x4c1298` (+0x8),
  (3) building display lists: for objects from pools `0x4cc3d0`, `0x515a98`, `0x4c1978` a call to `0x47cb30(list 0x4d9c10, position, ..., sprite)`, and for layers
  per side `0x47d070`/`0x47d430` (list `0x4d9c0c`); the sprite of the current frame is returned by the virtual method +0x70/+0x74/+0x80/+0x5c, the position by +0x14.
  Conclusion for the renderer: the scene is built once per tick; smooth movement requires remembering each object's previous position and interpolating when drawing.

## Stages
1. **G** (historical plan or observation). [x] Class and field tools, plan.
2. **G** (historical plan or observation). [ ] Reconstruction of structures: base object class, world object list, tile map, commands.
3. **G** (historical plan or observation). [~] Oracle: a state hash per tick exists (RT_STATE_LOG). Missing: recording of commands per tick (hook on 0x43d120) and pool snapshots for module tests.
4. **G** (historical plan or observation). [ ] GPU renderer with interpolation (reads game state from the recompiled memory, later from `sim`).
5. **G** (historical plan or observation). [ ] `sim` module: pathfinding + collisions, then the rest, each with a differential test.
6. **G** (historical plan or observation). [ ] Coverage of unexecuted functions: scenarios (missions 10-15, editor, skirmish, AI in harder layouts),
      and for functions without a scenario: unit tests calling them directly on state dumps.
7. **G** (historical plan or observation). [ ] Replace modules in the recompilation with the new ones, until nothing from the lifter remains.

## Label audit corrections (2026-11-23)

Evidence checked: Reference: 0x433190, 0x43e890, 0x43d5d0 and 0x4577b0; the read-only factory table confirms 90 entries and 12 kinds including Camera. Clone: `src/engine/sim/game_sim.cpp`, `rules.cpp`, `src/engine/data/type_anims.hpp` and `runtime/oracle.c`. Old heap/class-count/run statistics remain uncorroborated; stages describe historical goals.

1. Corrected an aspirational differential-testing statement that read as completed parity coverage.
2. Corrected the Side +0x690 interpretation.
3. Corrected primary/secondary HP and separated the owning tactic from animation storage.
4. Corrected the secondary-HP claim, Marine descriptor base and unsupported cross-class layout generalization.
5. Removed the misleading generated public-layout-header recommendation.
6. Made the Camera factory case explicit: 11 gameplay kinds plus Camera, 12 cases total; the 44 Building and 17 Marine counts are retained.

No original executable or reference runtime was run for this audit. Historical
validation totals retain their original dates; they are not this
audit's test results. Private captures, derived source and probe output remain
outside the tracked documentation.

# C++20 engine (src/engine)

A clean reimplementation that loads the original game data from `game/`. Nothing from the game or the executable is stored in the repository.
CMake targets: `aztec_engine` (library `aztec_engine_lib` plus `src/engine/app/main.cpp`) and the tests `test_engine`, `test_path`, `test_formats` (run all with `ctest`).

## Running
    cmake -S . -B build_engine && cmake --build build_engine -j --target aztec_engine test_engine
    build_engine/aztec_engine                          # front-end menu
    build_engine/aztec_engine --game game 1            # mission 1 directly (also: 3, Mission_4, Pentagram, a path to a .mp)
    build_engine/aztec_engine --list                   # list maps
    build_engine/aztec_engine 1 --wander --borderless  # units walk randomly (shows 12 Hz tick interpolation)
    build_engine/aztec_engine 1 --size 1920x800 --screenshot out.png --cam 40,40 --zoom 1.5
    build_engine/test_engine game
Keys: scrolling bindings from `Keys.ini` (arrows / numeric keypad by default) and edge scrolling, middle or right-drag = pan, wheel = zoom, left click on the minimap = jump,
F11 borderless, Alt+Enter fullscreen, F1 help, F2 grid, F3 passability, F4 labels, F5 markers, F6 edge scroll, F7 terrain overlays, F8 fog,
F9 cursor confinement, F10 HUD, Space pause (view, only if unbound in `Keys.ini`), P pause (simulation), PrintScreen screenshot, F12 toggle panel, Esc cancel targeting / quit / pause menu.

Cursor confinement is enabled by default during focused gameplay in windowed,
borderless and fullscreen modes. Focus loss, holding Alt (including Alt-Tab),
pause and the front-end release it. F9 toggles the preference for the current
session; `--no-mouse-grab` starts with it disabled. Absolute mouse coordinates
remain available for scrolling at the outer window edges, including over the
right panel. Scripted playtests and screenshots do not grab the mouse.

## Modules
- `platform/`: `Window` (windowed / borderless / fullscreen, vsync, any resolution), `Input`, `FixedStepper` (fixed 12 Hz step).
- `assets/`: `GameData` (archives, case-insensitive file lookup), `mission` (adapter over `core/mp.*`, cp1250 -> UTF-8), `sprite_provider.hpp`
  (type -> sprite interface), `anim_provider.*`, `daylight.*`.
- `world/`: `World` / `Object` (fixed-point positions px*256, `prev_*` and `cur_*` per object for interpolation), `Camera` (smooth scroll, zoom steps, clamping), `type_info`.
- `sim/`: `Simulation` interface (12 Hz tick; the simulation writes only `cur_*`, the driver calls `World::begin_tick()`), `GameSim` (see below), `path/` (movement module).
- `render/`: `Atlas` (2048^2 pages, shelf packing), `SpriteStore` (lazy decoding from .xbe), `QuadBatch` (SDL_RenderGeometry, one call per atlas page),
  `SceneRenderer` (terrain, overlays, objects, fog), `Font` (original fonts, built-in 3x5 fallback), dependency-free PNG writer.
- `ui/`: `Cursor` (software cursor drawn last every frame), `Minimap`, `Panel`, HUD, selection, command UI, front-end, pause menu.
- `audio/`: software mixer plus SDL audio.
- `app/`: command-line options, main loop (rendering at vsync, simulation at a fixed step), playtest driver, sprite-sheet tool.

## Sprites and data
- `AnimSpriteProvider` (`assets/anim_provider.*`, built on `data/sprites.hpp`): type + state (idle / move / attack / die / fidget) + direction 0..7 + animation tick
  -> sprite from `Units.xbe` (body 0x44c, shadow 0x454 drawn underneath as a translucent silhouette). Animation advances with `Object::anim_tick`
  (incremented in `World::begin_tick()`, i.e. at the fixed 12 Hz step); positions are still interpolated between ticks.
- Sprite hotspot: top-left = position - (ox, oy) (+ the frame's `yoff`). This matches a screenshot of the original (building position relative to units).
- Sprite palette comes from the header (+0xC): Tiles0 uses Pal 4/5/6 (grass/sand, water, shore), Tiles1 8/9/10; for Units, ids < 4 are team palettes
  `id + 0x10 + colour*4`. Sprites with flag 0x40 are palette-indexed RLE with alpha (`core/sprite_alpha.*`), not 16-bit colour.
- Tiles with `h > cell_h` are animation strips (e.g. water: 8 frames of 48 px): frame = `(tick/2) % n`. The `.map` overlay layer (high 16 bits) holds trees and rocks:
  body in 0x44c, shadow in 0x454 under the same id, 192x192 cells with 4 variants, y-sorted together with objects.
- Buildings from `[Town#]` are added to the world (type from `Type` and `Race`); a finished building uses animation slot 1 (slot 0 = construction stages).
- Fog of war: the local side's `.vis` layer (`World::local_side`), one texel per tile with linear filtering; objects of other sides in the dark are hidden;
  F8 toggles it, `World::fog_version` triggers refreshes. The minimap is fogged too.
- Cursors from `common.xbe`: feather (id 1, default), sword (0, attack / enemy hover), boots 6..13 = edge-scroll arrows (hotspot at the screen edge),
  mace 2 and spear 5 (move / pan, assignment uncertain), hammers 3 (unused), red X 14..21 (unused); animated at 80 ms per frame like the original.
- Right panel: `common.xbe` id 292 (204x493, palette 48), the bottom strip repeats in taller windows; the minimap sits inside the panel window and the camera
  width is the window minus the panel. Command buttons use common 275 (move hand), 257 (attack sword), 268 (stop shield),
  301 (repair tools), 288 (garrison helmet), and 269 (destroy bomb). Repair and garrison are disabled because the simulation does not implement them.
  Production uses original monochrome icons and the selection uses colour portraits from Units animation icon sequences: slot 23 (most units),
  7 (buildings), 39 (trucks), 9 (spiders), 17 (hopcannon); types without icon sequences fall back to their original idle sprite.
  Contact sheets identify footman icon/portrait 1385/1386, Russian town 4119/4120, truck weapon variants 3984..3995, and spider variants 73..87.
  Only sprite ids and animation references are stored in source; images remain in the user's game files.
- Tool: `aztec_engine --game game --sprite-sheet common:0-21 out.png` dumps a contact sheet from an archive (units|tiles0|tiles1|common).
- Plugging in your own sprites: implement `SpriteProvider::pick/pick_shadow` (or fill a `TableSpriteProvider`) and pass it to `run_engine(opts, provider)`.
  `--no-sprites` forces placeholder markers.

## Lighting, fonts, UI
- Daylight: the key tables (time, RGB, ambient level) are read at run time from `game/Aztec.exe` (`assets/daylight.*`; nothing from the executable is in the repo).
  The clock is the mission `StartTime` plus simulation ticks (7200 ticks per day); keys are interpolated linearly (the original steps). Sprites are baked at palette
  level 14 and modulated by colour (tint * brightness of level ambient+1 relative to 14). Against a screenshot of the original, sand is about (178,128,111) vs (201,124,112) and grass
  about 1.3x too bright. The +1 level and the missing per-tile light map are approximations. `--no-light`, `--time T`.
- Fonts: `fnts.xbe` base 1200 (small, anti-aliased, palette 53), glyph = base + cp1250 byte; the built-in 3x5 font remains as a fallback and for placeholder labels.
- `core/strbase.*` parses `strbase.bds` (UTF-8); `ui/names.*` maps type ids to display names (ordinals 166-210, matched by name).
- Selection: click or left-drag a box (own units only, Shift adds), animated selection ring (common 309 / 314), health bar (descriptor maximum from object state),
  panel summary, hover tooltip ("Name\nPrice: %i Time: %i" when `docs/data` is available). `--select X0,Y0,X1,Y1` selects in screenshot mode.

## Simulation
- `sim/game_sim.*`: deterministic 12 Hz simulation (integers only), commands from `sim/commands.hpp` (numbers = PCT_FRAME_*), state hash (`state_hash`), command log
  (`serialize_log`) and replay (`GameSim::replay`), which doubles as snapshot / restore. Tests (`test_engine game`): the same commands give the same hash, different ones do not,
  replay reproduces the hash, combat, unit training, scripts.
- Rules and data: `sim/rules.*` reads `docs/data/*.csv` at run time (`--rules-dir`); `docs/engine_rules_status.md` lists what is verified and what is a placeholder.
- Movement: `sim/path/` (movement agent from the pathfinding work); `GameSim` only uses `MoveSystem`.
- Controls (`ui/commands_ui.*`): right click = move / attack (right-drag pans), `M` + click = move, `X` + click = attack an enemy / attack-move on terrain,
  `H` = stop, Ctrl+0-9 / 0-9 = groups, Ctrl+Shift+0-9 adds to a group, `P` = pause the simulation. M/X/H/P are engine extensions, available only when
  the key is unbound in `Keys.ini`. Original Destroy, Add, scrolling, ToggleInterface and PrintScreen bindings are loaded through `core/userfiles.*`.
  Unsupported repair, garrison and stance bindings perform no gameplay action. Arrow/keypad scrolling avoids conflicting with W/A/S/D mode bindings.
  Targeting also works on the minimap; right-click or Escape cancels it. Target clicks preserve selection.
  Panel: production icons from the side's build tree, prerequisites / money / queue-full tooltips, paged entries for short windows, and `[B]` placement icons.
  Click queued icons to cancel with the simulation's refund; click the queue header to pause/resume production. All gameplay actions use `sim/commands.hpp`.
  The minimap terrain receives the same daylight tint and palette brightness as the world; team markers and UI artwork retain their own colours.
- Mission scripts: `sim/script_runner.*` executes `.scn` files (conditions, actions, tactics, timers, victory / defeat, messages, centerView); AI skeleton: `sim/ai.cpp`.
- Test options: `--cmd-move X,Y` / `--cmd-attack-move X,Y` (for `--select`), `--no-sim`, `--wander`.

## Front-end, audio, saves, playtests
- Front-end (`ui/frontend.*`): main menu (common 280 background), campaign and custom maps (270), briefing with `.mit` text (290) and an intro video through `ffmpeg`
  ("Intro video" button), options (284). Starting without a mission argument opens the menu; `--no-menu` skips it. Esc in a mission opens the pause menu
  (resume, save, load, restart, main menu, quit); after a win, Enter continues to the next mission. `AZTEC_MENU_SHOT=dir` saves screenshots of the menu screens.
- Audio (`audio/`): software mixer plus SDL; effects come from field [2] of animation frames with the original pan / attenuation rules; music follows `Music` / `MusicMask`.
  `--no-audio`, `--volume V`.
- Saves: Ctrl+S / Ctrl+L (slot 1), `sim/savegame.*` (command log plus replay).
- Playtests without a human: `--script file` (`app/playtest.hpp` documents the commands: click, drag, key, run, shot, print, expect); examples in `tests/playtest/`.
  Virtual time: one frame = one tick.

## Known gaps
- No per-tile light map (light sources, `light.xbe` masks are not applied).
- Initial unit facing (the engine uses south); attack / die / fidget states and 32-direction slots (trucks, birds) are only handled for idle / move.
- Damage and construction stages, projectiles, effects and the second (turret / weapon) sprite are not drawn.
- The `.rm` map is loaded but unused.
- No garrison / repair, networking, original `.sav` format; side colours (`Color` 0..7) for placeholder markers are arbitrary. Other rules: see `docs/engine_rules_status.md`.
- A screenshot of the recompiled original (800x600, mission 1) was compared by eye: camera / object layout matches; differences are lighting and the panel buttons.

## Panel verification

Run `test_engine --pure` for input, layout, targeting, construction, queue pause/cancel/refund, and deterministic command replay checks without game files.
With the original game installed, run from the repository root:

    SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy build/aztec_engine --game /path/to/game 1 --size 1000x700 --no-audio --script tests/playtest/basic.txt
    SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy build/aztec_engine --game /path/to/game 1 --size 1000x700 --no-audio --script tests/playtest/panel.txt

`panel.txt` verifies selection preservation, command cancellation, production pause, refunds, and keyboard conflicts. Its screenshots are written under `build/`.
Use `tests/playtest/panel_lighting.txt` with `--time 0` and `--time 3600` to compare minimap daylight while keeping identical camera and selection inputs.

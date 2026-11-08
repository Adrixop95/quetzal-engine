# quetzal-engine

An open-source reimplementation of the engine of **The Aztec / Aztec Empire** (New Media Generation, 1999), a real-time strategy game.
The project reads the data files of your own copy of the game; **no game data, executable or generated code derived from the executable is part of this repository.**

Status: early work in progress. See `docs/` for what is understood so far.
The [v1 roadmap](docs/ROADMAP.md) defines campaign playability, deterministic
simulation, engine save/load and Linux/Windows release requirements.

## What is here
- `src/core/` - loaders for the original file formats (archives, sprites, palettes, maps, mission files `.mp`/`.scn`/`.vis`, strings, saves, audio banks).
- `src/engine/` - the new C++20 engine: fixed 12 Hz deterministic simulation, GPU-friendly renderer with interpolation, widescreen, fog of war, original cursors/fonts/UI.
- `runtime/`, `tools/` - research tooling: a static-recompilation runtime used as a reference implementation and test oracle (record/replay of lockstep commands, per-tick state hashes), analysis scripts, experiment runners. These need your own copy of the game and a Ghidra install; generated code is never committed.
- `docs/` - documentation of file formats, object classes, rules, movement and AI as they are discovered.

## Project status

Estimated 2026-11-09. These are **the maintainers' estimates**, not measured values; there
is no single progress metric. Three things are kept apart: how much of the original's
behaviour is understood, how much of it the engine implements, and how much has been checked
against the running original or by playing. Update this table when a major finding or merge
changes it (see `docs/gap_recon.md` for the open items behind each row).

| Area | Understood | Implemented | Checked in the original or by play |
| --- | --- | --- | --- |
| Data tables (types, descriptors, weapon records) | ~90% | ~85% | partial |
| Weapon and damage formulas | ~85% (weapon graph, anti-air table, effects) | ~50% | ~25% |
| Movement and path finding | ~60% | ~55% | ~30% |
| Unit behaviour (states, acquisition, groups) | ~50% | ~45% | ~25% |
| Economy and construction | ~60% | ~60% | ~30% |
| Computer players (purchases, waves, defence) | ~35% | ~30% | ~10% |
| Mission scripts (conditions, actions, flow) | ~75% | ~85% | forced-state tests only |
| Transport (ferries) | ~90% (control flow, passenger rule, tick timeline) | ~35% (model to be replaced) | one measured cycle |
| Special mechanics (towers, air units, fire, town capture) | ~60% (capture rule, ownership mechanism, hypno resolved) | ~30% | ~15% |
| Saves and resume | ~60% | ~65% | ~20% |
| Interface and menus | ~40% | ~55% | ~15% |
| Audio | ~40% | ~50% | ~10% |
| Campaign (missions 1 to 15) | ~30% | ~70% (scripts) | ~7% (one mission played honestly) |

Overall (rough): about 65% of the original's rules understood, about 45 to 50% implemented,
about 15 to 20% checked against the original or by play. The reference oracle (the original
executable running headless under an observer, plus its decompilation) covers about 35% of
the logic so far; the decompilation corpus now holds 5468 functions with an index of one-line annotations used as leads, not as truth; missions 4 to 15 have not been measured.

## Building the engine
```
cmake -S . -B build_engine && cmake --build build_engine -j
build_engine/aztec_engine --game /path/to/game 1
```
Requires SDL2 and a C++20 compiler. `/path/to/game` is the installed game directory (with `Res/`, `Maps/`, ...).

## Legal
This project is not affiliated with or endorsed by the original developers or publishers. You must own a legal copy of the game.
Game assets are never distributed here.

## License
GPL-3.0-or-later (see `LICENSE`). Third-party libraries keep their own licenses (SDL2: zlib).

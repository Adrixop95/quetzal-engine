# quetzal-engine

An open-source reimplementation of the engine of **The Aztec / Aztec Empire** (New Media Generation, 1999), a real-time strategy game.
The project reads the data files of your own copy of the game; **no game data, executable or generated code derived from the executable is part of this repository.**

Status: early work in progress. See `docs/` for what is understood so far.

## What is here
- `src/core/` - loaders for the original file formats (archives, sprites, palettes, maps, mission files `.mp`/`.scn`/`.vis`, strings, saves, audio banks).
- `src/engine/` - the new C++20 engine: fixed 12 Hz deterministic simulation, GPU-friendly renderer with interpolation, widescreen, fog of war, original cursors/fonts/UI.
- `runtime/`, `tools/` - research tooling: a static-recompilation runtime used as a reference implementation and test oracle (record/replay of lockstep commands, per-tick state hashes), analysis scripts, experiment runners. These need your own copy of the game and a Ghidra install; generated code is never committed.
- `docs/` - documentation of file formats, object classes, rules, movement and AI as they are discovered.

## Building the engine
```
cmake -S . -B build_engine && cmake --build build_engine -j
build_engine/aztec_engine --game /path/to/game 1
```
Requires SDL2 and a C++20 compiler. `/path/to/game` is the installed game directory (with `Res/`, `Maps/`, ...).

## Legal
This project is not affiliated with or endorsed by the original developers or publishers. You must own a legal copy of the game.
Game assets are never distributed here.

---

# PL
Otwarta reimplementacja silnika gry **The Aztec / Imperium Azteków (Aztec Empire)** (New Media Generation, 1999). Projekt czyta pliki z Twojej własnej kopii gry;
repozytorium nie zawiera zasobów, pliku wykonywalnego ani kodu wygenerowanego z pliku wykonywalnego. Etap: wczesny. Szczegóły w `docs/` i `src/engine/README.md`.

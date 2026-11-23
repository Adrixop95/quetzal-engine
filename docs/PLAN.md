# Aztec Empire: reconstruction plan

Evidence labels (audit 2026-11-23): V = inspected source fact, with reference
or clone scope stated; M = measurement rechecked from retained captures or an
offline clone probe; G = inference, proposed procedure or uncorroborated
historical report. A clone V does not establish native parity. A marker covers
one paragraph/list item or the entire immediately following table. Unresolved
meanings remain G even inside a table whose codec structure is V.
See [label_audit.md](label_audit.md) for counts and evidence limits.

**G** (historical host plan or observation). Goal: a playable build of Aztec Empire (New Media Generation, 1999) for Linux/Windows/macOS, with fixes (widescreen).
The repo does not contain Aztec.exe, assets, or any code derived from the binary. The user supplies their own game installation.

**G** (historical host plan or observation). This is the historical 2026-10-18 reference-host plan. The independent C++ engine now builds on Linux, Windows and macOS; the reference runtime remains separate. Stage checkboxes below are historical goals, not current acceptance results. See [HANDOFF.md](HANDOFF.md).

## Strategy: static recompilation
1. **G** (historical host plan or observation). `tools/ghidra_scripts/LiftAll.java` (Ghidra headless) turns the P-code of each x86-32 function into C
   (`work/gen/gen_*.c`). The output file is derived from Aztec.exe and never goes into git.
2. **V** (host construction). `runtime/` is a hand-written runtime: 4 GiB of guest memory, PE loader, function dispatcher,
   WinAPI shims (win32.c), and DirectX on SDL2 (ddraw.c, dsound.c, dinput.c, backend.c).
3. **G** (historical host plan or observation). `src/core`, `src/app`, `src/tools`: our own decoders for the formats (.xbe archives, sprites, palettes, maps)
   and an SDL2 viewer. They serve verification and a future clean reimplementation.

## Stages
- [x] Resource formats identified and verified (docs/formats.md).
- **G** (historical host plan or observation). [x] Lifter: 100% of functions compile to C.
- [~] Runtime: start-up CRT and WinMain run; further missing shims and addresses.
- [ ] Playable single-player mode on Linux (menus, missions, sound, Indeo video via ffmpeg).
- [ ] Widescreen: SetDisplayMode/viewport (patching the game's constants or reimplementing the view).
- [ ] Windows and macOS (VirtualAlloc instead of mmap, long double on ARM).
- [ ] Multiplayer (network protocol from the `Network` code).
- [ ] Gradual conversion of lifted functions to clean C++ (module by module, following the paths in `map_modules.py`).

## Iteration
An unhandled API or address ends with a `FATAL` message. Missing addresses go into
`tools/extra_entries.txt`, missing shims into `runtime/`.

## Status (2026-10-18)
**G** (historical host plan or observation). The lifted game starts on Linux and renders the main menu (800x600, 16 bpp, SDL2).
Resolved along the way: function discovery (pointers in data and constants in code), DF cannot be eliminated
as a dead flag, DirectDraw2 (SetDisplayMode takes 5 arguments), virtual register, MCI for movies,
GetVersionExA must validate the structure size.
Diagnostics: RT_TRACE_API, RT_TRACE_WAIT, RT_DUMP_DIR (PPM frame dump), RT_DUMP_SURF, RT_WATCH_ADDR,
-DAZTEC_TRACE_CALLS=ON (counter of guest function entries), -DAZTEC_HEAPDEBUG=ON.
To do: movies (ffmpeg), network, sound (to be checked), widescreen, Windows/macOS, second disk (CD2 music).

## Test mode (fast)
**G** (historical host plan or observation). The historical CLI used `RT_TIMESCALE=3`, `RT_SCRIPT` and `RT_DUMP_DIR`; any future launch must use the private-copy, dummy-SDL and namespace procedure in [EXPERIMENTS.md](EXPERIMENTS.md).
Script commands (time in ms of game time): `wait MS`, `move X Y`, `click X Y`, `rclick X Y`, `down`, `up`,
`key DIK`, `keydown DIK`, `keyup DIK`, `shot` (saves frame shot_NN.ppm), `quit`.
The scenario "create player" (`"utwórz gracza"`, game string: "create player") takes about 5 s instead of 25 s. Player files (*.plr) are created in the game directory.

## Transition from menu to mission (2026-10-18)
**G** (historical host plan or observation). Path: menu ("Wybierz gracza", game string: "select player") -> "Gra pojedyncza" (single player) -> "Kampania" (campaign) -> "Graj misję" (play mission) -> "odprawa" (briefing) -> "Start Game" (shield) -> mission 1 map.
Game directory layout (as on the discs): Maps/ (install maps + missions from both CDs), Video/, Music/, Res/.
The virtual registry returns the paths "D:\Maps", "D:\Video", "D:\Music" (the game uses the first 3 characters as the "CD drive").

## Widescreen (2026-10-18)
**V** (host patch; native display result unmeasured). Missions render at any resolution (verified at 1280x720): `./run_aztec.sh --res 1920x1080`.
Mechanism: the game keeps a resolution table (0x515510) and a whitelist of modes (0x495498); the runtime replaces entry 0
and 1280x1024 with the requested size and adds it to `EnumDisplayModes`. The map view scales, the side panel
keeps a fixed size (cosmetic: empty area below the panel). The menu (800x600) is cut out in the backend when the rest
of the surface is black. Not verified: very large resolutions (the game's static buffers).

## Helper menu (F9) and cheats
**G** (historical host plan or observation). SDL overlay (runtime/overlay.c, font from tools/gen_font.py) opened with the F9 key or `--menu`.
Settings (saved to game/aztec_settings.ini): resolution, window mode (window / borderless / fullscreen, live),
scaling (sharp / smooth), movies. Changing the resolution requires a restart (button in the menu).
In-game help: "Odblokuj wszystkie misje" (game string: "unlock all missions"; calls game function 0x43f560(15): saves progress 15 to the player file *.plr, verified)
and "Natychmiastowe zwycięstwo" (game string: "instant victory"; sets the flag like the built-in CheatWin key: DAT_004dd2f4+0x140+idx*0x69c, verified with the WYGRANA banner, game string: "VICTORY"). Keys.ini also has a built-in action `CheatWin <key>`.
Not done: adding gold (the field is not a plain int/float/double in the page array; to be tracked further with the snap/diff/fsnap/ddiff script commands in backend.c).
Test script commands: wait, move, click, rclick, down, up, key, keydown, keyup, shot, quit, f9, ovclick X Y,
cheat unlock|win, scan/scan16/snap/diff/fsnap/fdiff/fdiffd/fdifff/dsnap/ddiff/poke/pokeb/pokeside/dumpside (memory diagnostics).

## Mouse: virtual cursor (2026-10-18)
**G** (historical host plan or observation). The system pointer is locked to the window (SDL relative mouse mode), so it does not escape on any compositor.
Relative motion is integrated into a virtual cursor in image coordinates (independent of scaling and of the bars
when aspect ratios differ), with sensitivity set in the F9 menu (`mouse_speed` in aztec_settings.ini, default 100%).
The position is written to the game's cursor variables (0x4c5468/0x4c546c, 0x4c4f50/0x4c4f54); the F9 overlay draws its own arrow.
Debug: `tools/debug_run.sh` (log in debug/<time>/debug.log, categories mouse/window/render/dinput/overlay/state).

## Freeze in mission: causes (2026-10-19)
1. **G** (historical host plan or observation). No Winsock: the game runs a lockstep gate through the network layer even in single player. Without `WS2_32.dll`
   the network thread ended with an error and the simulation never started. Now runtime/winsock.c provides real UDP sockets
   (LAN as a side effect) and an IPX socket with a loopback (the game sends itself a test packet and waits to receive it).
2. **G** (historical host plan or observation). Bug in `timeGetTime`: a negative nanosecond difference cast to uint64 gave time jumps of billions of ms, and the tick loop
   computed `Sleep(140462661)` from it. Always compute time in int64.
3. **G** (historical host plan or observation). Fixed earlier: mutexes with an owner, VirtualAlloc with recovery, x87 precision 53 bits (RT_FPU53).
Diagnostics: `netstate` log (session/frame) in debug.log, `[sleep]` for Sleep > 300 ms, `[wait]` BEGIN/END for infinite waits,
`[event]` for Create/Set/Reset/PulseEvent.

## Label audit corrections (2026-11-23)

Evidence checked: Reference: 0x43d5d0/0x4577b0 establish the native factory/loop, not the plan milestones. Clone/host: `runtime/aztec_rt.h`, `runtime/core.c`, `ddraw.c`, `backend.c`, `overlay.c`, `winsock.c` and current CMake/native CI definitions. Resolution patches and guest-memory reservation are host construction facts; old run timing, coverage and rendered outcomes remain G.

1. Separated the historical runtime plan from the current independent engine and native CI definitions.
2. Removed the unsafe direct shared-installation launch example; no reference launch was performed in this audit.

No original executable or reference runtime was run for this audit. Historical
validation totals retain their original dates; they are not this
audit's test results. Private captures, derived source and probe output remain
outside the tracked documentation.

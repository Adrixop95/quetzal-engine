# Mission script presentation (task 126)

Status: independent implementation and synthetic checks; **needs Windows live
measurements**. V means inspected in private original code/data, not a live
parity result. G means engine policy, inference or unresolved mapping. No original
process was launched. Original scripts, strings, assets and captures stay private.

## Evidence and implementation

| Area | Evidence and result |
| --- | --- |
| View jump | V: `f_00402b00` converts tile coordinates to pixels and centers the viewport, without interpolation or duration. `centerView` now jumps at the tile origin. V follow-up: signed pixel conversion and native lower-then-upper clamps retain the four-pixel horizontal and one-pixel vertical margins, including undersized maps. |
| Vision cameras | V: `f_00402330` and continuation `f_0040236d` look up the side's indexed record and construct a stationary vision object. `f_0043e890` places it at the tile center. Activation now stamps the owning side's vision without changing the viewed side or view position. |
| Camera size | V: the third record value selects a mask within the eight vision masks; `f_00402a70` returns it. The clone uses the existing runtime sight resources, without embedding private masks. |
| Duration | V: activation stores its second argument as lifetime. `f_00402a50` decrements positive lifetimes and destroys the source at zero; nonpositive values persist. V follow-up: visual updates follow scripts (`0x4577b0`, `0x43edc0`), so the activation update consumes the first lifetime tick. Duration one expires before vision stamping. |
| Repeated activation | V: each activation constructs a source and replaces the record's latest-object reference. `f_00402f10` deactivates that referenced object only. Older live activations persist. The clone retains latest-record tombstones so expiry cannot redirect deactivation to an older source. |
| Follow and cinematic lock | V: these handlers and their parser do not supply moving/following views or input locks. Tactic lock is a separate gameplay action. No cinematic lock is inferred from camera activation. G: other native cinematics remain unmeasured. |
| Print | V: `f_00402df0` dispatches ordinary print to `f_00462760`: a bounded ring retains fifteen prints with four-second wall-clock deadlines. Drawing near `0x004681a0` spaces expired removals by one second and starts text near the upper-left corner. The clone now uses bounded UTF-8 messages and presentation-time expiry, including while paused. V follow-up: font slot 4, palette state 0, oldest-first, unwrapped entries at (10,20) with 18-pixel pitch and viewport clipping. G: native debug-overlay suppression remains missing; scaled layout needs captures. |
| Alternate print | V: `f_00436f00` consumes `printStringR` without registering an action in the inspected executable revision. Ordinary print registers an action. Deliberate discard is retained and tested; showing both would invent behavior. Other language revisions remain unverified. |
| Timers | V: `f_00401a10` increments an unstopped counter; start/stop change its stop flag. `f_00401cb0` assigns a value without clearing that flag. Kill deletes the timer. Set now preserves an existing stop flag. V follow-up: pool prepend/traversal confirms lazy creation/recreation at running zero and first increment on the next update; the separate action latch retains its one-update delay. |
| Timer display | V: the inspected action parser provides set/start/stop/kill, without a display action. No countdown HUD is inferred from a script counter. G: independent native mission timer displays remain a live question below. |
| Sound/music actions | V: the inspected top-level and side action dispatchers have no sound/music action grammar. Mission music uses mission globals; audio also has non-script producers. Existing music startup, track progression, scene audio and shutdown remain in use. No invented audio commands or print-to-speech mapping. G: briefing/result sound IDs and timing need captures. |
| Briefing/objectives | V: private campaign documents have multiple titled sections with goals distinct from narrative/orders. G: use their final section as goals; native UI loading of these files is unestablished. The pre-mission briefing retains all sections and offers a separate Objectives view. Pause opens objectives alone. G: editor notes supply a custom-map fallback. No completion checkboxes or inferred trigger-to-text links. |
| Mission end | V: victory/defeat handlers update per-side outcome state. G: clone terminal modal, retry, continue, profile policy and final heading retain existing engine behavior. Existing synthetic flow checks cover victory, defeat, retry, objectives, next briefing, final return and progress persistence. Native delay, statistics, audio and ending sequence remain unmeasured. |

Evidence was read from private lifted functions and their instruction-label
comments; local decompilation/disassembly helped resolve calls. No passages were
copied into public code. This supersedes the historical jump-only camera
interpretation in `campaign_scripts.md`.

Camera definitions and active lifetimes now participate in simulation hashes,
independently of the viewing side, and replay reconstructs them from scripts.
Engine replay-save compatibility advances from version 10 to 11: incompatible old
replay saves are rejected. Message deadlines and objective views remain outside
simulation hashes. Original-save cameras remain a save-import dependency; this
change does not reconstruct or execute original saved script state.

## Synthetic verification

`script_unit` checks finite/indefinite/repeated/expired/deactivated cameras,
foreign views, invalid indices, mask limits, actual fog stamping, hash sensitivity,
replay reconstruction/continuation, stopped timer assignment, ordinary/alternate
prints, bounded queues, presentation-time expiry and reset. Worlds, scripts and
masks are invented. `campaign_ui` checks separate briefing/objective text,
two/three sections, custom fallback, missing text and scroll/close controls.
Existing campaign flow, audio, determinism and save checks cover surrounding
transitions. These are engine contracts, not native parity or fifteen completable
original campaigns.

Baseline: Release build passed; CTest passed 63 entries and skipped six (five
private-data entries and restricted UDP processes); pre-commit passed.
Final: Release build passed; CTest passed 63 entries and skipped the same six,
with no failures. All tracked-file pre-commit hooks passed, including the added
synthetic campaign flow controls; the new document also passed its explicit hook
run. `git diff --check` passed. V: the read-only inventory parsed all fifteen
private campaign scripts without missing files, parse errors or unsupported
action sites; mission 6 retains two missing-trigger-target warnings. This is
static coverage, not mission completion or reference execution. No private game
simulation was run. The synthetic hash golden values were updated because the
new camera collections intentionally change the simulation hash contract.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DAZTEC_GEN_DIR=
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
pre-commit run --all-files
pre-commit run --files docs/mission_script_presentation.md
git diff --check
```

## Windows maintainer run sheet

G: every row is pending. Use the installed Windows original, separate writable
installation/profile copies and normal mission input. Do not run the original in
this restricted environment or bypass namespace rules for a later Linux probe.
Capture two independent native runs per case with identical mission, language
revision, difficulty, speed, resolution, initial profile and input. Keep original
text, audio, numeric traces and captures private. Return reviewed behavioral
conclusions labelled V, with private evidence identifiers.

| Case | Capture and control |
| --- | --- |
| View jump | Reach a normal `centerView` site; record preceding/following frames and viewport edges. Distinguish tile origin from center, include map edges and align other actions to simulation updates. |
| Camera lifetime/fog | Reach activation without moving the view. Record side fog, minimap and enemy visibility before activation and around expiry, at two speeds. In a separate private editor map, include positive, zero and negative durations. Distinguish updates from milliseconds. |
| Repeat/deactivate | Activate the same camera twice with different durations; deactivate before and after the latest expires. Test whether an older indefinite source survives. Retain failures for missing indices. |
| Input/follow | During cameras and actual cinematics attempt keys, edge scroll, drag, minimap, selection, commands, pause and Escape. Track any followed entity. Identify the producer of any lock rather than assigning it to activation. |
| Print queue | Record ordinary and paired alternate prints: order, position, font/color, wrap, deadline and retirement spacing. Repeat paused and at two speeds. Use a separate private editor map for more than fifteen prints. Record the exact executable/language revision. |
| Timer HUD | At timer set/start/stop/kill sites record the whole HUD and objectives. If a counter appears, measure label, direction, format, first change, pause/speed response and disappearance. Otherwise record the checked mission/site and interval without generalizing absence. |
| Briefing/goals | For all fifteen missions record selection, narrative/order/goals navigation, intro playback/return, Start, pause objectives, scrolling, close/resume. Establish actual text source and section mapping, including two-section cases and any completion marks. |
| Audio | Record launch, prints, objectives, timers, victory, defeat, retry, successor and final ending. Distinguish music/effects/speech and repeat with each channel muted. Supply evidenced resource IDs privately before adding producers. |
| End screens | Obtain normal-input win/lose routes. Record terminal trigger/update, first terminal frame, delays, cinematics/statistics, buttons, keyboard defaults, retry reset and successor briefing. Mission 15 needs the complete ending/movie/credits/menu and profile sequence. |

Incomplete dependencies: aligned native UI/audio/tick observations, validated
winning routes, native timer display evidence and original-save camera mapping.
The implemented work does not depend on them. Exact native end screens, new
cinematic locks, new audio producers and any timer HUD await those observations.

Task 181 follow-up (2026-11-25) supersedes the historical validation and replay
version above: see [camera/interface gaps](script_runtime_gap_list.md#camera-and-interface-follow-up-2026-11-25)
and the [aggregate report](script_approximation_report.md). The earlier tests'
activation-tick lifetime expectations were corrected, not measured live.

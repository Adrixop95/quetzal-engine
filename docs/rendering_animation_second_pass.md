# Rendering and animation second pass

Status: static audit and synthetic rendering controls; **needs Windows measurements**.
No original executable was launched. Changes remain uncommitted because git
metadata is read-only. No original pixels, animation banks, script bodies,
measurements or executable-derived code are included.

Evidence labels apply throughout this document: **V** means a fact established
by inspecting private original instructions or resource structure; **G** means
an engine assumption, synthetic control or unresolved implementation policy.
Static inspection does not establish live timing or pixel parity.

## Audit and changes

| Area | Evidence and comparison | Result / remaining work |
| --- | --- | --- |
| Per-type animation and facing | **V:** the kind-specific selectors documented in `sprites_mapping.md` distinguish Marine, Spider, Building, Zeppelin, Truck, Bird, Headtruck, Defence, Boat, Cannon and Johnka; Camera has no body. Animation durations belong to the installed bank, including zero-duration holds. **V:** Truck `f_004607b0`, Zeppelin `f_004718b0` and Bird `f_00407a60` compare the rounded dir32 cell with the dir8 centre. | Fixed the provider's exact-angle gate: a facing within the centre's dir32 cell can cycle. **G:** `cycle_ready` remains an external readiness gate; Bird steering cooldown, turn dynamics and Cannon hop restarts need producer evidence. The new gallery tests all 90 type IDs, five states, eight directions and three ages with invented records. |
| Attack and secondary animation | **V:** Truck fire uses directional one-shot slots; Headtruck `f_0042c740` uses its finer body angle; turret and body sequences are separate, as detailed in the existing mapping. | Existing independent body/attack clocks and turret fallback are retained. **G:** simulation firing cadence and phase synchronization are not proven by the gallery. |
| Death and corpses | **V:** the original mapping distinguishes Marine corpse frames from separate vehicle explosion/debris and radius-selected building rubble. **V:** actual resource availability determines whether a requested slot draws anything. | Existing hold, missing-body and decoration controls remain. **G:** engine corpse removal, stable variant selection and unknown building radius still require live comparison. The gallery deliberately supplies all slots; it does not assert that flight resources contain death frames. |
| Construction | **V:** `f_00410c30` seeks by HP and frame count, with an invisible prefix; completion uses the built sequence. | Existing synthetic hidden-prefix and threshold tests remain; all three sampled gallery ages use HP-independent built loops. Additional gallery cases check hidden, intermediate and completed construction rectangles. **G:** unfinished/cancelled structure lifetime needs observation. |
| Damage, fire and smoke | **V:** `f_00410cc0` computes positive-HP damage stages, creates independent fire at ground Y+1, and at the most damaged stage also creates effect 105 at height 80 on a three-update schedule with varying horizontal placement. | Existing fire thresholds and independent effects are retained and tested. **G:** the engine has no equivalent severe-damage smoke producer; effect 105 also has other callers, so resource ownership alone cannot justify a generic always-on smoke emitter. RNG/phase synchronization remains open. |
| Projectiles and effects | **V:** installed animation records supply frame timing and height; the original effect classes have different position/height sources (`render_rules.hpp`, `effects.hpp`, existing source audit in `sprites_mapping.md`). | Synthetic gallery exercises effect rendering, frame completion, ground shadows and altitude. **G:** flight physics, weapon phase, collision and impact scheduling require the combat/observer dependencies; render success does not establish those rules. |
| Shadows and altitude | **V:** body position subtracts hotspot and applicable height; shadow stays on the ground. **V:** Spider and scripted transports use zero body height; aircraft use object altitude. | Gallery checks actual queued rectangles and rendered pixels with asymmetric invented hotspots. **G:** aircraft altitude producers, original shadow lookup/blending and fractional rounding remain unresolved. |
| Terrain transitions and water | **V:** `f_00465a50` reads the base tile ID from the low map-cell half; animated tiles select a stacked cell using the unpaused view counter (`f_00463980`). Overlay wind uses a separate counter and regional strength. **V:** private Tiles0/Tiles1 headers include four/eight-cell vertical stacks. | Replaced simulation-tick/2 water and selection phases with an explicit view counter. **G:** one engine presentation is treated as one original view update; wall-clock rate is not measured. Existing transition art is selected from runtime map data; no transition pixels are generated or embedded. Regional wind production remains missing. |
| Draw ordering | **V:** `f_00464b10` and the sort in `f_00465a50` put underlays first, then ground Y ascending and X descending, finishing each item's layers. | Existing shared-list sort controls remain. **G:** stable order among equal underlays is an engine policy. Fixed selection ordering: each sorted item now draws its ring before its own layers, matching the inspected per-item placement; a later ring no longer disappears beneath every earlier body. Fixed anchor culling margins can miss unusually tall/offscreen sprites; this requires a separate bounds/visibility producer change. |
| Selection and health | **V:** the original view chooses ellipse/health art by display size and uses a view-clock ellipse frame. The existing format audit identifies ellipses and health arcs in common.xbe. | Selection phase and per-item ordering are corrected. **G:** the engine still chooses only unit/structure ellipse sizes and draws rectangular health bars with fixed offsets; those remain explicit approximations. Their synthetic pixel checks protect existing behavior, not original parity. Original display-radius and health clipping mappings need completion. |
| Cursor art | **V:** `f_00464290` uses a direction lookup for ordinary scrolling. Runtime resource headers confirm separate ordinary and blocked arrow sets; ordinary NE art is present in this installation, superseding the older missing-21 note in `formats.md`. **V:** `f_00464550` advances at most one frame after an elapsed interval of at least 80 ms. | Fixed reversed scroll directions and ordinary-scroll art selection. Every cursor's hotspot/scale/art choice is checked with invented colors. **G:** the clone's absolute time/80 phase differs after a delayed frame; blocked-edge detection, command modes and cursor phase resets still need live controls. |

The factual selector inventory and resource identifiers already present in
`src/engine/data/` were not regenerated. No private bank values, numeric resource
dumps or art were turned into fixtures. The synthetic bank has deliberately
uniform durations and dimensions that differ from real banks.

## Deterministic headless gallery

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DAZTEC_GEN_DIR= -DGAME_DIR=
cmake --build build --parallel 2
ctest --test-dir build -R '^(render_states|render_contracts|animation_headless)$' --output-on-failure
```

`render_states` generates temporary invented XBE archives, uses SDL's software
renderer with dummy drivers, draws a text label for each case, and reads pixels
back into memory. Its expectations select slots independently of the production
selector. It checks the animation resource/frame identity and the actual screen
rectangles queued by `SceneRenderer::observe_sprite`, including ground shadows,
body height and installed hotspots. The observer is optional and does not mutate
animation clocks. The test emits diagnostics on failure and a case count on
success; it writes no gallery images. **G:** these are reproducibility and
implementation controls, not measurements of original drawing.

Existing `render_contracts` and `animation_headless` add damage boundaries, hold
frames, timed prefix completion, independent turret clocks, ordering, light masks
and visibility controls. No gather/cast slot is guessed: this checkout lacks
corresponding action-state producers and verified dedicated slots.

## Windows maintainer run sheet

All rows are **G / pending measurement**. Use the legally owned Windows install;
retain captures, logs, save files and resource-derived tables privately. Do not
run the original executable in this restricted environment. Linux reference
experiments, if performed later on an authorized host, must retain the private
installation copy, dummy SDL and `unshare -rn` isolation from EXPERIMENTS.md.

Record executable/version, native resolution, display scaling, mission, save,
side/color, speed, pause state, camera, input route and timestamp clock. Repeat
each original scenario twice from the same initial state. Report observed ages,
resource identity where an audited observer exists, and unscaled screen bounds;
unknown frame IDs remain unknown. Do not infer IDs from expected clone output.

| Case | Capture / measurement | Acceptance and producer dependency |
| --- | --- | --- |
| Every installed unit type | Idle, travel, turn, attack stationary/moving, all eight directions and boundaries on either side of each dir32 centre. Include age zero, each transition, completion and one later tick. | Body/turret IDs, first/last frame ages and body/shadow bounds. Separate resource duration from simulation speed and render rate. |
| Death | Three death variants where observable, ground vehicle, aircraft at altitude, building sizes; continue beyond last frame and corpse removal. | Disappearance, retained corpse frame, rubble/explosion/debris bounds and lifetime. Record random choices; do not prescribe them. |
| Construction and damage | Hidden prefix, immediately before/at/after each HP-stage boundary, built loop, cancellation; damage at each quarter boundary and repair in both directions. | Construction/overlay/turret visibility, independent fire clock reset, severe-damage smoke cadence/height/offset. Needs audited HP/radius and RNG observations. |
| Weapons | Arrow, axe, ball, flame, splash, beam and shockwave; impact on ground/air targets. | Launch/impact ages, trajectory height, frame completion, body/shadow bounds, ordering and light. Combat producer acceptance is separate. |
| Overlap and clipping | Equal-Y reversed-X units, tree/building/aircraft overlap, underlays, selected units, anchors outside each viewport edge. | Visible order and clipping; keep exact pixels and tolerant metrics separate. Requires original bounds and display-radius mapping. |
| Selection/health | All available ellipse sizes, full/partial/critical HP, multiple overlapping selections, zoom/scaling and altitude. | Sprite choice, hotspot, HP arc clipping, overlay ordering and clock. Rectangular clone bars cannot count as a match. |
| Cursor | All scrolling directions, blocked map edges, modal/nonmodal screens, move/attack/build contexts; one deliberate frame stall. | Art/hotspot/bounds, elapsed-time phase, reset behavior and post-stall frame. |
| Water/transitions/wind | Both tilesets, static and animated shores/transitions, still/slow/fast wind; pause/unpause and two presentation rates at the same simulation speed. | View-counter phase, wall-clock cadence, pause behavior, corner transitions and wind-region strength. |

The missing synchronized render observer and outstanding simulation producers
prevent a full second-pass parity verdict. These do not block the independent
synthetic work above. No campaign completion or original pixel parity is claimed.

## Verification

Baseline: Release build and 69 CTest entries completed successfully (63 passed;
five private-data entries and one restricted network entry skipped). Pre-commit
passed using the existing cached environment; the attempted empty cache
could not fetch GitHub because network name resolution was unavailable.

Final Release configure/build passed. All 70 CTest entries completed without
failures: 64 passed, five private-data entries and the restricted network entry
skipped. The 10,800-case gallery and its construction, effect, selection-order,
health fallback, camera, water, wind and cursor controls passed. Pre-commit
passed for tracked files and explicitly for the two new files. No private-data
process or original executable was launched; skipped checks add no parity evidence.

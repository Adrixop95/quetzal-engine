# Engine: status of the simulation rules (verified / measured / placeholder)

## Marine shot period (2026-11-22)

**V resource facts / clone construction / synthetic checks:** Marine cooldown
now explicitly uses `max(reload + 1, attack animation ticks + 1)` for finite
resource sequences. The existing retained-target gate already enforced the
animation limit. Footman remains 10 ticks, Rifleman 21 and Demolisher 31;
Hypno and Chief remain animation-bound at 31 and 12. Invented Footman/Rifleman
tests cover all facings, both limits, equality and missing-resource fallback.

**G unresolved original timing:** The measured Rifleman 26..27-tick hit gaps
are not explained by the clone's 10-tick attack sequence. No additional tick
factor is inferred and no original executable was run. See
[shot_period_implementation.md](shot_period_implementation.md) for the complete
Marine duration/period table and evidence limits. This supersedes older
statements below that describe Marine scheduling solely as reload+1.
## World rendering rules (2026-11-22)

**V rules / synthetic implementation checks:** shared underlay/y-ascending/
x-descending sorting is retained. Explicit collected underlays draw before sorted
items; each item now finishes its health art and marker before the next item.
Selection/health palette 3, the documented size table, strict HP bands and clipped
bar width are implemented. Placement can draw a team-coloured building image in
49 separately clipped 48-pixel cells using shade rows 20/21/22 after the drag box.
`--original-look` selects tick positions, logic-rate presentation and per-pixel
palette lighting: uniform terrain rounds, the light surface truncates, gradients
use bilinear ordered quantization and bodies sample their ground row. Flash
presentation inputs use flat row 19 on odd view ticks; shadows retain shade key 12.
Interpolation and widescreen remain available in the ordinary presentation.

**G / P integration:** native display-size/bar rectangles, flash/marker and
collected-underlay producers, exact dither thresholds, shadow palette, daylight
palette ramps and placement sprite/status mapping remain incomplete. Missing art
retains diagnostic fallbacks. Path dots and original menu redraw/dim-box rules
remain gaps. No original executable was run and no exact screenshot parity is
claimed. See [render_implementation.md](render_implementation.md) for V/G labels,
mode controls, construction tests and remaining dependencies. This entry
supersedes the older two-size ellipse/final rectangular-bar rendering inventory.

## Towers, aircraft and specials completion (2026-11-21)

**V rules / synthetic implementation checks:** Defence now acquires on every
ready tick, uses descriptor d4's whole-step heading gate, latches/clears targets,
uses weapon reload without Marine's extra tick, and releases resource-backed
base/turret shots on animation markers with descriptor d63 launch height.
Non-positive-damage victim categories cannot become tower targets; construction
blocks firing and no garrison/ammunition gate is added. Building d59 protection
now applies consistently to every combat damage callback, including blast and
projectile impacts.

**V rules / synthetic checks:** Bird releases and re-picks after a nose-on shot,
including Direct mode. Zeppelin retains range-only bombing targets and follows
ordered destinations independently. Both remain airborne and rise one pixel per
tick; Zeppelin's initial zero height corrects the prior shared 30-pixel start.
Existing Snowman attack-delay, sixteen-explosion volley, fire-stream ally quarter
factor, descriptor 8.8 resistance and plain Hypno-tower damage remain covered.
Sweep values 20/30 denote angular sector widths within a 96-pixel reach, as
established in the ground-weapon audit, rather than pixel radii.

**G:** Native tactic scoring, activation RNG, exact flight paths, spatial query
ordering and native pool/animation phases remain provisional. Missing animation
resources retain immediate release; arrows use linear height interpolation.
No original executable was run and no live parity is claimed. See
[specials_implementation.md](specials_implementation.md) for evidence labels,
regressions, remaining dependencies and actual verification results. This entry
supersedes older statements below about generic tower timing, held Bird targets
and missing protected-building combat callbacks.

Simulation: `src/engine/sim/game_sim.*` (12 Hz, fixed-point harvests and original float32 price factors, driven by the command queue in `commands.hpp`). Data comes from `docs/data/*.csv`, loaded at run time (`rules.*`). The reference specification is [rules_summary.md](rules_summary.md) with details in [rules_types.md](rules_types.md) and [rules_ai.md](rules_ai.md). New reference findings below do not imply engine implementation changes.
Movement and pathfinding: `src/engine/sim/path/` (movement agents; the interface is swappable: `GameSim` only uses `MoveSystem`).
The [L3 iteration](rules_movement.md#10-l3-disconnected-selections-and-observation-budgets-2026-11-01)
fixes unreachable slots for selections spanning disconnected regions: a six-unit
synthetic diagnostic improves from 2/6 settled in 300 ticks to 6/6 by tick 30,
with jam windows falling from 31 to zero. Tests cover replacement orders and
deterministic hashes. The 48 existing course/count results retain hashes and
movement metrics (656 arrivals, zero stuck). Delayed benchmark orders now receive
the full per-order observation budget; comparison reports include stuck/idle
metrics and handle absent arrivals. These are engine/tool construction checks.
Archived open/gap observations remain the only accepted reference route results;
the fresh isolated probe failed at namespace setup before launch. L3 tolerance
acceptance, formation/boat/flight parity and the V02/V04/V05 dependencies remain
open; no measured reference steering parameters were introduced.
The [path quality regression](rules_movement.md#9-wall-side-destination-regression-2026-10-29)
keeps group destination slots in the anchor's visible arrival area. This corrects
wall-side stalls in synthetic engine tests; reference formation parity remains open.
Legend: **V** = verified against the original (code or screenshot), **M** = measured on the recompiled original, **P** = placeholder (invented, to be replaced).

## Original wave search (task 152, partial, 2026-11-21)

V specification / engine construction: ground point queries now use the round
wave described in [pathfinding_wave.md](pathfinding_wave.md), with framed u16
labels, departure delays 1/2/4, equal diagonal costs, unrestricted corners,
documented expansion/extraction order and Chebyshev-ring fallback. Tests cover
fog/occupancy overrides, blocked starts/goals, radius outlines and goal lists.
Water and legacy field comparisons explicitly retain the old octile query.

V rules / synthetic candidate only: an opt-in tile-reserving mover implements
one owned tile released/reserved at step start, straight/+45/-45 side tests,
two-requester admission with retry priority, eight-step memory, Marine norm and
velocity arithmetic, and Truck/Headtruck turn/brake helpers. Candidate hashes
include pending paths, reservations, admission, retries, ownership/fog and vehicle
state. These rules are **not promoted to GameSim's default movement**.

G integration: tactic anchors, local goal lists, connected-member look-ahead,
formation scans/timers, group margin-class selection, side-fog alignment and vehicle
model scheduling are unresolved. The documented three-way extraction tie can
select a blocked non-candidate on a synthetic map; it reports an explicit failure
instead of returning a malformed route. Growable fronts and a label-wrap guard
are deliberate engine safety extensions, not verified reference behavior.

V engine measurement: the candidate has 585/656 arrivals, 71 lost and 53 stuck
across the 48 benchmark cases. A default-promotion experiment failed seven
existing suites, including sustained AI and faithful playthrough stress (an earlier candidate
that applied margin layers to members). Base/member versus margin/group layers
are now explicit; full tactic promotion remains open. The
game therefore retains disc/flow movement; default benchmark metrics and all 48
hashes remain unchanged (656/656 arrivals, zero stuck). Existing acceptance
assertions are retained. The original executable was not run. Only 14 original
CSV rows are usable, including seven explored rows; failed captures do not
establish movement behavior. See [implementation and before/after tables](pathfinding_implementation.md)
for V/G boundaries and the remaining promotion requirements. Full original
pathfinding parity and the requested default replacement remain incomplete.

## Cursor confinement (task 39, 2026-10-29)

**Engine presentation policy; reference parity unverified.** Interactive focused
gameplay requests SDL mouse grab by default, with absolute coordinates and no
keyboard grab. Focus loss, minimization, holding either Alt key (including
Alt-Tab), pause menus, view/simulation pause, mission exit and the front-end
release it. Resuming focused gameplay reapplies the session preference. F9
toggles that preference without key-repeat; `--no-mouse-grab` starts disabled.
Scripted playtests and screenshots never request confinement. This state does
not enter simulation commands, saves or hashes.

Before the fix, an isolated dummy-SDL reproduction using the existing Window
reported no mouse grab request (exit 1). The new synthetic `mouse_grab` CTest
checks SDL request flags in windowed, borderless and exclusive fullscreen modes,
focus/pause/Alt transitions and toggling. It checks continuous camera movement
from a stationary cursor at each edge and two corners, including a 2x HiDPI
coordinate transform and the outer right edge beyond the panel. Unfocused,
outside-window and Alt-held inputs produce no edge scroll; focus loss also
clears pending click/key edges and motion to avoid stale input on resume.

The dummy backend verifies requested SDL state and camera behavior, not physical
desktop confinement or window-manager Alt-Tab handling. Those still need a live
windowed/fullscreen check. A reference probe with a private game copy and dummy
SDL inside `unshare -rn` failed at `ip link set lo up` with `Cannot open netlink
socket: Operation not permitted`, before the game launched. No original-game
cursor behavior or reference record/replay result was obtained. Private probe
inputs and output remain under ignored `work/cursor-grab/`.

Validation: Release build and pre-commit gates pass. The full 32-entry CTest run
has 27 passes and five expected private-data/network skips. Separate `formats`
and `engine` checks pass with the complete private map copy under dummy SDL in
`unshare -rn`; a bounded mission 1 engine screenshot smoke also passes there.

## Core simulation
| Rule | Status | Source / notes |
|---|---|---|
| 12 Hz step, 24.8 fixed-point positions, state hash after every tick, replay from the command log | V (construction) | no comparison with the original yet; the engine hash differs from the recompilation's hash |
| State hash contract 2 covers future-affecting mutable state under identical immutable rules/sight inputs | engine construction | Includes queued commands, object/effect previous positions and source, world/script-side identity, town/side/path/resource inputs, per-side outcomes, script program and collection lengths, navigation blocker counts/configuration/cached field costs. Local presentation and diagnostics are excluded; see [audit and limits](determinism.md). This is not a content-compatibility digest or reference hash |
| Command types (PCT_FRAME_*) and their numbers | V | `docs/object_types.md`. Implemented: PAUSEON/OFF, CREATEGROUP, ADDTOGROUP, MOVE, ATTACK, BUILD, BUILDMODE, DESTROYHERE (list toggle), GARRISON, REPAIRMODE, QUEUEKILL, PAUSEBUILD. Partial engine conventions: SELECTGROUP, ATTACKMODE, SELECTBYNO, FOLLOWTEAM (see V10 update) |
| Stop (0x50) and AttackMove (0x51) | P | engine extensions; the original has no such packets |
| Command coordinates / tiles from packets | engine | clamped into the map before any fixed-point conversion; invalid tiles are ignored |
| Command order inside a tick: ascending sides 0..6, then submission order | V side range/order / G packet mapping | Native one-packet ring, active-side gate and latency remain open; engine batches remain supported. See [RNG/tick implementation](rng_and_tick_order_implementation.md) |
| Mission RNG and object phase order | V generator/construction / G consumption parity | Shared seed-zero MSVC stream for supported combat, decoration, planner and scene sound choices; logical unit pools walk newest first with LIFO reuse. Effects precede scripts; projectiles follow units; completed time advances last. Missing loading/wind/input/descriptor producers and grouped town/movement/sound phases prevent full sequence parity. Replay semantics 14 / AZC2 |
| Descriptor primary/max and secondary HP | V fields / engine checks | All nine descriptor families loaded; mission/spawn maxima never depend on other instances. Positive mission HP is preserved. Original-save +0x3c is secondary HP; maximum comes from rules at simulation initialization. See V04 below |
| Descriptor regeneration | V inspected thresholds/countdown/overshoot / P facility producer | Marine, Truck, Zeppelin, Bird, Headtruck, Spider and Cannon recover below natural thresholds, or full HP while the hashed service countdown is positive. Pre-add check permits overshoot; countdown expires after the update. Facility coverage/refresh and exact tick alignment remain open |
| Scripted object health and placement identity | V zero maximum getters / engine identity checks | Boat, Johnka and Camera maximum getters return zero; Submarine records create stationary decoration controllers, not combat units; their scenario actions now use the adapters in [transport_rules.md](transport_rules.md). Town membership uses section numbers, including sparse IDs; team scenario/tactic, defence and vessel provenance remain distinct. Live reference identity still depends on V02 |
| Descriptor movement speed | V getters / synthetic integration | Marine d5, Headtruck/Spider d7, Zeppelin/Truck/Bird d5 in px/tick, converted to 24.8; Bird d5 is its maximum cruise speed. Historical averages no longer override established descriptor speeds |
| Measured speed fallback | M | `speed_measured.csv` (mean mid-run speed), used only without a verified speed getter; these averages include steering/sampling effects |
| Bird speed and transport motion | V documented ferry rules / G remaining policies | Bird and unknown speeds default to 8 px/tick. Ferries move on the fixed NE-SW diagonal at five pixels per axis, stop before the next Small-walkable tile, and use independent boat states/ferry phases. Fresh landing groups, requester survival, boat-loss cargo death, unrestricted phase activation and submarine decoration replacement have synthetic coverage. See [ferry_implementation.md](ferry_implementation.md) for V/G boundaries; live parity and Johnka scheduling remain open. |
| Unit radius (collision disc) | P | 14 px, large 20 px |
| Movement class small / large (`.dat` 1 = small only) | V code facts / synthetic integration | Marine d11 low byte selects small when nonzero; Spider/Truck/Headtruck use large. Bear is small; Knight/Bizon/Snowman/Elefant large. Missing Marine descriptors retain an explicitly provisional small fallback |
| Air orders and ground collisions | engine / P reference integration | Aircraft use straight fixed-point movement without terrain/contact blocking, including engine attack-move orders. Ground agents retain continuous disc avoidance/pushing and flow fields. Reference tile reservations, vehicle turning, formation/path decisions and exact arrival timing remain unimplemented or unmeasured; see [V05 progress](rules_movement.md#8-v05-partial-integration-2026-10-28) |
| Price and build time | V formula / synthetic checks | `types.csv` with outpost/developed hall, runtime speed, hostile-to-everyone and computer branches. Float32 factors evaluated before truncation after +0.5; developed fast computer 100 -> 160 gold / 53 time units. No lifted 161 rounding |
| Production countdown | engine / O parity | Computed time units drive tick countdowns; one payment at production start, none at spawn. Exact original start/completion tick ordering is unverified |
| Build tree (who can build what, prerequisites) | V | `buildtree.csv`; ordinary prerequisites require completed objects in the producing town, A entries use side-wide counts. Only map-construction entries reject completed/in-progress/queued copies in that town; flag-0 resource structures and defences are repeatable. Hall upgrades retain their separate reservation guard; DisabledID and adjusted affordability are checked |
| Town upgrades and non-map structure production | engine / V tables, P placement | Flag-0 structures use adjusted costs/timers and the production queue. Hall completion preserves identity and installs its race/level tree; farms/mines/defences can be queued repeatedly, are automatically placed on free tiles and activate descriptor effects. Exact reference placement, HP transition and completion ticks remain unverified; see validation below |
| Side `DisabledID` blocks menu entries | V | `docs/formats.md` |
| Weapon damage | V documented arithmetic / engine synthetic checks | Marine d7 and Defence d59 select weapon IDs. Classes 1/2/13 apply each matching 8.8 multiplier in sequence, then armor term; arrow zero-air suppresses armor. Classes 4/5/6/10 constants implemented; 8 delegates and 12 sums known child damage. Historical duel entries are diagnostics only. Unsupported weapon behavior retains explicit gaps; see V04 and the siege investigation |
| Weapon reload getters and cooldown | V getters / partial scheduling | Ball/Axe use d2/d3 for reload/range; class 12 uses its first child. Siege launch cooldown uses reload, following Truck/Headtruck decrement-before-check; Bird/Zeppelin, Cannon, class 8 and class 11 also use their inspected reload; other weapons retain reload+1. Animation-marker launch delays and exact hit intervals remain unverified |
| Projectile and unresolved weapon behavior | partial | Ball/Axe and paired siege launchers now fire descriptor payloads; class 9 supplies chained category damage. Ball impacts evaluate a spatial blast per victim. Class 11 now scans a source-facing flame segment; class 8 schedules sixteen explosions over ten updates. See [fire_air_siege.md](fire_air_siege.md) for evidence and remaining geometry/timing approximations |
| Weapon range | V getters / P geometry and fallback | Established weapon getters replace generic ranges; Euclidean centre distance plus mobile source radius and placeholder target radius/footprint remains. Types without loaded weapon descriptors retain 56/288/336 px placeholders |
| Sight data and visibility predicate | V facts / synthetic checks | Descriptor mask IDs replace the scalar radius. Unit brightness must exceed 32 in 8.8 interpolation; buildings test four bounding corners. Ownership does not bypass raw fog; daylight and frame glow contribute. User lighting resources enable these rules; asset-free acquisition uses the mode-dependent combat envelope (G geometry). See the [sight and fog notes](sight_fog.md) |
| Auto-attack of visible enemies (idle and attack-moving units) | V visibility gate / P tactic integration | Side brightness >32, hostility and known weapon eligibility gate candidates. Nearest enemy, lower-id ties and four-tick per-object scanning remain placeholders; original tactic bounds, 100-entry arrays and 15..23-tick scans are not implemented |
| Sight data and visibility predicate | V facts / synthetic checks | Descriptor mask IDs replace the scalar radius. Per-side interpolated brightness must exceed 32; daylight and frame glow contribute. User lighting resources enable these rules; asset-free acquisition uses tactic bounds and listed enemies with permissive visibility (G fallback). See the [sight and fog notes](sight_fog.md) |
| Auto-attack of visible enemies (idle and moving units) | V rules integrated / G assignment | Persistent side-local tactics scan member/anchor bounds widened -6/+7 on a 15–22-think timer. Forced damage reports list enemy tactics in every mode; automatic ground steps use the 4/8/8/6 tile leash. Nearest eligible assignment, independent RNG and visibility details remain G. See [unit behaviour implementation](unit_behaviour_implementation.md) |
| Death and corpses | P / V effect metadata | Marine corpses remain 480 ticks; vehicles spawn 152 and ground debris 113, known-radius structures select rubble 116/115/114 for 250 ticks; unknown radii retain provisional rubble 116. Airborne altitude remains open |
| Simulation fog brightness and exploration | V formulas / synthetic checks | Every active tick: global daylight plus saturated glow (maximum 152), minimum own-side vision darkness, nonnegative difference; previous exploration capped at 32. Allies do not share vision. Raw brightness replaces invented 0/96/152 states. Renderer integration remains separate |
| Building visibility and display memory | V inspected rules / synthetic checks; G fallback bounds and original-save history | Structures test four clipped bounding-box vertex corners strictly above 32. Per-side frozen display snapshots serve the scene and minimap; hidden state changes and destruction remain unseen until observation. Current targeting/picking never uses display memory. See [fog implementation](fog_implementation.md) |
| Raw-fog minimap background | V inspected buffer/index rules / synthetic SDL checks; G UI scaling and fallback colors | A 180 x 180 buffer indexes the user's palette resource 15 with local brightness shifted right three, independent of terrain/preview. Remembered occupied building cells remain marked; exact marker palette colors and integer-scale margins remain open |
| Building construction: town queue, progress = adjusted build time, stages = animation slot 0 | V formula/stages / P live timing and worker semantics | Accepted placements immediately create unpaid, idle placeholders. Default production remains serial; the optional parallel construction modification gives each building an independent timer. Cancellation removes waiting placeholders without refund and refunds active unbuilt work. Progress adds HP without erasing damage. Original worker participation, start/completion tick alignment and damaged-construction transitions remain unmeasured; see task 47 |
| Building footprint in tiles | V descriptor mask / P fallback | Building d6..d54 and Defence d9..d57 form a 7x7 mask centred at (3,3); only value 2 occupies a tile. Navigation, reservations and preview share it. Missing masks retain the old d4/d5 rectangle fallback; clearance cells with value 1 remain unverified |
| Town production: construction and units share one active item per town by default | V inspected control flow / engine regression, P exact timing and spawn | FIFO waiting orders are unpaid; payment and speed/time selection happen at start. Capacity is 50 waiting items plus active work. Active pause is town-local; cancellation refunds unbuilt paid work. Missing/incomplete halls suspend timers. See task 36 and the task 48 update below for evidence, placeholder semantics and the opt-in parallel construction modification |
| Building income and repair supply | V formulas / synthetic checks | Descriptor occupancy cells equal to 2 weight resource channels A/B with d60/d61; periodic/scheduled 7200-tick clock from StartTime. Gold and d62/d320 repair supplies have separate accumulators. Completed same-town warehouse applies once to farms only. Missing resource map contributes zero terrain bytes |
| Starting money = side `StartMoney` | V | `.mp` |
| Garrison | engine / V semantic fact | Extracts live mobile town members into a fresh tactic and selects it. Explicit mission town membership and hall identity, invalid-owner checks, empty/repeated extraction, hash and save/replay checks. Reference packet field layout and live comparison remain unverified |
| Repair/destroy mode and list commands | engine / partial | Modes 0/1/2; mode changes clear lists, same-mode commands preserve them. Eligible completed town structures toggle membership; halls, mobile units, foreign-town targets and construction are rejected. Off mode collects damaged ordinary buildings. Dead, repaired or reassigned targets are pruned. Commands do not delete or change HP. Harvested credits now drive priority-group repair/destruction and are cleared after service; exact tick alignment remains unverified |
| Attack modes | V numeric modes / partial engine | Direct 0, defensive 1, aggressive 2, balanced 3 (default). Command, ownership/tactic propagation, direct range-only engagement, movement/follow gates and replay/hash state implemented. Task 153 integrates tile leashes and persistent anchors; attached-tactic inheritance and full assignment remain pending; see investigation below |
| Selection and follow commands | partial engine / P formation | Deterministic control-group recall and side-local tactic selection, persistent ground/air follow, ownership/cycle checks and save continuation; task 153 follows persistent tactic anchors; packet encoding, member readiness and collision arbitration still lack reference parity. See V10 update |
| Universal population cap | O reference / engine correction | Task 13 removes the unsupported AI-only cap 80 in develop. Synthetic human/AI production above 110 units passes. Queue 50 and assignment arrays 100 do not establish a side cap; no live population measurement succeeded |
| Difficulty bonuses | V map-key exclusion / O other effects | Map Difficulty stays editor metadata; synthetic 0/100 inputs produce identical simulation hashes. No HP, damage, income or price bonus is invented. Global difficulty effects remain unverified |

## Siege firing investigation (2026-10-29)

The synthetic `test_siege_ranged` reproduced the reported failure before the
fix: types 3 (axe siege), 4 (cannon) and 83 (catapult) could neither acquire nor
fire at an unarmed enemy structure 180 pixels away. All nine acquisition,
firing and damage/effect assertions failed. Truck kind 4 had no weapon mapping
or armed flag; Ball/Axe damage and paired range/reload getters were unavailable,
so combat eligibility rejected those descriptors. No melee damage was invented
to make these tests pass.

The engine now loads Truck d7 and enables known Ball/Axe/paired weapons. The
same eligibility calculation drives autonomous acquisition and explicit orders.
Out-of-range siege units choose a reachable, disc-clear firing tile within the
weapon envelope, with stable row/column ties; direct mode never pursues. This
selection is an engine pathfinding policy, not an original WaveSearch match.
These classes inherit the maximum-only CanFire predicate: no positive minimum
range or melee substitution is added. Target/source radii and structure geometry
remain the existing approximations.

Local reference inspection established Truck d7 (`0x4166a0`), Headtruck d9
(`0x42c3c0`), Ball/Axe reload d2 and range d3 (`0x4039a0`, `0x4039b0`), paired
first-child getters (`0x41e480`, `0x41e5f0`) and perpendicular d2 launch offsets
(`0x41e4a0`). `0x4038a0` supplies their maximum-only CanFire predicate. Truck and
Headtruck decrement a positive cooldown before testing it (`0x460a10`,
`0x42c880`); the engine uses descriptor reload between siege launch events.
Reference animation-marker firing and turret turn delays are still absent.

Ball damage delegates to its child; Axe potential damage includes its direct
component and chained weapon. Class 9 applies sequential category factors.
The class-7 blast branch (`0x44c130`) checks the airborne category, returns full
damage in the inner band, then applies distance falloff and outer-band structure
and victim multipliers. Balls evaluate every living victim at the actual impact
location, including allies, rather than guaranteeing a hit on their original
moving target. New flight/payload fields participate in state hashes and replay.
The engine's Euclidean norm, provisional victim radii, integer blast rounding,
launch-time aim instead of reference prediction, and Arrow/Axe collision behavior
still prevent a full projectile parity claim. Unmapped blast multipliers retain
256; Marine, Truck and Headtruck use their inspected descriptor fields.

Validation covers idle/explicit acquisition, descriptor ranges/reloads, three
launches over two reload intervals, close targets, direct mode at long range,
approach to a firing position across an impassable wall, paired projectiles,
ball altitude and moving-target escape, blast boundaries/category/friendly fire,
and per-tick replay equality. The private reference attempt used
`work/siege-game/` and `tools/zstat_run.sh` inside `unshare -rn` with dummy SDL;
`work/siege-probe/out.txt` records `Cannot open netlink socket: Operation not
permitted`. It failed before game launch. No live reference timing or mission-1
parity was established, and isolation was not bypassed.

Final validation: Release build and all 26 runnable CTest entries passed; four
game-data entries and the restricted UDP process check skipped. The separate
game-data engine check passed with private mission 1/4 copies and resource
symlinks under dummy SDL in `unshare -rn`. All pre-commit gates passed.

## Projectiles and effects

| Rule | Status | Source / notes |
|---|---|---|
| Projectile objects at 12 Hz, integer fixed-point flight, previous/current render interpolation, hash/replay coverage | engine | Arrow, Axe, BallExpl and FireThrow objects carry source/target ids, snapshot endpoint, speed, payload and animation age. No renderer writes simulation state |
| Type-to-weapon dispatch | V inspected fields / partial families | Marine/Truck d7, Defence d59, Spider/Headtruck d9. Truck is marked armed when its descriptor supplies a weapon. Class 12 retains both child IDs, damage and opposite perpendicular launch offsets from d2; class 8 delegates. Bird d9, Cannon d7 and Zeppelin d8 are loaded; Bird getter is 0x407780. Unarmed Eagle/Zeppelin do not fall back to generic damage |
| Arrow speed/reload/range | V table / P firing schedule | Arrow d1/d2/d3; descriptor reload+1 schedules shots, without duel overrides. Shots start at the combat event, not the animation fire marker. Flight timing parity remains open |
| Ball/Axe speed/reload/range | V getters / P exact timing | d1/d2/d3 supply speed/reload/range. Paired launchers delegate range and reload to their first child. Base CanFire imposes a maximum range and no positive minimum. Zero-speed balls stay at their launch ground position and fall; zero-speed Axe metadata remains outside the exercised siege types |
| Flame and splash behavior | V inspected blast branches / P geometry and arithmetic parity | Ball child class 7 uses d0 animation, d1/d2 inner/outer radius, d4 damage, d5 air category. Distance includes a provisional victim radius; the outer band falls off, applies the structure factor 204/256 and blast multiplier. Inner-band damage returns directly. Friendly and neutral victims are included. Class 11 has ten active flame updates, reload 40 and range 96; victim-attached burn is not inferred. Class 8 dispatches sixteen scattered payloads; native RNG/pool phase remain open |
| Projectile trajectory and collision | partial / P exact trajectory | Balls retain snapshot aim, planar velocity, altitude, vertical velocity and gravity; impact follows the fall at the actual ground location. Moving targets can escape; terrain does not intercept balls. Arrow/Axe retain provisional straight flight and guaranteed original-target damage; interception/swept Axe damage remain unresolved. Source death does not cancel flight |
| Reference trajectory, interception, burn and splash measurements | P protocol / S tooling / M absent; needs live run | The 2026-10-28 isolated probe failed before game launch: namespace loopback setup returned `Cannot open netlink socket: Operation not permitted`. This is not a no-hit observation. Static flight/flame/volley rules are now inspected; terrain/obstacle behavior, first update phase and live per-category splash/burn cadence remain unmeasured; see the [measurement protocol](EXPERIMENTS.md#projectile-measurement-protocol). No combat rules were changed from this failed probe. Follow-up supplies 126 ztest_battle placement candidates, engine-local per-flight/impact/victim tracing and fail-closed synthetic controls; audited reference projectile/burn and single-shot setup adapters remain missing. See the [live run sheet](EXPERIMENTS.md#v07-projectile-run-sheet-needs-live-run) |
| Effect animation ids, slots, durations and hotspots | V resource metadata | `effects.hpp`, `AnimSet`, `sprites_mapping.md`; effects read frame/light timelines from installed Units.xbe. 150/151/152 last 11 ticks in slot 0 and 10 in slots 1/2; debris/rubble 250; shockwave 105 lasts 30. Arrow/Axe/Ball use dir32 looping slots, flame uses a one-shot slot |
| Damage fire | V selection / engine descriptor HP inputs | built structures at <=75/50/25% HP use 154/155/156 at ground position +(0,1), looping; repair/death removes the fire. Descriptor maxima are supplied by V04; synchronized reference rendering remains open |
| Explosions, debris, muzzle and beam | V selection metadata / P spawn timing | vehicle death 152, ground debris 113; Arrow d13=105 creates shockwave; immediate descriptor d10/d11 enables muzzle 106 at +20 px, beam 107 at approximately 30-px steps, impact 110. No extra turret muzzle sprite is invented |
| Smoke and unknown owners | open | smoke contained in known animation frames renders through the same effect path. Standalone anims 104/153 remain unassigned: their owners/spawn conditions are unverified, so no smoke spawning rule is invented |
| Height and displacement | partial | Balls launch at height 24, aim two pixels below the target ground point and fall with separate fixed-point vertical state. Integer Euclidean distance/normalization and launch tick ordering still require differential measurement. Axe uses frame height, debris/rubble height -1; vehicle explosions retain explicit render height. Altitude-dependent light remains unresolved |
| Draw order | V contract / engine integration | effects share the unit/terrain overlay list: underlays first, ascending ground Y then descending X; each shadow precedes its body, then turret shadow/body. No separate air pass. Alpha uses the existing SDL approximation, not reference-exact background compositing |
| Effect glow | V masks / P final shading | light.xbe 0x455 ids 1..16 are additive brightness, saturated at 152; translated bilinearly onto the (width+1)x(height+2) 48-pixel grid at ground position minus nine tiles. Body frames supply unit light; turret frame light is not separately queried. Palette shade uses (brightness+4)/8. Per-tile sampling, daylight tint and existing fog remain approximations; vision masks are not glow masks |

Validation after merge: synthetic `engine_unit` cases cover projectile metadata
for all four families, descriptor-gated firing for unresolved weapons, known
arrow damage and reload through flight/impact, command-log replay in flight,
hash sensitivity to endpoint/age, paused effects, single splash damage, ally
exclusion, stale source ids, transient expiry, repaired fire cleanup, known-radius
rubble retention and missing-radius fallback without duplicate rubble. Both
descriptor combat and projectile suites run in the pure-test entry point.
`tests/playtest/effects.mp` is an independently authored placement fixture using
the user's mission_1 terrain. Run its companion script with both SDL dummy
drivers and the command in `tests/playtest/effects.txt`. Screenshots stay in
ignored `build/effects-*.png`; no original assets are included. This is an engine
smoke test, not a synchronized reference screenshot comparison or a parity claim.
Historical branch validation: Release build, all eight CTest entries (including installed-data checks), the
headless fixture and pre-commit gates passed on 2026-10-26. Cached hook
environments were copied into the worktree to run pre-commit without network
access. The fixture asserts vehicle losses and writes flight/impact/combat shots.



## Population, difficulty and tactic modes (2026-10-28, partial task 16)

**Reference investigation:** the supplied private data, local decompilation and
original instructions were available read-only. The 18 supplied campaign/tutorial
`.mp` files contain 58 Difficulty entries: 55 zero, one 100 and two 75. Those
values are editor metadata, not evidence of three difficulty levels or stat
multipliers. The inspected campaign `.scn` action names supply no difficulty,
population-limit or attack-mode setter. Existing descriptor tables supply per-type
HP/weapons and production data; no universal population field is established.
The inspected production path still gives no side cap. This is a bounded finding,
not proof that the whole game has no limit or difficulty effects.

The original UI has **four** modes, rather than an established three-mode
stand-ground/defend/hunt enumeration. See the factual mode table and method
addresses in [rules_ai.md](rules_ai.md#tactic-attack-modes). The mode setter clears
combat assignment and propagates to qualifying attached tactics. The reference
also has mode-dependent group distances 4/8/8/6 in tile coordinates; these are
not weapon ranges or replacement sight radii.

**Independent implementation:** the older command inventory above predates this
partial ATTACKMODE handler. `AttackMode` is hashed object state, defaulting to
balanced. Engine command fields use `a` for the mode and `ids` for owned mobile
members; their living mobile same-side tactic members receive the mode too.
Negative tactic IDs do not connect unrelated units. Invalid modes, foreign,
dead/removed, unknown and structure targets are ignored. These are engine command
conventions, not original packet decoding. Mode changes clear the attack target
and resume interrupted movement, or stop an idle pursuit. Direct control acquires
only in-range targets and never issues pursuit movement. Defensive mode can engage listed enemies while idle and preserves movement orders; aggressive/balanced modes can interrupt
movement and resume it after target death. Explicit attack orders remain available.
Task 153 supersedes the old per-object acquisition cadence and averaged idle
origins: persistent tactics now own the 15–22-think scan, expanded member box,
enemy listing, tile leash, shared walking path and lost-team follow/merge.
**V, construction:** anchors survive member removal; member movement requests
one tile at a time with independent descriptor speeds. **G:** nearest-target
assignment, A* routing, movement-state readiness, disconnected-member paths,
explicit focus pursuit and reference timing/flag details remain approximations.
See [unit_behaviour_implementation.md](unit_behaviour_implementation.md) for the
V/G boundary, preserved mission 1 regression and unresolved auto-engage CSV rows
above seven tiles. No original executable was run and live parity is unverified.

Task 13 has removed the unsupported AI-only 80-unit cutoff in develop. This patch
preserves that planner rather than modifying its superseded adapter. Human and computer production
still require existing build-tree prerequisites, money and queue admission.
This does not assert unlimited reference population. Difficulty remains absent
from simulation inputs; the core parser retains the editor key for tooling.
No bonus is inferred from the computer discount or runtime build speed.

**Synthetic verification:** production starts and completes with 110 existing
mobile units through explicit purchases on both human and computer sides. Parsed Difficulty 0 and 100 inputs
have identical state hashes at each of 110 ticks. All four mode values, invalid
values/IDs, side-local tactic propagation, independent unassigned members,
structure exclusion, direct ranged fire without pursuit, movement gates,
resumption after target death, hash sensitivity and serialized command-log replay
are covered by `test_engine --pure`. Hash changes invalidate older engine saves
with recorded hashes. These checks establish engine behavior, not oracle parity.

**Measurement blocker:** a private map with 110 friendly footmen and a distant
hostile control was generated using `make_zstat.py`, then attempted through
`zstat_run.sh` with both dummy SDL drivers inside `unshare -rn`. Namespace setup
failed at `ip link set lo up`: `Cannot open netlink socket: Operation not permitted`.
The executable did not launch. No no-input/record/replay population result,
difficulty bonus or stance timing was measured. Private maps, disassembly and
failure logs remain under ignored `work/population-difficulty/` (the disassembly
is `work/population-text.asm`). No unisolated fallback was used.

**Dependencies:** task 01's audited packet/object mapping and aligned normalized
observer remain prerequisites for differential oracle comparison; the full tactic
assignment/group model is still absent from this checkout. Work independent of
those prerequisites is implemented above. A permitted reference host must test
production across candidate boundaries, difficulty settings, stationary/moving
stance scenarios and record/replay before promoting the unresolved statuses.

**Worktree validation:** bootstrap with `--no-hooks`, Release configure/build with
`AZTEC_GEN_DIR=`, and all pre-commit hooks passed. Final CTest with the external
installation and both SDL dummy drivers passed 15 of 16 entries, including all
three installed-data checks. `lockstep_processes` failed with `UDP socket:
Operation not permitted`, matching the baseline failure before edits. The new
synthetic engine checks pass. No git metadata was written; changes remain
uncommitted.

**Landing regression correction:** a synthetic footman duel reproduced no damage
and no casualties because the firing check omitted the attacker's collision
radius. A 10-pixel weapon plus a 14-pixel target radius allowed only 24 pixels,
while two 14-pixel discs stop 28 pixels apart. Mobile weapon checks now include
both source and target extents, shared by direct-mode acquisition and firing.
Collision radii and centre-distance geometry remain engine approximations, not
new verified reference facts. The regression checks damage, casualties and replay.
The develop integration must retain task 13's town planner and its removal of the
80-unit fallback cap; the older adapter in this checkout is left untouched.

**Revalidation after landing feedback:** the corrected changes were merged, without
conflicts, into an ignored `build/landing-repro/` snapshot of current develop.
Its Release build passed. The unchanged `ai_unit` and 20-minute `ai_skirmish`
checks passed, including the casualty assertion, viewing-side hash comparison and
command-log replay. Of 18 CTest entries, 14 passed and three installed-data checks
skipped in this clean-data snapshot; only `lockstep_processes` failed because UDP
sockets are denied by the sandbox. The original worktree also passed its Release
build and 15 of 16 checks with installed data, with the same network restriction.
All pre-commit hooks pass. The focused melee regression failed its damage and
casualty assertions before the reach correction and passes afterward.

**Landing retry correction:** the harvester previously printed success even when
`git checkout -b` failed because an earlier attempt left the task branch behind.
That led the supervisor to test and commit a clean develop checkout. Harvest now
resets the local task branch from current develop with `checkout -B`, checks every
git step, and reports preparation only after patch application succeeds. Mocked
git-command tests cover retries and staging, diff, reset and checkout/apply failure
propagation without writing git metadata. This does not publish or merge changes.
The final conflict-free snapshot of develop (`bc4d12c`) with this patch builds and
passes CTest: 15 tests pass, four skip (three installed-data checks and the
restricted socket test), zero fail. This includes the original AI skirmish and
newer town/projectile/audio suites. Five handoff-tool tests, including mocked git
failure cases, and all pre-commit hooks pass. The older original worktree retains
its previously documented UDP baseline failure. No landing command, commit or
push was run by the assistant.


## Mission scripts (`.scn`, `sim/script_runner.*`)

`side(local)` means the mission human side (`World::script_side`), independent
of viewing side. The current audit and evidence boundaries are in
[script_implementation.md](script_implementation.md); it supersedes older
immediate-execution and eager-timer policies in historical campaign notes.
V below identifies a statically inspected rule or an engine construction check,
not a live parity result; G explicitly identifies approximations.

| Rule | Status | Notes |
| --- | --- | --- |
| Enabled trigger executes previous latch, then short-circuit AND; empty conditions false | V static rule / synthetic | Initial latch false, all enabled, one-tick latency, every-tick repetition; disable freezes latch; self-disable finishes evaluation |
| Trigger IDs | V static rule / synthetic | First matching ID addressed; duplicates retain separate latches, enablement and traces; G file order substitutes for pool order |
| Timer lifecycle | V static rule / synthetic | Lazy zero/running creation; set preserves pause, stop/start retain value, kill erases and a later condition recreates; signed 32-bit counter. G existing timers advance before trigger pass |
| Operators and clocks | V operators/day-clock rule; G clock integration | Signed operator codes, StartTime-based localTime wrapping at 7200; globalTime uses active elapsed ticks, native clock ratios unmeasured |
| Side counters and rectangles | V hostile-side count and normalized axes; G counter/spatial adapters | Enemies counts active sides, not objects. Rectangles admit living runtime categories 0/1/6/7 (units, structures and vessels, excluding cameras); construction-class numbers are a different namespace. Inclusive tile edges remain G. Empty native groups and town-list lifetimes remain incomplete |
| Tactic lookup, lock/unlock and kill | V static lookup / synthetic; G hierarchy | Reserved plain groups before town fallback; unlocked plain group cannot be readdressed by ID. Kill uses member death without an attacker; recursive child groups, map Locked fields and AI reuse remain incomplete |
| Group/town gift and join | V distinct gift group / synthetic; G hierarchy/production details | Gift allocates receiver group without merging colliding IDs; town gift uses shared ownership transfer. Join requires both lookups. Native nested membership and town queue cancellation/refund parity remain open |
| Relations, money and outcomes | V static rules / synthetic | Directional relation changes; signed money adjustment floored at zero; first side outcome wins while playing, only human side ends mission |
| printString and presentation | V ring/deadline rules / synthetic; G visual integration | 16 ring slots retain 15 prints; 4000 ms presentation deadlines, staggered retirement, alternate print discarded. Camera vision/deactivation implemented; exact layout and camera phase remain G |
| Paths, attack/move and vessels | G existing adapters / synthetic | Path variants, whole-group orders and vessel activation exist; native hierarchy/controller parity unverified |
| Side build/move commands; condition codes 0c/0d | Missing side-plan dependency | Build/move remain explicitly unsupported; native plan conditions have no identified grammar producer or independent side-plan equivalent |

All 15 supplied campaign scripts parse with zero unsupported action sites in the
current inventory; two missing-target warnings remain in mission 6. Synthetic
regressions cover latches, duplicate IDs, timers, signed comparisons, ownership,
lookup, messages and replay, with integration expectations updated for latency.
The reference executable was not run. Campaign objective reachability and full
V12 parity remain unverified; see the implementation document for remaining G
rules and validation results.

## L8 validation attempt (2026-11-01, partial)

The isolated reference launch preflight failed before game startup at loopback
configuration (`Cannot open netlink socket: Operation not permitted`). It used
the external read-only game/runtime, private writable Maps/Saves/settings,
read-only asset symlinks, dummy SDL and `unshare -rn`. Private files are under
`work/parity/l8-launch/`. No original AI behaviour was observed and no numeric
reference expectations are committed. Composition, expansion, launch thresholds,
defence latency and retreat remain unresolved. The existing planner policies are
unchanged; no reference match is claimed.

Independent work completed despite the missing reference observations:

- `parity_ai` checks a numeric event-window comparator with invented inputs:
  inclusive timing/composition bounds from at least two references, total force
  size, missing/extra events, required behaviour classes, unstable reference
  coverage, malformed captures, private output paths and missing-input skip 77.
  It consumes event captures; audited observers and controlled passive-player /
  defence / retreat scenarios are still required. See `tests/parity/README.md`.
- The existing 14,400-tick skirmish retains every activity, viewing-side hash and
  replay assertion. Each five-minute interval additionally requires **each side**
  to make purchases, produce armed mobile recruits and fire. Wave composition
  checks count actual released members, require autonomous armed troops and
  verify the synthetic scenario's enabled footman-only composition for both sides.
  These stronger checks pass without changes to AI policy or scenario inputs.

Baseline Release build and CTest passed (34 passed, six skipped). Pre-commit
passed using a writable copy of the existing cached hook environments; downloading
new hook environments failed because DNS was unavailable. Final Release build,
CTest (35 passed, six skipped) and pre-commit, including both new Python files,
passed. Synthetic validation
does not close the missing reference observer, movement/combat/economy or tactic
dependencies described below.

## Computer player (`ai/planner.*`, `sim/ai.cpp` adapter)
Controlled sides have `OnlyComp=1`, including neutral computer sides. The pure planner reads
world state and validated purchase/placement offers, returning its next state and
commands. It does not read the viewing side, wall clock or global RNG. Internal
commands are regenerated during replay rather than added to the player command log.

| Rule | Status | Notes |
|---|---|---|
| Town states 0..23, science timer, production suspension, ordinary-building development sum clamped to 9, normal/fast spending comparison | V control flow / synthetic checks | Active construction or a town's queued production suspends transitions; science and speed continue. Queued map construction counts once toward development, even when it already has an object. State 0 has no loaded side-plan arbitration yet. Requested-type science selection remains a direct-prerequisite G adapter; autonomous hall-upgrade selection remains open; hall-upgrade commands are supported |
| Income and spending estimates | P estimate / V comparison | Actual cumulative harvest and purchase spending are tracked per town; average harvest substitutes for the unresolved instantaneous rate estimate. Starting cash is divided among the side's towns. Speed is normal below twice spending, fast otherwise |
| Economic requests | V preference / P placement | More than two farms and no warehouse prefers an available warehouse, otherwise farm, then mine. Uses existing resource-map harvests; no passive AI grant or worker-gather command is invented |
| Plan composition | V arithmetic / G adapters | Per-enemy M*W scoring with 0..63 noise, asymmetric need minus all prior picks, side-maximum-development batches, 256 entries, 32 reusable units per type and type 33 exclusion. Reuse exhaustion reselects a candidate. The observable last-wanted-candidate bug is retained. Shared RNG ordering, tactic eligibility and recursive science scoring remain G; see [ai_implementation.md](ai_implementation.md) |
| Target score | V formula / G record representation | Minimum normalized octagonal distance plus half mean relative unit HP and half capped unit count/256, recomputed each tick. Computer targets that are not all-enemies multiply only distance by three. Town centres stand in for record-object positions; the documented native count-index oddity is not emulated |
| AI money gate | V / synthetic checks | Units and economic requests queue without a reserve test; production start alone tests and deducts cash. An unaffordable head blocks its town and suspends strategy. Science alone pre-checks money at forced normal speed. Existing price/time factors are retained and covered across outpost, developed, computer and all-enemies branches |
| Intelligence | V discovery/cadence / P visibility | Records visible live towns, own ordinary buildings and visible foreign membership including nearby towns within eight tiles on both axes. Town discovery refreshes on first use and every `(side+1)*1000` updates. Previously discovered living enemy towns remain valid destinations through fog. Computer layers initialize at 24; uses engine visibility values <=24; exact MapExplorer weighting and visibility parity remain open |
| Attack waves | P (G fallback) | States 1/2 prepare purchases and release idle autonomous combat units toward a known hostile non-outpost town. Local and threatened force targets use documented `3*(development+1)` and `7*(development+1)` counts. Purchase composition follows the documented plan arithmetic; tactic release and launch timing remain G adapters |
| Threat entry and defence | V trigger / G recall and combat dispatch | One active non-allied object of a non-computer side within a squared four-tile outpost or eight-tile developed-town radius enters phase 16 from phases below 16, drops the local plan and cancels paid/unpaid town work. No hit or visibility test. Phase 16 continues for any foreign-side object within eight tiles, allies included. Recall covers autonomous side members; native tactic eligibility is unresolved. Existing last-hit combat dispatch and idle reinforcements remain G policies; computer attackers cannot enter the proximity threat branch |
| Placement | P (G fallback) | Stable radius/row/column search within eight tiles, weighted resource bytes for terrain harvesters. Failed placement produces no purchase, payment or reservation. Map construction uses placement offers; flag-0 farms/mines/defences use queued automatic placement, while hall upgrades replace the existing hall |
| Population and tactic limits | open | Removed the invented 80-unit cap. No difficulty bonuses or universal population limit. Task 153 adds the tactic scan; reference kill-time assignment and full side-response arbitration remain open |
| `Tendency`, `BuildSpeed`, `Difficulty` from `.mp` | no effect in rewrite | Runtime speed is selected by the strategist; mission loader keys do not override it |

`ai_unit` checks synthetic economic preferences, development, speed changes,
construction/queue suspension, fog, wave release above 80 units, defence alerts,
script ownership, sparse town IDs with queued structures and harvest accounting,
and `.scn` attackTown/moveTo/path actions across viewing sides.
`ai_unit` additionally checks idle reinforcements during queued production,
non-repeated active orders, script/other-town ownership, rejected threats,
science/elapsed advancement and speed updates while busy. Two towns sharing cash
issue independent farm orders priced after their own normal/fast speed selection;
the exact income-versus-twice-spending boundary is checked.

`ai_skirmish` runs twenty simulated minutes (14,400 ticks) twice, comparing every
tick hash across viewing sides, then compares command-log replay at every tick.
The synthetic `ai_economy.scn` drains both sides' cash at tick 1000 and grants
2000 at tick 1200; assertions reconcile those actions with harvested income and
purchase spending. Deliberately durable synthetic halls (1,000,000 initial HP)
keep both producers alive. Each five-minute interval must contain an autonomous
wave release, firing, casualties and new objects. A wave counts only when the
planner completes state 2 and an idle troop receives the destination order.
Both sides must launch, harvest and spend. These are asset-independent headless
engine scenarios, not reference parity or campaign balance measurements.

**V11 synthetic evidence (2026-10-28):** waves per five-minute interval were
2/2/1/1; firing events 354/287/205/190; casualties 10/11/15/22; new objects
30/21/23/30. All 14,400 tick hashes matched across viewing sides and replay,
ending at `a361c839f7171bfa`. Defence reinforcement dispatch is still a P policy
(G in the reference rules), not a newly measured reference fact. It introduces no
new persistent state or RNG draws. Changed autonomous orders can invalidate old
saves' stored replay hashes; the save format is unchanged.

The task 03 resource-map/pricing work and independent town queues are present;
the start-payment gate and existing partial-work refunds have synthetic coverage. V02's audited object
identities and aligned normalized AI observers are absent, although packet
validation and comparison tooling exist. Town-command and hall-upgrade support is
present; autonomous hall-upgrade selection, exact economic queue semantics, tactic
assignment and reference timing parity remain open. Unsupported transport sites
and unresolved build/move/camera semantics remain listed in the mission-script section.

V05-V10 remain incomplete dependencies for campaign acceptance. The work above
exercises the existing deterministic planner independently of their missing
reference movement, sight, combat, economy, town-service and tactic contracts;
composition, development scoring, placement, threat entry, offensive thresholds
and side-plan arbitration remain P/O. V11 therefore remains open.

The 2026-10-28 private mission_1 probe used the supplied read-only installation
and runtime, a private game copy, dummy SDL drivers and `unshare -rn` through
`tools/zstat_run.sh`. Namespace setup failed before launch with
`Cannot open netlink socket: Operation not permitted`; no reference run or parity
result is claimed. Private copies and failure logs remain under ignored
`work/ai-game/` and `work/ai-probe/`. No new reference M evidence was obtained.

Validation: baseline and final Release builds and pre-commit gates passed.
Full CTest: 26 passed, four private-data checks and the restricted UDP process
check skipped, zero failures. Separately, `test_formats` and `test_engine` passed
with the private installation copy and dummy SDL inside `unshare -rn`; these
check asset loading and engine integration, not reference behavior.

Private campaign smoke attempts used `aztec_engine --headless --max-ticks 14400`
with its automatic human-side exercising policy, twice per mission, under the
same namespace/dummy-SDL/private-copy restrictions. Mission 1 ended in defeat at
tick 1196 (state `f0fe635c3a48d9c`, tick-trace `531ea20dbc6f6c1e`); mission 4
ended in defeat at tick 1474 (state `459f91226436afb1`, trace
`163aed4c58ac510e`). Each mission's two complete JSON reports matched; neither
executed an unsupported action. These are bounded engine smoke results, not
winning routes, full twenty-minute campaign runs or reference comparisons.
Private reports/logs are in `work/ai-probe/engine-m{1,4}-{1,2}.*`.

## Audio (`src/engine/audio/`)

Current rule-by-rule audit: [audio implementation](audio_implementation.md). V rules
now include request flag rejection/protection, per-slot generation handles, corrected
voice families, attack/follow/stop-following offsets, local fog threshold 33,
start-only loop fog gating, and retained/deferred music selection. Synthetic checks
cover these contracts and original attenuation conversion. G event fallbacks require
`--audio-event-fallbacks`; descriptor/town/warning publishers, representative ranks,
fades and menu lifecycle parity remain incomplete. No original executable was run.

| Rule | Status | Notes |
|---|---|---|
| Effects from `Snd.xbe`: sound set (`sound_set`) from field [2] of an animation frame, played on the first tick of the frame, random member of the set | V format / engine construction; G native event timing | Supported scene choices and randomized pitch consume the shared simulation MSVC stream before view/device filtering. UI/standalone audio uses an isolated seed-one stream. Descriptor-only sounds, native helper interleaving and fixed/random caller flags remain open; see [implementation](rng_and_tick_order_implementation.md) |
| Pan = (x - view centre x) clamped to +-4000 hundredths of dB; attenuation: none up to 500 px, linear to -4000 at 800 px, silent beyond | V (`docs/formats.md`), P (100 units = 1 dB) | Scene dispatcher uses the active view visibility predicate; exact reference fog semantics remain open |
| Music: `Music=N` -> track N, otherwise `MusicMask`; next track when one ends | V (format and ascending cyclic order), V construction / synthetic checks | `music_allowed_mask` in `core/audio`; no simulation RNG or mission-name salt |
| Animation state loops, building/projectile/effect sounds | V construction / synthetic checks; simulation timing remains P where unresolved | Header loops and frame entries use a read-only scene snapshot; visibility/distance/load/exit cleanup checked in `audio_events` |
| Independent effects/music/speech gain, acknowledgement suppression and speech WAV/caption API | V construction / synthetic checks, P replacement/duration policy | Separate speech gain, UTF-8 captions, missing-WAV/muted/no-device fallback; six presentation seconds, outside World/hash/replay. Selection and supported order producers use the documented families; town/warning producers remain missing; see [audio implementation](audio_implementation.md) |
| L6 audio request traces and ordered comparison | V construction / O reference identity and parity | JSONL clone events with dispatch triggers; reference buffer requests require audited private sound/trigger mapping. Global order and one-tick tolerance; absent classes stay unobserved. [L6 evidence and blockers](parity.md#l6-audio-event-comparison-2026-10-29) |

## Saves
| Rule | Status | Notes |
|---|---|---|
| Engine saves: versioned AZS3, bounded transactional parsing, explicit little-endian encoding, content/script identity, atomic slot replacement and refusal of replay hash mismatch before replacing the active world | V construction / synthetic tests | `engine_save` and `engine_save_processes`: malformed inputs, repaired-checksum fuzzing, write-failure preservation, fresh-process 14,400-tick save plus 300 matching continuation ticks, camera metadata and script victory. Ctrl+S / Ctrl+L remain mission-local. [Format, limits and remaining V16 acceptance](engine_saves.md); AZS2 is refused. V03 full-state audit and native Windows/campaign continuation remain unverified |
| Original `*.sav` format and engine import | V reader/import construction / O continuation parity | Core decoding and CLI snapshot import exist; `savegame`, `save_import` and optional `save_import_private` cover supported fields. Saved production, orders, script execution and other simulation state remain incomplete; see [import limits](savegame.md#independent-engine-import) |

## Resource and production validation (2026-10-26)

**V specification / synthetic implementation checks:** `test_engine --pure` uses
invented maps and objects plus public numeric descriptor tables. It checks an
asymmetric mine mask (16 gold/100 active ticks), a channel-A-only Rus farm
(363 gold at clock 1000), the once-only warehouse payout (484), mixed A/B
rounding (367), repair credits without cash, StartTime 1200 partial cycles,
periodic wraparound, Chinese-style date-zero cursor reset, local versus side-wide
prerequisites, duplicate structures, insufficient funds, speed-dependent costs,
single payment at production start, cancellation and serialized command replay over a day.
These are engine/formula checks, not measured reference equivalence.

**Engine conventions / O:** each unpaused tick advances the clock before building
updates; newly active accumulators use the preceding clock as their creation
phase. An exhausted schedule resets on a separate tick with clock <= first date.
`Build`/`BuildMode` may select a town through `ids[0]` (an owned live town member),
with the first owned town as the default. This is an engine command convention,
not a claim about original packet fields. Human and computer towns initialize at
normal speed. Autonomous town transitions suspend during active construction;
non-map structures use automatic placement. Concurrent town queues and repair service
are integrated in the V08/V09 update below; exact original placement/spawn ticks
and active-building suspension still need evidence.

**M differential evidence:** none added. This worktree has no local game assets,
reference executable/lift or task 01 normalized economy/training comparison tool.
Mine/farm and training reference comparisons could not be run. Historical lifted
161/56 observations remain separate from original conversion facts and synthetic
checks. Resource channels retain A/B names; their natural-resource labels remain
unresolved in `formats.md`.

## Sprite and lighting integration

The current [renderer integration inventory](render_integration.md) separates
R/C/T/P reference evidence from independent implementation and synthetic checks.
Kind-specific body/secondary layers, HP construction boundaries, damage fires,
height projection, common Y/X/underlay sorting and user-resource light/vision
mask grids are integrated. This does not change simulation AI sight or establish
pixel parity. Aircraft altitude, structure destruction-radius and overlay
producers, transport phases, wind, per-pixel gradients and exact alpha/shadow
blending remain open. Owners 104/153 remain unknown. New attack/death, overlap and
night probes are **pending** because private prerequisites are missing; no new P
label was assigned. Existing simulation placeholder labels above still apply.

## Descriptor combat validation (2026-10-26)

**Engine construction checks:** synthetic structure/nonorganic and airborne/organic
combinations, Marine d1/d4 differences (Bear/Bizon), strict threshold boundaries,
sequential rounding, armor rounding and zero-air suppression, special constant
weapons, unavailable/delegated weapons, reload+1 hits against a stationary
structure, and replay hashes at every tick of a 24-tick duel. Hash sensitivity
covers secondary HP and regeneration state (superseded by the V04 countdown below). Old engine saves
with previously computed hashes are not hash-compatible with this correction.

**Evidence boundary:** V arithmetic refers to documented reference facts, not a
new runtime observation. The d11 byte-1 polarity (nonzero selects e8, zero f4)
is an engine interpretation of the documented complementary flags, pending a
reference check. Regeneration selector transitions and threshold overshoot need
a more precise specification. These are not promoted to verified parity.
Task 01 is still queued; no normalized combat comparison runner is present.
No private game installation, generated lift or reference executable is available
in this worktree, so no isolated reference duel or record/replay was run. Historical
duel intervals remain M observations, never universal reload rules.

## AI validation update (2026-10-27)

The Release build and all ten asset-independent CTest entries pass, including
the twenty-minute AI skirmish with per-tick equality and replay. Both game-data
entries (`formats`, `engine`) also pass against a private installation copy with
SDL dummy drivers inside `unshare -rn`; these tests do not need network-interface
setup. Pre-commit runs use the installed hook cache copied into the ignored build
directory because remote hook fetching is unavailable. The separate reference
runtime probe failed at network-interface setup, as recorded above.

## Whole-mission parity audit (2026-10-27)

The [mission parity runner](parity.md) found two prerequisites still missing:
engine CLI replay of oracle packets with audited object-ID mapping, and a
normalized reference observer aligned before command processing. Existing pool
hashes cannot identify semantic fields, and historical unit-log side/max-HP slots
are insufficient. Task `01-oracle-parity` remains a dependency; the mission runner
and strict normalized comparator are available independently of that work.

**Experiment result:** all nine supplied mission recordings were attempted in
private game copies with the required namespace isolation. Loopback setup failed
with a denied netlink socket in every attempt, before the reference executable
launched. No cross-engine divergence tick, object or rule was measured. Failure
reports remain private under `work/parity/mission-audit`; no rule status above is
promoted by this audit.

Prioritized gaps for the next permitted mission comparison:

1. **Measurement blockers (observed):** complete task 01 packet decoding,
   canonical object identity, tick alignment and normalized field coverage;
   rerun on a host permitting the documented network namespace setup. Establish
   reference repeatability before attributing mismatches to engine rules.
2. **Command semantics (existing P / incomplete):** verify side/tick ordering,
   group selection, town selection and currently logged-only commands. Reject
   unsupported corpus packets explicitly so later rule findings remain valid.
3. **Initialization and movement (existing P):** compare object presence,
   ownership and primary/secondary HP first, then speed, collision radii,
   movement-class assignment and path decisions. Identity errors must be fixed
   before treating position mismatches as movement-rule evidence.
4. **Combat (existing P / unresolved):** validate acquisition/visibility,
   range geometry, regeneration state selection, projectile timing and unresolved
   weapon scheduling using the earliest normalized differences.
5. **Economy and production (existing O / incomplete):** measure clock phase,
   active-building behavior, simultaneous town queues, training/spawn ticks,
   non-map structures, upgrades and refund policy against the reference.
6. **Mission scripts and AI (existing P / incomplete):** verify trigger/timer
   timing, unsupported transport/build commands, strategist speed selection,
   purchase planning and autonomous attack waves over the recorded horizon.

Items 2 through 6 are a dependency-oriented investigation order based on the
existing inventory, **not a ranking inferred from measured mission divergences**.
The next successful report should replace this provisional order with counts and
reproducible first-field findings; agreement over an input interval still does
not establish whole-game parity or mission completion.

## Town command implementation (2026-10-27, partial task 05)

**Reference facts (existing V):** the garrison, repairer and hall-upgrade rules in
[rules_types.md](rules_types.md) remain the specification. No new M observation
or reference-parity claim is made here.

**Independent implementation:** mission `[Town#]` membership survives loading,
including mobile members and hall identity. Side defences without explicit town
membership remain unattached; no nearest-town association is guessed. Garrison
extracts eligible members, clears their town attachment and assigns a fresh
nonnegative tactic number above existing object tactics. Empty extraction leaves
the previous selected tactic intact. The selected tactic is deterministic side
state; only the viewing side's UI selection changes. The garrison panel button
and configured key submit this command for a selected hall.

Engine command fields use `b` as the mission town section number for Garrison,
RepairMode and DestroyHere; RepairMode uses `a` for 0/1/2, and DestroyHere uses
`ids` for targets. These conventions are independent of the reference wire
payload. Duplicate target IDs in one packet count once. DestroyHere toggles the
active repairer list; off-mode manual toggles are ignored. The UI only allows
eligible list targets when a mode is active and routes them to their own town.
The hall Repair button now cycles automatic service, marked repair and marked
destruction through logged commands. Its configured Repair key does the same.

**Engine construction checks:** synthetic invalid town/side/target, multiple-town
extraction, repeated empty extraction, duplicate/repeated toggles, mode-change
clearing, same-mode preservation, full-HP repair rejection versus destroy
acceptance, hall/mobile/construction exclusions, off-mode collection, stale-target
cleanup, hash sensitivity and serialized save/command-log replay pass. Tests do
not establish reference tactic numbering, off-mode service timing or packet
compatibility. Hash changes mean older saves with recorded hashes are incompatible.

**Dependency boundary after merge:** resource-based income, per-tick repair-credit
harvesting, town-local prerequisites and producing-town attachment for new spawns
are implemented by the economy work described above. Independent town progression,
damage-priority allocation and destruction service are implemented in the V08/V09
update below. Hall-upgrade completion/build-tree changes and upgrade save/replay
scenarios are covered by the subsequent flag-0 production entry. The mission
parity runner and strict normalized comparator are present, while task 01's packet
replay, audited object-ID mapping and aligned reference observer remain prerequisites
for a normalized reference comparison.

**Observation blocked:** the supplied private game and runtime are available,
but isolated setup fails at `ip link set lo up` with `Cannot open netlink socket:
Operation not permitted`. No reference command was observed and no fallback
outside `unshare -rn` was used. Private setup/run logs stay under ignored
`work/town-commands/`. Original instruction evidence and historical measurements
retain their existing labels.

**Historical town-command validation:** Release configure/build and the default
CTest run passed (eight checks passed, two game-data checks skipped). The final
full CTest run passed all ten checks with `GAME_DIR` set to the
supplied external installation and both SDL dummy drivers; these are independent
engine/format tests, not reference-runtime observations. All pre-commit hooks
pass using cached hook environments copied into `build/pre-commit/` because
network access is unavailable. Synthetic replay hashes agree after every command
and across different viewing sides. Loader checks retain town membership and
exclude standalone side defences from inferred town attachment.

## Campaign harness boundaries

The automated runner and renderer-free engine mode are described in
[playthrough.md](playthrough.md). Reports and installation-specific observations
stay under ignored `work/`. Two equal independent-engine runs establish internal
reproducibility only. Script path state hashing now includes cycling and inversion
flags and separate side/tactic keys; script initialization resets the unsupported
action counter. Synthetic checks cover these corrections. Older recorded hashes
may change.

Larger engine gaps exposed by campaign exercising remain open:

- Side build/move script commands and camera deactivation reach unsupported
  action branches. Vessel actions now have default-on engine adapters.
  The harness audits dormant branches as well as counting executed ones. Missions
  depending on them need script/transport implementation before completion can
  be assessed; camera-only diagnostics need not prevent gameplay progress.
- The existing computer AI purchases units and reacts defensively. The harness's
  human-side purchase/nearest-hostile policy cannot solve scripted transport,
  escort, exploration or construction objectives. Completing the documented AI
  and script task remains a prerequisite for credible autonomous campaign coverage.
- Orders can remain active without positional progress. A stalled-order report
  needs a movement/pathfinding investigation with the object, target, terrain and
  dynamic blockers preserved; it does not establish an unreachable objective.
  Expensive missions can hit the wall budget without producing a terminal hash.
- Immediate script defeat and pending victory conditions need mission-specific
  initialization/trigger investigation and aligned reference observations. Do
  not bypass triggers, infer a win from enemy elimination, or change script
  semantics merely to make the exercising policy pass.

The checker proves only the documented static impossible-rectangle cases and
identifies scripts without local victory actions. General objective reachability,
mission strategy and reference campaign parity remain unresolved. No oracle
record/replay dependency is required for the standalone harness; it remains
required for any claim that its outcomes match the original game.

## Town upgrades and flag-0 production (2026-10-28)

**Independent implementation / existing V tables:** the menu retains flag-0
structure entries from `buildtree.csv`. The existing price/time formula applies
at production start, with one payment and a frozen countdown (task 36). Queue reservations block
competing hall upgrades in the same town. Only map-construction entries reject
duplicate structures; queued farms, mines and defences allow multiple copies. DisabledID,
affordability and local/side-wide prerequisites still apply. Completion replaces
the owned live hall in place, preserving its ID, town and selection references,
updates its descriptor HP and navigation footprint, resets its harvest clocks,
and installs the race/level build tree. No generic troop-stat bonus is applied.
Other flag-0 structures complete in their producing town and use the existing
resource-map income, warehouse, repair-supply and prerequisite rules. Town-member
command selection resolves mission section numbers independently of vector slots;
repair supply uses the corresponding town slot. The deterministic town planner
selects economic, development and combat offers; autonomous hall-upgrade selection
remains open. See the computer-player inventory above.

**P engine policies / O reference parity:** automatic placement searches free
footprints in increasing square rings around the live hall, with row/column tie
breaking. This is a deterministic fallback, not a verified terrain ranking or
reference placement algorithm. Production suspends its timer if its hall is
missing/incomplete and waits at zero if no footprint fits; cancellation remains
available. Hall
upgrades require a free replacement footprint and assign the new descriptor's
full HP on completion. Original HP transitions, active-building suspension and
exact start/completion ticks remain open. Queues now progress independently per
town. Task 36 supersedes the earlier side-wide pause, upfront payment and full
refund policies, and clarifies capacity as 50 waiting items plus active work. Map construction
continues to use the build-tree construction flag; flag-0 commands ignore map
coordinates. These engine command conventions do not establish packet parity.
Town levels are now hashed, so older saves with stored hashes are incompatible.

**Synthetic validation:** all supported race/level upgrades, sparse town section
numbers, preserved hall identity and footprint blocking, build-tree unlocks,
competing/repeated requests, paused timers, flag-0 farms/mines/defences, adjusted
costs, repeated queued/completed farms/mines/defences across all three races,
distinct placement sites, queue capacity/payment/cancellation, map-construction
duplicate rejection, resource accumulation after completion,
DisabledID, insufficient money, invalid town members, dead halls, cancellation,
serialized command replay and upgrade save/replay are covered in `engine_unit`.
These checks validate engine construction, not measured original behavior.

**Repeatable structure correction (2026-10-29):** a synthetic regression first
failed on the previous blanket structure duplicate check: the second purchase
was rejected and completed mines remained disabled. The check now follows the
build-tree map-construction flag. Private reference rule inspection at `0x411ac0`
shows that only flag-B entries test their own completed/in-progress counts;
ordinary prerequisites use town counts and A prerequisites use side counts.
Those counts are prerequisites, not a one-mine-per-side limit. No build-tree
CSV, placement algorithm or runtime code changed. The regression completes three
copies per tested farm/mine/defence type, verifies payment and serial countdowns,
and reproduces the final state through serialized command replay.
The sustained-opposition synthetic fixture now uses richer farm terrain (resource
byte 130 rather than 65) to support its combat-per-interval assertions while the
planner can purchase additional farms. Planner policy is unchanged; this fixture
does not measure campaign balance.

The new isolated reference attempt used a private installation copy, dummy SDL
and `unshare -rn`; it failed at loopback setup with
`Cannot open netlink socket: Operation not permitted` before game launch.
Thus no new live original-game placement/timing or full parity claim is made.
Private experiment output remains in `work/repeatable-reference/`.

**Dependency and observation boundary:** the economy/prerequisite implementation
is present. Task 01's audited packet replay, object identity mapping and aligned
normalized observer are still missing; that dependent comparison remains undone.
An isolated reference attempt used a private game directory with read-only asset
symlinks and dummy SDL drivers under `unshare -rn`. Loopback setup failed with
`Cannot open netlink socket: Operation not permitted`, before runtime launch.
No reference observation or new M evidence is claimed. The private attempt log
is under ignored `work/town-production/reference.log`.

**Checks:** Release build and `engine_unit` pass. Full CTest with the supplied
read-only game installation and SDL dummy drivers passes 15 of 16 checks,
including all three private-data checks. `lockstep_processes` fails at UDP socket
creation with `Operation not permitted`, matching the pre-change baseline.
All pre-commit gates pass using cached hook environments inside this worktree.

## V20 malformed-input and sanitizer subset (2026-10-28)

V (construction property): `AZTEC_SANITIZERS=ON` instruments independent targets
with fail-fast ASan/UBSan. The dedicated Linux CI job builds without private
inputs and runs the synthetic `unit` suite. Hosted CI execution is pending.
`fuzz` now exercises every core file loader's file and memory paths, plus engine
save/log, mission, animation, light-mask, daylight-header, optional CSV and
playtest loading. See [tests/ROBUSTNESS.md](../tests/ROBUSTNESS.md) for the
coverage inventory, commands, limits and evidence boundaries.

The expanded corpus runs 117,068 deterministic mutations plus explicit
regressions. It exposed signed overflow in descriptor speed conversion under
UBSan; loading now rejects unrepresentable speeds. Footprint rounding uses
wider arithmetic, optional text metadata is bounded, sparse path indices cannot
overflow vector sizing, and daylight loading validates header/section ranges
and clears prior keys. Engine save/log parsing rejects malformed framing and
preserves the save output or clears the log output on failure.

Local GCC Release and Clang ASan/UBSan synthetic checks passed. Local leak
checking is unavailable because LeakSanitizer thread inspection is denied;
CI keeps leak checking enabled. Game-data and socket-process checks skip in
this environment. This establishes loader regression evidence only: V17-V19
campaign, packaging and platform acceptance remain dependencies for the rest
of V20. No reference/game-data run, full campaign soak, safe active-world save
replacement (V16), or whole-game parity is claimed.
## V18 release packaging (2026-10-28)

**Independent implementation / construction evidence:** the release workflow
requires Linux and Windows builds, synthetic CTest and pre-commit before a single
publish job can attach packages. Separate fresh jobs extract and relocate the
actual archives before smoke testing. `tools/package_release.py` stages only the
engine, tracked public rules metadata and package documentation/license. Linux
bundling collects transitive shared dependencies, Debian copyright notices,
common license texts, exact source versions and their source archives/patches;
the host supplies glibc and drivers. Windows now bundles SDL2 and MSVC runtime
DLLs and carries SDL2 source and dependency notices (see the readiness update below). Publication also includes the exact public
engine source archive and checksums. No game files, oracle or lifted code are
inputs. See [PACKAGES.md](PACKAGES.md) for external installation setup and the
optional separately installed ffmpeg dependency.

Rules resolve beside the executable before the source-tree `docs/data` default;
`--rules-dir` overrides them. Headless mode also honors `AZTEC_GAME`, matching the
interactive engine. These are application setup changes, not simulation rule or
reference parity evidence; no V/M/P simulation label changes.

**Local synthetic evidence:** Release configure/build and full CTest pass:
17 passed, three absent private-data checks skipped and the denied-socket process
check skipped. Pre-commit passes using cached hooks inside the worktree. The
relocated local package passes `tests/test_package.py` with both SDL dummy drivers
inside `unshare -rn`, using a private synthetic installation and unrelated
working directory: victory at tick 10, repeated identical reports, bounded SDL
rendering and useful failures for missing/corrupt archives, corrupt terrain and
missing rules. No private installation or reference process was used.

**Pending dependency / release evidence:** local staging on Fedora uses host
libraries and is explicitly marked non-distributable. Debian dependency/source
collection, clean-runner Linux package execution, Windows DLL loading and
native Windows smoke execution require the release CI jobs; none was run here.
V17's code/build matrix exists, but green remote gates and branch protection
remain unverified. GitHub PR refresh failed at the API connection, so no unmerged
work is assumed available. V18 stays incomplete until both platform package jobs
pass. This work does not claim campaign playability, device testing or parity.
## V13 simulation animation producers (2026-10-28, partial)

**Engine construction checks:** body and turret angles are separate 0..255
simulation fields, initialized from facing and included in state hashes. Older command-log saves with recorded hashes are
not hash-compatible with this change. Actual
fixed-point displacement supplies body direction; attack targets supply turret
aim, including the killing shot before target removal. Turret aim preserves a
stationary chassis direction. Aircraft return to idle when displacement stops.
This is an engine policy, not measured original turn rates or turret tracking.
The existing aligned-direction selector now receives full angles; Bird steering
cooldown and Cannon hop restart rules remain unresolved under V05/V07.

Rendering, simulation light selection and frame audio use one read-only object
snapshot, including both angles, transport phase, construction, height and
independent clocks. Animation ages now advance in simulations rather than in
`World::begin_tick`, so pause freezes them. Defence construction produces its
slot-6 overlay while progress is incomplete and clears it at completion. Existing
HP seeks, fires, death explosions, debris and known-radius rubble remain in use.
Altitude and destruction radius inputs remain unresolved; no dynamics or radius
formula is invented here.

**C/R integration:** Boat opening finishes in Open, closing finishes in Closed,
and turning toggles orientation once and finishes in Closed. Completion uses
finite frame durations from the installed animation bank through the existing
`SightData` resource bundle. Missing, holding or malformed sequences never imply
completion. The transition resets the phase age to zero. Vessel action adapters
now initiate ferry entry and submarine animation dispatch; see
[transport_rules.md](transport_rules.md) for fallback phases and timing limits.
Johnka retains its existing orientation loop.

Synthetic tests cover both Boat orientations, exact duration boundaries, stable
phases, missing resources, pause/resume, off-axis aim and flight direction,
killing-shot aim, angle hash sensitivity, command-log replay and construction
completion. Dummy-SDL exact pixels verify that the renderer consumes the body
angle without mutating state. These are independent implementation checks, not
reference observations; exact and tolerant reference metrics remain separate in
`zspr_compare.py` and no new score is reported.

**Blocked reference evidence:** all three attack/death, overlap and night probes
used the supplied external runtime and installation through the private-copy
runner, both dummy SDL drivers and `unshare -rn`. Each returned status 1 at
interface setup: `Cannot open netlink socket: Operation not permitted`.
The runner inventory is ignored at `work/v13-contract-probes/inventory.json`.
No game launch, record/replay control, reference pixels or new M/P evidence was
obtained. V13 remains open for V05-V07/V12 behavior, aircraft altitude, destruction
radius, synchronized scenes and visibility/minimap acceptance.

Validation: Release build and all 17 runnable CTest entries passed; three private
checks and restricted UDP skipped in the asset-free configuration. Private
`test_formats` and `test_engine` passed with a complete private Maps copy inside
`unshare -rn` and dummy SDL. An initial copy containing only mission 1 failed
the checks requiring mission 4 and at least 30 maps; completing the private
fixture resolved those failures. The independent engine rendered mission 1
with fog and minimap after 24 requested ticks (`work/v13-engine.png`, ignored).
This is a smoke check, not campaign visibility acceptance. Pre-commit passed
using locally cached hooks relocated into ignored build output.

## V02/V03 replay and determinism progress (2026-10-28)

**Engine construction / synthetic evidence:** the oracle text parser fails closed
on malformed or unordered packets and missing input/output files. Injected
non-NOTHING packets are logged for exact readback checks. The mission runner
retains all raw pool differences and, when adapters exist, compares normalized
observations from both reference repeats before comparing engines. The optional
engine replay observer exposes the before-command boundary without inventing an
oracle object mapping. `oracle_packets`, `parity_controls` and
`determinism_state` cover these contracts; see [parity method](parity.md).

The state audit fixed hash omissions, made script timers explicitly 64-bit,
reset command history on reinitialization and removed host `long double` from
production/warehouse arithmetic using the same exact binary32 coefficients.
Hash contract 2 invalidates older saved hashes; the UI refuses a replay mismatch
before replacing the active world. This is not full V16 save hardening.
The 14,400-tick synthetic process scenario includes training/RNG spawns,
movement/combat, command-log replay and fresh-process continuation from tick
7,201, with viewing/presentation variations. The workflow now compares complete
synthetic transcripts across GCC, MSVC, MinGW and AppleClang; remote results and
native Windows validation are still required. See [determinism audit](determinism.md).

**Reference evidence unavailable:** nine isolated corpus attempts failed during
loopback setup before the reference executable launched. Private outputs are in
`work/parity/v02-isolated/`. No transient-pool cause, original-versus-lifted
arithmetic diagnosis, audited reference object/ownership/max-HP observer, packet
translator or first semantic mismatch was obtained. Tasks 01/11 remain partial;
the changes above do not depend on those missing adapters. V02 and V03 remain
open, including reference ordering/RNG and full campaign/platform evidence.

Local validation: GCC 16.2.1 Release build and all 21 runnable CTest entries
passed; three private-data entries and the UDP process check skipped. Clang
22.1.8 passed the five focused path/oracle/determinism/tooling checks, with an
identical 14,400-tick transcript and movement golden digest. All pre-commit hooks
passed using a copied local cache, including explicit checks of new files.
The reference wrapper passed a C11 syntax check; no lifted runtime was linked or
launched here. Open-PR refresh could not reach the GitHub API, so no unmerged
dependency is assumed present. These results do not establish native Windows,
remote CI, live rendering/audio or campaign determinism.
## In-game UI layout (2026-10-28, partial task 14)

The historical layout below describes modern mode. The newer verified geometry
and corrections are integrated behind `--layout original`; see
[interface_implementation.md](interface_implementation.md) and the update below.

**Existing reference evidence (V):** [formats.md](formats.md#cursors-and-interface-selection-of-sprites-from-commonxbe)
records the 800x600 mission 4 layout: Menu at x=598, a 120x90 minimap at
(638,70), portraits near x=722, and a 254x208 command card at (546,392).
Local resource inspection confirms common:282 is a 60x22 two-frame Menu button,
292 is the 204x493 side background, and 297 is a 254x208 command background.
These are resource dimensions and historical coordinates, not a new live capture.

**Independent implementation:** shared geometry anchors the minimap/selection
strip to the right and the command card to the bottom. Menu uses its original
resource and its actual 60x22 hit region. Maps smaller than the minimap opening
retain one pixel per tile at UI scale 1; larger maps fit the opening. The first
selected object's portrait occupies the right column and uses its owner's team
palette. Command/production controls follow the five-column card cells, with
paging and all eight queue hit regions retained. This particular allocation of
commands, production and queue controls is engine presentation (P), pending live
comparison. The scale is constrained by both window dimensions so a 900-pixel
window height alone cannot double the panel beyond the available width.

Resource text, selection labels, tooltips and messages now use physical integer
UI scaling for both installed and fallback fonts. Long text wraps at UTF-8
character boundaries; tooltips wrap long tokens and cap their height. Messages
wrap within the visible world area, use font-derived line spacing and keep the
latest lines when full. Hiding the panel restores the full width for messages
and mission banners. DirectInput translation includes digit keys and Escape;
existing configured bindings still suppress conflicting M/X/H engine shortcuts.
Unsupported gameplay modes remain unavailable.

**Synthetic verification:** `ui_screenshots` renders invented RGB565 panel,
button and glyph resources through SDL's software renderer with dummy drivers.
It saves eight synthetic PNGs under the build directory, covering 800x493,
800x600, 1024x768 and 1600x1200 with installed-font-shaped fixtures and the
fallback font. It compares independently specified panel silhouettes, Menu and
command-icon regions, and message glyph rasters with per-channel tolerance 3
and at most 0.2% differing pixels in each region. A deliberately displaced card
edge must fail comparison. Geometry assertions cover minimap/portrait anchors;
long-tooltip bounds and the existing command/replay tests cover additional
regressions. These checks verify independent rendering contracts, not original
pixel parity. No game screenshots or pixels are test fixtures.

**Live limitation:** the isolated reference attempt failed at `ip link set lo up`
with `Cannot open netlink socket: Operation not permitted`, before launch. The
script and failure log remain in ignored `work/ui-probe/`; isolation was not
bypassed. Independent-engine static and selected-unit captures succeeded using
dummy SDL inside `unshare -rn` with a private installation copy. Their visual
review establishes readability only. Full command icon semantics, font choice,
selection/group presentation, resource decoration, tooltip timing/appearance,
message-log history and synchronized reference comparison remain open (P).
Repair and hall-upgrade UI acceptance also depend on the incomplete town-command
work described above; this task does not invent those gameplay semantics.

## V08/V09 economy and town service update (2026-10-28, partial)

**Independent implementation:** production advances the first pending item of each
producing town, retaining insertion order for commands, completions and cancellation
indices. Completing an item does not advance that town's next item on the same
tick. Missing/incomplete halls suspend queued timers; they never spawn units at
an unrelated side start. UI production menus and queue cancellation follow the
selected object's town, and placement remembers the producing town. Cancellation
feedback now shows the refundable unbuilt payment. Task 36 supersedes the earlier
side-wide pause and full refund policies. The panel scrolls through all waiting
orders, with 50 waiting slots per town in addition to active work.

Repair service consumes each town's harvested credits after orders/mark maintenance
and discards its unused budget every tick. Modes 0/1 repair; mode 2 destroys.
Membership, HP, credits, navigation changes and death effects already participate
in hashes; no unrecorded service clock or persistent budget was introduced.

**V inspected facts / synthetic implementation checks:** local instruction review
of repair selection/allocation (`0x444730`, `0x444920`) establishes three ordinary
building groups using integer maxHP/currentHP: greater than 3, equal to 3, and
at most 2. Repair processes Defence before those groups; destruction processes
Defence last. Each group takes a fixed integer share of its incoming budget.
Capping work on one target does not redistribute its unused share inside the
group; residue passes to the next group. Zero work removes membership, including
when the group is too large for a positive share. Destruction triples the budget
before sharing, limits work by current HP, applies three damage per work unit,
and divides remaining work by three before the next group. Weapon 10 provides
the first group member as attribution; this path passes explicit damage rather
than calculating another descriptor hit. Building byte +0xec rejects this damage
while still consuming work (`0x410560`); service loads that ordinary-building
protection flag. Death uses the engine's existing navigation/effect cleanup.
These are independently described facts, not copied executable-derived source.

`engine_unit` covers exact damage-group boundaries, Defence priority, capped
shares, zero-share removal, residue flowing between groups, no carry between
ticks, automatic service in another town, no cash spending, protected buildings,
destruction death/footprint cleanup and save/replay starting before completion.
It also checks simultaneous town training, serialization within a town, pause
clock suspension, completion boundaries, producing-town UI cancellation and
construction's final tick versus first harvest tick. All-race hall upgrades also
check a saved paused countdown resumed through completion. Existing build-tree and non-map structure checks remain in place.
These checks establish engine behavior; construction's same-tick countdown,
activation phase, hall HP reset and spawn positions remain engine conventions.

**O dependencies and failed observation:** V02's audited packet replay, object
identity adapter and aligned normalized economy observer are not available in
this checkout; V04 initialization/HP parity is also incomplete. Work independent
of those prerequisites is implemented above. The supplied private installation
and runtime were accessed read-only. A private game copy with resource symlinks,
ordinary Maps/Saves directories and copied configuration was prepared under
ignored `work/economy-game/`. The bounded dummy-SDL reference attempt under
`unshare -rn` failed at loopback setup with `Cannot open netlink socket: Operation
not permitted`, before launching the game. Private logs remain under
`work/economy-probe/`; there is no new M evidence. Exact clock/command phase,
activation/suspension, placement, original training/spawn/completion ticks,
refunds and hall-upgrade HP transitions remain open. V08 and V09 therefore remain
incomplete in the roadmap. Changed simulation behavior also means older saves'
stored hashes may no longer match; the save format was not changed.

**Validation in this worktree:** baseline and final Release builds pass with
`AZTEC_GEN_DIR=` and a worktree-local ccache. Full CTest passes 17 entries; the
three game-data entries and restricted UDP process test skip. All pre-commit
hooks pass using a copy of the local hook cache under ignored build output.
Private-data/reference runs are not verified by these skipped checks.

## V14/V15 campaign presentation and audio (2026-10-28, partial)

**Independent implementation:** pause offers briefing/objective review and live
sound controls. The front-end and in-mission reader share all `.mit` section
headings/text, with mission notes as fallback; no script condition is invented
as a completed objective. Text wraps long UTF-8 tokens, and scrolling uses the
actual visible line count, with wheel/arrows, PageUp/PageDown and Home/End.
Long campaign labels shrink/ellipsize within buttons. Modal input cannot issue
world commands or advance simulation time, including the closing click.
Overlays draw after panels/HUD. Save/load failures now provide on-screen
feedback; notices remain presentation state.

The outcome screen has clickable continuation, restart/retry and menu controls.
Enter retries defeat; victory continues to an existing next mission's briefing.
Numeric mission arguments, case variants and `.mp` paths use bounded campaign
parsing rather than unchecked pointer arithmetic; custom and final missions do
not invent a next map. Winning mission 15 displays Campaign Complete. Outcome
freezes presentation-driven simulation and blocks command input. This is an
engine flow, not the original ending video or full campaign acceptance.

Campaign wins from ordinary menu sessions persist in a separate default engine
profile, `Saves/engine_campaign.txt`. The 2026-11-22 correction below replaces
the old fifteen-bit record with the documented scalar progress/unlock rule.
The file is bounded/validated before replacement; writes stage in an exclusively
created directory and rename over the active record. Invalid existing profiles
are preserved and a failed write is reported. A stale `.pending` directory
refuses a write; it can be removed after confirming no engine session is writing.
Original `.plr`/saves remain untouched. This does not implement multiple profile
selection, original profile interoperability or V16 fresh-session frontend loads.
Scripted playtests and imported worlds do not record campaign wins.

**Audio construction evidence:** explicit speech WAV requests have independent
live gain and CP1250-to-UTF-8 captions, even without a WAV/device or with muted
speech. Acknowledgement suppression, caption options/expiry, replacing the
previous utterance and mission/load cleanup have synthetic checks. Six-second
caption duration and replacement are P engine presentation policies. No
selection/order/type/race/announcement ID mapping has been invented: the speech
API has no campaign event producer yet. Existing animation body/turret/death,
building/effect cues, loops, visibility attenuation and mission playlists remain
active. See [audio event mapping and remaining gaps](audio.md).

**Synthetic evidence:** `campaign_ui` checks all fifteen completion bits,
round-trip/replacement/failure/corruption handling, strict mission parsing,
missing-next/custom/final progression, briefing headings, audio option bounds and
invalid gains. It renders invented briefing, outcome and sound overlays at
800x493, 800x600, 1024x768 and 1600x1200, writes PNGs only under the build tree,
checks hit regions/clip restoration and distinguishes the first/last text pages
and victory/defeat images. Its real mission-loop playtest confirms pause,
briefing and sound dialogs freeze ticks and consume commands, then reaches a
synthetic victory with no further ticks from world input. Existing
`ui_screenshots` panel/message comparisons still pass. `audio_events` adds
1,200-tick complete hash comparisons with and without dispatch under changing
gains, captions, visibility, listener position and mixer cadence, followed by
matching command-log replay. No simulation fields, commands, RNG, save encoding
or hash contract were added or changed by this work.

**Dependencies/evidence limits:** V09/V10 command semantics, V12's 19 unsupported
transport action sites, V13 rendering and V03 campaign/platform determinism
remain incomplete in this checkout. Those parts are not claimed as delivered by
UI/audio construction tests. Required selection/order speech mappings, building
announcements and original voice policies remain unverified. Full campaign
controls/groups/hotkeys/repair/upgrade acceptance, live objectives, defeat/retry
and progression through all missions still require successful player routes and
reference/interactive evidence. V14 and V15 remain open.

Release build and all runnable CTest checks pass, with private-data and restricted
UDP checks skipped in the default configuration. Explicit private format and
engine checks and the 15-mission script inventory passed with the supplied
installation through a private copy, dummy SDL and `unshare -rn`; original-save
import skipped because that copy has no original saves. A mission 1 engine
capture/music-event smoke check succeeded. The reference attempt failed before
launch at `ip link set lo up` with `Cannot open netlink socket: Operation not
permitted`; isolation was not bypassed. Private scripts, logs and captures are
ignored under `work/v14-v15-probe/`. There is no new reference recording, replay,
pixel metric, audible comparison or campaign completion claim. Open-PR status
could not be refreshed from GitHub; no unmerged dependency is assumed present.
## V04 initialization and regeneration (2026-10-28, partial)

**V inspected field facts:** local constructor/getter and instruction review
establishes this descriptor mapping. Entries are dword indices; a dash means no
such descriptor field is loaded. Only factual metadata and independently written
engine logic are included in the public tree.

| Kind | Primary/max HP | Secondary HP | Regeneration | Natural threshold | Physical armor | Weapon ID |
| --- | --- | --- | --- | --- | --- | --- |
| Marine | d1 | d4 | d2 | d3 | d13 | d7 |
| Building | d1 | - | - | - | d57 | - |
| Zeppelin | d1 | d4 | d2 | d3 | d13 | d8 |
| Truck | d1 | d4 | d2 | d3 | d12 | d7 |
| Bird | d1 | d4 | d2 | d3 | d15 | d9 |
| Headtruck / Spider | d3 | d6 | d4 | d5 | d15 | d9 |
| Defence | d3 | - | - | - | d65 | d59 |
| Cannon | d2 | d5 | d3 | d4 | d12 | d7 |

Maximum getters include `0x45fe50`, `0x4711a0`, `0x44a0f0` and `0x415dc0`;
physical armor is virtual +0x10c (for example `0x45ff20`, `0x44a180`,
`0x40fab0`, `0x41cc20`). Boat/Johnka/Camera inherit a zero maximum getter;
no mission-instance maximum or invented one-HP maximum is assigned to them.
The renderer now uses each object's simulation maximum, including after upgrades.
Original-save import preserves secondary HP independently; initialization installs
the descriptor maximum without replacing saved primary HP or service countdown.
This is a field correction, not complete original-save resumption.

Recovery Think paths (`0x433db0`, `0x460a10`, `0x4719b0`, `0x407b10`,
`0x42c880`, `0x44b360`, `0x417090`)
select the natural threshold when object +0x54 is zero and full primary HP when
positive. The comparison precedes addition, without clamping to the threshold.
The base update `0x43edc0` decrements the counter after Think; the setter
`0x402990` replaces it. Town facility service refreshes it to 10 in the inspected
`0x45d4d0` path. Headtruck/Defence instead override that service method with
immediate capped healing (`0x41cbe0`); a countdown is not invented for that call.
The engine implements countdown expiry and descriptor recovery. Automatic facility
coverage/refresh was added in the [capture and facility service update](#capture-and-facility-service-2026-11-21),
with separate small/large-pool predicates and explicit spatial/timing approximations.
Live reference validation remains open.

**Engine construction checks:** synthetic placement covers all 90 public types,
side ownership, sparse town IDs 4/9, hall linkage, colliding scenario IDs, separate
defence/vessel provenance, positive damaged/over-max HP and independent maxima.
Seven recovery families cover natural/full boundaries, overshoot, service expiry,
pause, dead-object exclusion, unchanged secondary HP and serialized command-log
save/replay hashes through expiry. Original-save tests distinguish saved secondary
HP from descriptor maximum and retain a positive countdown. The private engine
test additionally loads and checks initialization for all 15 campaign missions.
These establish engine behavior and data coverage, not reference equivalence.

The Hypno predicate/weapon mapping and a tested kill-time calculation use secondary
HP for Hypno and primary HP otherwise, with ceiling hit count times reload
(`0x46fac0`); it is a calculation helper, not integrated tactic assignment.
Hypno damage/conversion, secondary-HP transitions, ownership transfer and assignment
remain unverified and unimplemented. Existing generic hit handling must not be
interpreted as verified Hypno behavior. Unresolved weapon families remain unresolved.

**Missing dependencies and observations:** V02's audited reference identity/field
adapters and aligned observer are incomplete in this checkout. A bounded private
reference probe used the supplied read-only runtime, resource symlinks, copied maps
and configuration, private Maps/Saves directories, dummy SDL and `unshare -rn`.
It failed at loopback setup with `Cannot open netlink socket: Operation not
permitted`, before launching the game. Inputs and failure output remain under
ignored `work/v04/`; there is no new M evidence, record/replay result or normalized
reference comparison. V04 remains open for these dependencies, live facility service
validation, Hypno behavior and campaign-wide live transitions.

**Validation:** baseline Release build and 26 CTest entries passed; five entries
skipped (four private-data checks and restricted UDP processes). Baseline
pre-commit passed using a worktree copy of the local cache. The private engine
check passed with dummy SDL inside `unshare -rn`, using a private installation
copy. Final Release build, the same 26 CTest entries, the private engine check
and all pre-commit hooks passed; the same five CTest entries skipped.
Changed behavior can invalidate older saved replay hashes; the save container
format is unchanged and existing mismatch rejection still applies.

## V10 command integration (2026-10-28, partial)

**Reference facts and boundary:** read-only inspection of `0x448520`, `0x448f60`
and `0x448210` confirms distinct point-selection, tactic-identity selection and
follow/attachment dispatch. `0x447ba0` resolves tactic identities rather than
control-group digits. The unmodified follow branch invokes tactic virtual +0xf0;
the private vtable resolves that to `0x451440`, which records a leader link.
Modifier bits select different attachment/position branches. This establishes
neither the engine's control-group payload layout nor its centroid tracking
policy. No executable-derived source or data was added to the repository.

**Independent implementation / synthetic checks:** the existing engine numbered
control-group slots remain engine conventions. Create/add filter ownership,
liveness and duplicate IDs. SelectGroup recalls the live owned members; an empty
slot clears command selection. SelectByNo selects the live owned members of a
side-local tactic identity; absent identities and unsupported modifiers preserve
selection. Number-key recall submits a command. Command selection is hashed
separately from viewing-side UI selection. Empty unit-order ID lists resolve
that command selection, so selection and orders in the same tick replay together.
Explicit IDs continue to determine ordinary UI orders.

FollowTeam expands selected mobile members to their owned source tactics and
records a separate leader tactic. It rejects missing leaders, self-follow,
cycles and unsupported modifiers. The engine follows the live mobile leader
centroid, refreshing destinations every six ticks; this is P formation policy.
Ground and air followers retain the order at arrival. Defensive/direct modes
preserve the follow path; aggressive/balanced modes can interrupt it for combat
and resume it afterward. Mode changes also resume interrupted follow orders.
Stop, explicit move/attack and script orders cancel following; loss of all leader
members clears it. Follow state and command selection participate in hashes and
AZS3 replay/save continuation. Replay compatibility is now version 2: earlier
saves are refused before replay because the state and command semantics changed.

The unsupported AI-only cap 80 was already absent in this checkout. The remaining
invented eight-entry town production cutoff is replaced by the inspected queue
capacity 50. Synthetic checks cover admission through 50, refusal of entry 51
without payment, and production with over 110 existing units for both human and
computer sides. These limits apply to production queues, never total population.
Task 36 adds scrolling beyond the first eight displayed entries and distinguishes
50 waiting slots from the active item. No universal
population cap or global difficulty bonus is inferred. Existing parsed Difficulty
0/100 hash-equivalence checks still apply only to the engine and the inspected
map-loader/price-time exclusions; other difficulty effects remain O.

**Dependencies and failed observation:** V02's audited object/command adapters and
aligned reference observer, V04 initialization/HP parity and V08 measured production
semantics remain incomplete. Work above does not depend on those missing parts.
A fresh private game copy was prepared under ignored `work/v10-game/`, with
read-only asset symlinks and ordinary Maps/Saves. A 110-footman population control
was attempted using `zstat_run.sh` and the supplied read-only runtime, with dummy
SDL inside `unshare -rn`. It failed before game launch at loopback setup:
`Cannot open netlink socket: Operation not permitted`. Private inputs/failure
output remain in `work/v10-probe/`; there is no new live measurement.

V10 remains open for campaign acceptance, reference follow/selection modifiers,
attached-tactic propagation, formation anchors and mode distances 4/8/8/6,
assignment/target eligibility, join/spawn mode inheritance, population observations
and difficulty effects outside the inspected paths. Synthetic replay establishes
internal determinism only, not reference or campaign parity.

**Validation:** bootstrap `--no-hooks`, worktree-local Release build with
`AZTEC_GEN_DIR=`, and all pre-commit hooks passed before and after changes.
The full final CTest run passed 26 entries and skipped five (four private-data
checks and restricted UDP). A separate bounded dummy-SDL run inside `unshare -rn`
with the private installation copy passed `formats`, `engine` and
`script_campaign_inventory`; `save_import_private` skipped because its original
save fixture was absent. These installed-data checks do not establish live
reference gameplay. A read-only recount of the 15 `mission_N.mp` files found
49 Difficulty keys: 46 zero, two 75 and one 100, consistent with retaining the
key as metadata rather than deriving a bonus table. Changes remain uncommitted.
## L0 parity harness (2026-10-29)

**Construction / synthetic evidence:** `tools/parity_run.py` validates versioned
JSON specifications in `tests/parity/`, selects IDs/`--all`/`--layer`, lists
without launching, skips absent private inputs and retains private observations
and first-divergence reports under ignored `work/parity/`. The common integer
JSONL comparator validates complete coverage and reports tick, object and field
with explicit lifecycle/field/error categories. See [parity.md](parity.md) and
[the scenario schema](parity_scenario.schema.json) for supported semantics.

The headless engine can emit live-object HP, side, UGI type and fixed-point
positions before each tick's commands/update. Its IDs are engine-local;
`engine-index-v1` documents their units, lifecycle and tick origin without
claiming an original identity mapping. CTest `parity_scenarios` checks two mocked
runners, schema and selection failures, private skip behavior, first-difference
ordering, lifecycle, malformed coverage/contracts, namespace/error propagation
and actual engine observations from an invented installation. These are tooling
checks; no simulation P/M/V label changes.

**Private evidence and dependencies:** a bounded run with the supplied read-only
installation/runtime, separate private game copies, dummy SDL and `unshare -rn`
completed the clone's 12-tick horizon. Reference loopback setup failed with
`Cannot open netlink socket: Operation not permitted` before runtime launch;
the combined scenario reports `experiment_failed`. Details stay in
`work/parity/l0-private-20261029/`. There is no measured original observation or
semantic divergence. The audited reference normalization/identity observer and
packet translator from task 01 are absent. Nonempty private setup/input/replay,
unsupported layers and incompatible normalization remain explicitly blocked.
L0 runner/schema/comparator work and synthetic checks are implemented independently
of those missing prerequisites; whole-game/reference parity remains open.

Validation: baseline and final Release builds pass with `AZTEC_GEN_DIR=` and a
worktree-local ccache. Full CTest passes 27 entries; four absent game-data checks
and the restricted UDP process check skip. All pre-commit gates pass, including
explicit checks of new files. Private reference parity is not established by
these synthetic checks or by the independent clone smoke run.
## L4/L5 comparison instrumentation (2026-10-29, partial task 44)

`--ui-trace` records independent engine render/input observations and commands
submitted during the UI update. Scripted cursor animation now uses virtual
frame time. Synthetic controls exercise hover, held press, release, Move target
submission, Attack hotkey/cursor, attack-move and cancellation, with matching
traces across repeated runs. The six command buttons retain sprite frame zero
on press; selected modes rotate through presentation frames. Drawing and tracing
share frame/highlight/enabled state, with hover reported separately.
Production/queue state normalization and reference input semantics remain open.

The private-output-only comparator checks six independently supplied structural
masks, tolerant RGB metrics, normalized UI states and semantic command order.
Its synthetic failure controls verify diagnostic behavior, not game parity.
See [parity.md](parity.md#l4l5-visual-and-interaction-controls-2026-10-29-partial-task-44)
for schema, coverage, tolerances and limits.

A private clone capture succeeded under dummy SDL in `unshare -rn`. The reference
probe failed at loopback configuration before launch. Fixed-tick reference
camera/UI capture, audited identity/command normalization and task 42's common
runner are absent in this checkout. Matching scene comparison remains blocked;
UI fidelity remains **P**, with no new M/V reference evidence. Only code,
invented specifications and the aggregate
[results table](parity_visual_ui_results.md) belong in the public repository.

### L4/L5 iteration (2026-11-01, partial task 52)

**Synthetic implementation evidence only:** new regressions reproduced edge dots
overwriting all four minimap borders and UI traces reporting hover as highlight
and selected buttons as frame zero. Minimap contents now respect the map bounds
and the caller's clip, including disjoint clips, without changing the surrounding
frame. Command drawing and tracing share presentation state, including town
service/marked-building selection. Scripted tests check idle hover, Move rotation
and target completion; raster checks cover minimap edge containment and clip
restoration at four window/scale configurations with both font paths. These
presentation changes do not add simulation state or alter replay hashes.

The isolated private probe again captured a clone frame/trace, while reference
network setup failed before launch. No matching scene, per-region mismatch rate
or reference UI sequence was obtained. Task 42's shared runner, fixed-tick
reference camera/UI observer and normalization remain missing dependencies.
HUD, selection/resource panels, tooltips, fonts, cursor shapes, placement colors,
construction, health bars and selection circles still require aligned reference
measurements; no L4/L5 reference status is promoted. Captures and failure logs
remain under ignored `work/parity/`.

Validation: baseline and final Release builds passed with `AZTEC_GEN_DIR=`.
The default CTest run passed 34 entries and skipped five private-data entries
plus restricted UDP process creation. The separate private engine check and
final clone frame/trace capture passed under dummy SDL inside `unshare -rn`
using the private installation copy. Pre-commit and whitespace checks passed.
## L1 rule experiment preparation (2026-10-29, partial task 43)

**Construction / synthetic evidence:** 19 P scenario specifications cover siege
speed, damage/category controls, reload, range and projectile flight, mine build
time/price/income, sight, regeneration and serial/two-town production. They use
`docs/PARITY_TEST_PLAN.md`; task 42's harness was absent during that attempt.
The L0 harness now exists, but audited L1 adapters remain absent. Tasks 36/37/40
are not present in this checkout either. `tools/parity_rules.py` generates
private placement candidates and attempts isolated launch controls. Its separate
comparison command reuses the strict normalized comparator, requires two equal
reference repeats and complete scenario coverage, and stores integer-only
expectations plus the first clone divergence under ignored `work/parity/`.
Synthetic CTest controls cover export, differing fields, unstable repeats,
truncation, private settings/maps, missing-input skip and denied isolation.
These verify tooling, with no change to simulation rules or V/M/P labels.

**Reference evidence unavailable:** all 19 candidate launch attempts used private
game copies, immutable resource symlinks, dummy SDL drivers, hard timeouts and
`unshare -rn`. All failed at loopback setup with a denied netlink socket before
the runtime launched. Private outputs are in
`work/parity/l1-isolated-20261029/`. No measured rule, expectation, clone parity
pass or first semantic difference was obtained. Tick/object/field remain null.
Generated candidates still require audited commands, AI/tech/money/fog/HP
controls, town membership and normalized observers. Nonzero mine yields,
production branches, boundary/faction repeats and flight controls remain open;
see [the L1 parity record](parity.md#l1-rule-micro-experiments-2026-10-29-partial-task-43)
and [scenario instructions](../tests/parity/README.md). Task 43 remains partial.

**Local verification:** baseline and final Release configure/build passed with
`AZTEC_GEN_DIR=` and a worktree-local compiler cache. Final CTest passed 27
entries; five private-data entries and the restricted UDP process check skipped.
Pre-commit passed for all tracked files and explicitly for the new scenario,
tooling and test files using the copied local hook cache. No skipped check is
counted as a live original-game measurement.
## V19 campaign coverage update (2026-10-28, partial)

**Engine construction / synthetic evidence:** `tools/playthrough.py` now emits
an explicit matrix for missions 1 through 15, including missing/unselected data,
private scenario routes and expected victory/defeat outcomes. Every scenario runs
twice and retains status, unsupported action sites/executions, static unreachable
findings, unmet objectives, stalled orders, stderr and crash/wall/tick timeouts.
Per-tick transcripts include tick zero and report the first differing tick;
missing, gapped or truncated evidence cannot pass. A third fresh process loads
the ordinary AZS3 mid-mission save and compares its complete continuation suffix,
including the saved boundary and terminal outcome, to the uninterrupted run.
Inherited sight configuration is pinned to each private installation copy.

The renderer-free mode adds tick transcript, save-at-tick and resume options;
it uses existing content/script/hash verification and command replay. It resumes
scheduled orders and the automatic exercising policy at absolute mission ticks.
No simulation rules, player command semantics or outcome conditions were changed.
Synthetic tests cover all 15 fixtures through victory and saved continuations,
expected defeat and a separate fresh winning attempt, commands before/after the
save boundary, mismatched scripts, first differing ticks, malformed/incomplete
transcripts, missing mission coverage, scenario validation, crashes and timeouts.
These establish engine consistency, not original-game parity or real Retry UI.

**Private bounded observations:** the 24-tick isolated sweep reached every
mission with both dummy SDL drivers under `unshare -rn`, private ordinary Maps/
Saves directories, resource/map symlinks and copied configurations/profiles.
Paired hashes matched for all 15 missions. Fourteen continued identically from
tick-12 saves through tick 24; mission 11 ended in defeat at tick 1 before saving.
No mission won. These are two-second simulation probes, not campaign acceptance;
source data, transcripts, saves and complete matrices remain under ignored
`work/v19-private-sweep/`.

A second isolated sweep used a 1,200-tick budget (100 simulated seconds), saves at
tick 600, and a 15-second wall limit per process. Missions 1, 4, 10 and 11 ended
in defeat; 2, 3, 5, 6, 7 and 9 reached the tick limit; 8, 12, 13, 14 and 15 exceeded
the wall limit in both runs. No crashes or victories were reported. All ten
completed pairs matched every tick. Seven missions (2, 3, 5, 6, 7, 9 and 10) also
matched fresh-process save continuations; 1, 4 and 11 ended before the save tick.
The five wall-limited pairs have unverified hash/save evidence; expensive work
within this host's budget is not proof of a hang. The runner correctly exited
one. Executable/rules snapshots, paired transcripts, saves and the full matrix
are retained privately under `work/v19-campaign-sweep/`. These observations use
the exercising policy, not validated player routes. Immediate mission-11 defeat,
other lost objectives and expensive missions remain investigation leads, not
newly diagnosed engine rule defects.

**Reference boundary:** a bounded mission-1 record attempt with the supplied
read-only reference runtime used a separate private installation with copied map
files and read-only resource symlinks. Namespace loopback setup failed before
launch with `Cannot open netlink socket: Operation not permitted`. No command
recording was produced, so replay and normalized comparison remain unverified.
Failure logs and navigation input remain under `work/v19-reference/`; required
isolation was retained. Engine self-comparison cannot replace that missing oracle.

**Remaining dependencies:** V03-V16 are not fully accepted in this checkout.
The independent harness and inventory above do not require those remaining
adapters. Successful routes still depend on movement/transport, combat/economy,
AI, script objective semantics, controls and campaign save/progression behavior.
The existing audit lists 19 unsupported transport action sites in missions 2,
7, 9 and 15, plus two missing-trigger warnings in mission 6. General objective
reachability, player-controllable winning routes, same-input defeat/retry,
front-end progression/final ending and Linux/Windows interactive acceptance remain
unverified. Every generated matrix keeps interactive, retry and progression
acceptance pending. V19 remains open; see [method and full planning matrix](playthrough.md).

**Validation:** Release build and full CTest pass (26 passed, five skipped).
The skipped entries are four default private-data checks and restricted UDP
processes. A separate dummy-SDL `unshare -rn` run against a private copy passed
formats, engine integration and campaign script inventory; original-save import
skipped because original saves are intentionally excluded. Pre-commit and
`git diff --check` pass. Neither the synthetic checks nor bounded probes prove
full campaign completion, reference parity or native Windows behavior.


## Parity triage and matrix tooling (2026-10-30)

**Construction / synthetic evidence:** task 42's harness is available, and
`tools/parity_triage.py` groups its direct or nested first-divergence reports
into private review drafts under ignored `work/parity/tasks/`. It preserves
existing reviewer edits and never queues work. `tools/parity_matrix.py` writes
only aggregate scenario-result, pass/fail/skip and category counts to
[parity_matrix.md](parity_matrix.md). Unknown private categories use a fixed
public bucket; absent layer metadata remains `Unknown`. Invalid/conflicting
metadata fails rather than inventing coverage. See [parity.md](parity.md) for
selection, repeated-run counting, output restrictions and exit codes.

CTest `parity_triage_matrix` exercises invented diffs without game data:
grouping, nested harness reports, first-category extraction, classification,
deterministic aggregate counts, exclusion of private values, draft preservation,
malformed input, path/symlink restrictions and missing-input skips. A fresh
actual harness invocation with `synthetic_l0` and `private_l0` produced one
mocked equality and one absent-private-input skip; these are the committed
matrix's two results. No live game process or reference measurement was used.
The audited reference observer/identity and replay adapters remain missing.
No simulation rule or P/M/V evidence label changes; reporting completion does
not close whole-game parity.

Validation: baseline and final worktree-local Release builds passed with
`AZTEC_GEN_DIR=`. The final full CTest suite passed 32 entries and skipped six
(five private-data checks and the restricted UDP process check). Pre-commit
gates passed for tracked files and explicitly for the new tools, test and matrix.

## Task 36: mixed construction and unit queues (2026-10-29)

Historical record: task 48 below changes placeholder creation time and adds an
optional parallel construction mode. Default town progression remains serial.

**Reproduction:** `test_mixed_production_queue` in `engine_unit` failed before the
fix: construction appeared while earlier unit orders were pending, queued orders
consumed cash, and pausing town 0 also stopped town 1. It uses invented terrain
and ordinary public rules tables; it requires no original assets.

**V, inspected reference behavior:** a town owns one active production item and a
FIFO queue of up to 50 waiting objects. Selecting a different building in the same
town does not create another production lane. Separate towns progress
independently. Build admission reserves an item without paying; production tests
prerequisites and available cash, then charges once at start using that town's
current speed. An unaffordable head waits. Cancelling a waiting reservation does
not credit cash. Active pause/resume and active cancellation address a town's
current item. Active cancellation credits the unbuilt fraction of the recorded
payment, using completion in 256 parts and integer truncation. Evidence anchors:
town production `0x459bb0`, admission `0x45cec0`, waiting cancellation `0x45cfd0`,
active cancellation `0x45a880`, timer accessors `0x40a480..0x40a4db`, and packet
0x21/0x22 dispatch in `0x4577b0`. These are behavioral facts from private inspection;
no executable-derived implementation or data has been added to the repository.

**Independent implementation:** map construction, automatic structures, hall
upgrades and units use one town-owned queue. Waiting map placements reserve their
footprints, but create a live incomplete building only when payment succeeds.
The active item keeps its original paid price and duration across later speed
changes. Pausing it holds subsequent items in that town. Cancellation releases
its type/placement reservation, removes live incomplete construction and its
navigation blocker, and returns only its unbuilt payment. Destroyed construction
is discarded without a cancellation refund. Menus permit unpaid orders; the
scheduler rechecks affordability/prerequisites before starting. The panel admits
50 waiting items, scrolls over all entries, pauses the selected town, and shows
waiting/paused state and the refundable amount. Command fields are engine
conventions documented in `commands.hpp`, not reference packet compatibility.
New item state participates in the deterministic hash. Save replay version is 3;
older engine saves are rejected rather than silently replaying different rules.

**Synthetic checks:** mixed building/unit ordering through different members of
one town, independent towns, FIFO cash stalls, unpaid cancellation, partial active
refunds without double credit, construction pause/cancel and released footprint,
speed selection at start, active/waiting capacity, UI admission and cancellation
beyond slot eight, new-state hash sensitivity and command-log replay. Existing
upgrade save/replay and AI checks cover the new payment timing.

**Updated by task 96:** production freezes the truncated integer work increment
at payment; training uses its quantized duration and explicit construction advances
actual HP, so damage delays completion. A completed item can start its successor
in the same update. Refunds still use the engine queue's progress fraction;
hall-upgrade HP transitions, exact campaign start ticks, automatic placement and
spawn locations remain unresolved. Shared-cash town arbitration retains stable
storage order pending measurements. See [the partial early-spending audit](early_payment.md)
for the observed lump payments, all twelve money samples, the corrected submarine
controller count, and the still-missing side-plan requests. Native diagnostics do
not establish full normalized production or live campaign money parity.

The isolated reference attempt used the supplied read-only runtime, a private game
copy, dummy SDL drivers and `unshare -rn` through `tools/zstat_run.sh`. It failed
before launch at loopback setup with `Cannot open netlink socket: Operation not
permitted`; inputs and failure output are under ignored `work/queue-reference/`.
It establishes no live queue parity and was not rerun outside isolation.

**Validation:** Release configure/build, all 26 runnable CTests and all pre-commit
hooks passed. Four private-data entries and the restricted UDP process test skipped
in the default 31-entry suite. Separate `test_formats` and `test_engine` runs
passed against the private installation copy under dummy SDL and `unshare -rn`.
A bounded engine mission 1 smoke run exited successfully with no script execution
errors, ending in defeat at tick 1196 of a 1200-tick limit; it is neither a winning
route nor a reference comparison. These results do not close the live timing/HP
and reference observer dependencies above.

## UI interaction feedback (2026-10-29, partial parity)

**Reproduction:** the asset-independent `ui_screenshots` test was extended before
changing the engine. Invented twelve-frame command sprites exposed a frozen
selected command, and an exact whole-button comparison exposed the extra hover
rectangle. Both failed on the baseline. No original pixels are test fixtures.

**Reference facts (V, local resource and control inspection):** common command
resources 257/268/269/275/288/301 contain twelve stacked rotation frames; Menu
282 contains two 60x22 frames. The Menu control selects its second frame while
pressed, clears depression when dragged out, and activates on release inside.
The shared command control advances its rotation counter on presentation updates;
mode-specific selectors return that counter while active and frame zero otherwise.
The production grid uses palette shade 9 for an available entry and shade 1 for
an unavailable entry. Its normal path does not add the engine's gold hover border
or construction-letter overlay. These facts were checked locally at the shared
control, Menu, active-mode and production-grid selectors; they are inspection
evidence, not a successful reference playback or a measurement of elapsed time.

**Independent implementation:** Menu depression now uses its recovered frame and
captures press/release. Command-card activation also captures an enabled control
and rechecks availability on release; dragging to another control, losing focus,
changing selection or cancelling cannot activate the original control. Queue
length changes invalidate a held queue control. Active targeting and town-service
controls use the recovered rotation frames. A presentation-only counter advances
once per input/render update, including while the simulation is paused; repeated
rendering of the same snapshot is pure. The counter and capture state never enter
simulation hashes or command logs. Production availability uses the original
palette shades, and generic command/production borders and the construction-letter
overlay are removed. Selection changes clear stale targeting/placement and paging.

**Checks:** synthetic captures cover both font paths and four window/scale layouts,
exact idle/hover regions, selected-frame changes and twelve-update wraparound,
Menu hold/drag/re-entry/release/focus loss, and indexed production palette states.
Command tests cover release-time availability and replay hashes. A private dummy-SDL
mission 1 script selected a unit, activated Move and cancelled it inside
`unshare -rn`; captures remain under ignored `work/ui-feedback/`.

**Open (P):** the reference probe again failed before launch at namespace loopback
setup (`Cannot open netlink socket: Operation not permitted`). Synchronized visual
comparison and wall-clock animation cadence remain unverified. The existing
engine allocation and semantics of command-card cells, town-service indicators,
production paging/queues, tooltip timing and unsupported original modes remain
partial; this change does not establish full mission 1 UI parity. Full service
and upgrade presentation still depends on the incomplete town-command work
identified in the earlier UI inventory. No runtime code or private assets changed.

**Final validation (2026-10-30):** the worktree-local Release build passed with
`AZTEC_GEN_DIR=`. CTest passed 26 entries and skipped five (four checks requiring
private data in the default configuration and the restricted UDP process check).
All pre-commit hooks passed. The separate private-data `test_engine` check also
passed inside `unshare -rn` with dummy SDL and the private installation copy.
## L6 audio event audit (2026-10-29)

The audio-specific normalizer/comparator is independent of the absent task 42
shared scenario harness. Synthetic controls cover all seven trigger classes,
interleaved order, missing/extra requests, ID/kind differences, one-tick timing,
incomplete/malformed captures and rejection of unresolved reference identities.
The engine dispatcher supplies building/mobile-animation contexts, preserves
loop provenance and timestamps music with the current caller tick. Direct
selection/order/UI context tests exercise the API; they do not implement or
verify those missing application dispatchers. Generic effects remain unknown.

The reference sound shim records buffer Play/Stop/release requests against the
existing command clock. It has no resource-ID/trigger observer or streamed music
track-change observer. Raw handles are never treated as sound IDs. Reference
normalization therefore requires independently audited private annotations and
tick alignment; the probe alone cannot supply them. Full L6 acceptance remains
open pending those observers, shared scenarios and successful paired captures.

The private isolated attempt under `work/parity/l6-audio-probe/` failed at
namespace loopback setup with a denied netlink socket, before the supplied
runtime launched. Selection, orders, combat, building, UI, music and ambient
comparison results are all unmeasured. No gameplay rule is promoted to M/V.
An isolated clone-only scripted smoke run produced a valid nonempty event file;
it verifies logging integration only. An earlier timed-out clone probe remains
a failed/incomplete capture. Release build, 28 runnable CTest checks and
pre-commit gates passed; five private-input/network checks skipped.
The [L6 method](parity.md#l6-audio-event-comparison-2026-10-29) records commands,
formats, exit codes and evidence limits. No original audio, IDs or trace values
are public fixtures.

### L6 iteration (2026-11-01, partial)

Three synthetic regressions were reproduced before correction: missing provider
fallback decoration requests, loop cleanup targeting the surviving decoration
after an earlier one ends, and duplicate fire requests when a simulation effect
owns the fire. Rendering and audio now share decoration selection, preserve
channel positions, and apply the renderer's existing fire ownership suppression.
No simulation state, RNG, commands or timing rules were changed.

The application dispatcher exports an invented log that task 45's comparator
matches across 37 requests, with exact ticks additionally required by the test.
Tests cover construction silence, built/damaged objects, healing, visibility,
distance, removal, held frames and independently ending death decorations.
Loop source checks supplement the comparator's sound/kind/trigger contract.
The 1,200-tick audio-independence test now includes damaged/dying objects and
retains complete hash equality at every tick plus replay equality.

The isolated reference capture failed before launch at namespace loopback setup;
all seven live trigger-class comparisons remain unmeasured. Two isolated clone
smokes completed using separate private game copies; audio enabled/disabled
runs reached the same final simulation hash. The enabled run logged requests.
These results establish engine integration only. Selection/order speech, UI cues,
production/completion announcements, generic effect provenance and original music
transition timing remain open. Task 45 tooling is available; audited reference
identity/trigger/music observers and synchronized paired scenarios are still
required. Private logs and before/after diffs remain in `work/parity/`.
The final Release build, all 34 runnable CTest checks and cached pre-commit
hooks passed; six private-input/network checks skipped. The task 45 normalize
and compare CLI commands also passed the final invented capture.

## Task 47: placement, footprint grid and construction (2026-10-30)

**Reproduce before fixing:** the unchanged Release build and pre-commit passed.
The new deterministic `engine_unit` regression then failed eight assertions:
a barracks' western occupied arm was accepted over blocked terrain, an empty
corner was rejected, a ground unit was ignored, a remote site and an empty mine
site were accepted, the mine button submitted automatic production instead of
entering placement, a mixed-validity footprint had no green cells, and a progress
update erased construction damage. These checks use invented terrain, objects
and software-renderer pixels without game assets.

**Independent implementation:** Building and Defence descriptor masks now drive
placement checks, queued reservations, navigation insertion/removal, upgrades,
death, cancellation and debug teleport. Occupancy is centred on the clicked tile,
not inferred from the sprite's pixel bounds. Task 150 supersedes the original
placeholder predicate: the UI checks each body cell before sending a command;
admission and payment/start do not repeat placement validation. Object tiles
replace collision discs, including flying objects; dead/removed objects are
ignored. See [placement rules](placement_rules.md) for V/G evidence boundaries.

Every non-upgrade structure button enters placement mode, including all three
factions' mines and repeatable farms/defences. The preview follows the current
camera/cursor and draws each occupied tile with its own validity. It is drawn
above terrain and below objects/fog; it is hidden over the panel, minimap and
modal UI. Left-click confirms a legal site; an invalid click keeps placement
active; right-click or Escape cancels without payment. Placement binds the
producing town when the mode starts and cancels if its remembered member
vanishes. Build-tree uniqueness flags are separate from this UI decision.

`PlaceBuilding` (engine command extension 0x52) supplies explicit coordinates for
flag-0 structures. Existing `Build` flag-0 logs/AI orders retain automatic
placement; hall upgrades remain in-place operations. Both paths use the same
placement checks. The new command is accepted by command-log/save and lockstep
validation. Invalid-tag tests now use the next unsupported extension, 0x53.

Explicit construction is paid only when it starts at the head of its town queue
by default. Task 48 below moves placeholder creation to command acceptance and
adds an optional independent building scheduler.
The town hall owns production; this implementation introduces no worker unit or
worker requirement. Pause holds elapsed work and HP. Each progress tick adds
only that tick's HP contribution, retaining prior damage; completion switches to
the built animation, and harvesting starts on the following tick. Slot 0 stages
still seek the established HP-based construction frame, including the initial
interval without a body frame. Cancellation removes the live incomplete object
and its exact navigation mask, with the existing 8.8 unbuilt-payment refund.
Destroyed construction yields no refund. Occupied-mask bounds also replace
sprite dimensions for the existing approximate structure reach and producer
exit offsets.

| Rule | Evidence | Boundary |
| --- | --- | --- |
| Descriptor occupancy and construction frame selector | V static reference / engine construction | Only mask value 2 affects placement; 0/1 are ignored (`0x409940`) |
| Town-local radius | V static reference / implemented | Every body cell must satisfy squared distance from the producing hall tile < 64; radius 8 (`0x45bd50`, `0x409940`) |
| Terrain and resources | V static reference / implemented | Layer 0 nonzero (`.dat >= 2`); no deposit exception or standalone overlay/mineral rejection. Non-defence d60/d61 request A/B; one body cell nonzero in either requested channel suffices |
| Collision, light and reservations | V static predicate / G engine mapping | Empty body object cells, no orthogonal building neighbours; side byte > 32. Engine uses structure masks and current object tiles; exact grid membership, side-map identity and reservation timing remain G |
| Valid/blocked preview colours | P | RGBA (60,220,60,110)/(230,50,60,110), outlines alpha 230; original colours, opacity, grid shape and line width are unmeasured |
| Start, workers, damage growth, completion and refunds | V engine construction / P original timing and worker participation | Shared town queue and prior task 36 refund calculation; no live construction equivalence claimed |
| New live measurements | P unmeasured | Isolated reference launch failed before the game started; no colours, rules or timings inferred |

**Reference attempt:** a private game copy under ignored
`work/placement-game/` has read-only resource/executable symlinks and ordinary
Maps/Saves/config files. A bounded UI capture script used the supplied reference
binary, both dummy SDL drivers and `unshare -rn`. Loopback setup failed with
`Cannot open netlink socket: Operation not permitted`; launch output is retained
in `work/placement-probe/launch.log`. Isolation was not bypassed. Repeat this
experiment on a host supporting namespace interface setup before promoting P
rows. The aligned live reference observer/record-replay dependencies described
in V02 remain unresolved; the independent implementation and synthetic checks do
not require them.

**Coverage:** three-faction mine UI selection, invalid/valid clicks, right-click
and Escape cancellation, zoomed cursor coordinates, stable town identity with
sparse town numbers, empty/mineral controls, map/radius boundaries, mixed red/green
pixels, payment, stages, pause, cancellation/refund, footprint release,
completion/first harvest and serialized command-log replay. Existing mixed
construction/unit queue, hall upgrade, destruction and long deterministic
simulation checks remain required. No original assets or captures are tracked.

**Landing correction against develop `5eff2ea`:** the previous validation used
the older worktree base. A fresh archive of current develop passed its baseline
suite; applying the first task 47 patch reproduced the reported `ai_skirmish`
failure, including no waves, firing or losses in the final five-minute interval.
The earlier richer-resource/road fixture adjustment was not a sufficient landing
check and has been removed.

Two new engine regressions reproduce construction-related problems independently
of strategist timing. Twelve automatic farms can seal the producer's approach
area; automatic placement now leaves a one-tile gap between occupied structure
masks, including AI map-construction offers. A completed ground unit was created
at a fixed exit even when every tile was blocked; production now retains the
preferred exit when it fits, otherwise searches tile centres in stable square
rings out to 16 tiles. If no collision-free ground exit fits, the paid queue item
waits at zero; it remains cancellable with no unbuilt-work refund. A single-free-
tile control and command-log replay verify successful fallback and suspension.
These spacing/search choices are P engine policies, not measured original rules.
Blocked placement cells skip unnecessary object scans, and automatic spacing uses
bounding rectangles to avoid testing distant masks.

The twenty-minute pressure/replay fixture now holds technology constant with
mission DisabledID: initial village (11), footmen (0) and repeatable farms (12).
Its original terrain, resource richness 130, 2000 starting cash, economy script
and synthetic hall HP are retained. This isolates sustained opposition from
unmeasured high-tech campaign balance when footprints/formations change.
Development, prerequisites, defences, higher-tech offers and town-local commands
retain their separate unit coverage. Every original per-five-minute offensive
wave/firing/loss/birth assertion, both-producer survival check and per-tick
viewing-side/replay comparison is retained. The strategist state machine, production price/time formulas and assertions
are unchanged.

The corrected current-develop soak observed offensive waves 3/3/1/4, firing
304/816/278/3304, losses 21/25/21/28 and births 32/32/32/35 across its four
five-minute intervals, with final hash `374e05968c563276`. These are synthetic
fixture observations, not reference measurements or campaign acceptance.

**Final landing validation:** develop advanced during this correction. The final
integration uses `1a4f9536349dce83f934a39de6a64deb9c476e4e`, including the later
siege and UI-feedback changes. The UI tests send full press/release pairs and
respect the shell's selection-change cancellation; the remembered producer is
checked only while placement is active. This also avoids conflicting cancellation
resets with the UI-feedback work.

Release build and all 39 integration CTests passed: 33 passed, six skipped
(five default private-data entries and restricted UDP process creation).
`ai_skirmish` passed in 17.91 seconds with the original pressure assertions and
unchanged 180-second timeout. The older worktree's full suite and separate
private-installation formats/engine tests also passed under dummy SDL and
`unshare -rn`. All pre-commit gates and whitespace checks passed.
The integration copy preserves develop's changes outside this task; all tracked
changes remain uncommitted because git metadata is read-only.


## Task 48: placeholders and optional parallel construction (2026-10-30)

**V engine construction / P live placeholder parity:** an accepted explicit build
or placement command immediately spawns a visible, one-HP building at construction
stage zero, with its navigation footprint. It remains unpaid with zero elapsed
work until production starts. Start rechecks prerequisites and cash without
rejecting its own placeholder. Waiting cancellation removes the placeholder and
blocker with no refund. Active cancellation retains the established unbuilt-paid
fraction refund; destroyed placeholders or construction yield no refund.

**Default original-style progression:** construction, automatic structures, hall
upgrades and units retain one active item per town and FIFO order, including
unaffordable-head stalls. Task 36 records V private reference control-flow
inspection for that policy. Immediate placeholder creation is the required
original-style behavior, but its live visual/start timing remains P here.

**Clone-only modification, V engine construction:** `parallel_construction` is off
by default. When enabled, ordinary buildings (explicit and automatic) advance
independently of each other and the unit queue. Units and hall upgrades retain
one serial lane per town. Shared cash is spent in insertion order; each start
chooses the current speed and records its price/duration. Missing/incomplete
halls suspend work. Capacity stays 50 waiting items per town plus started work.
The AI keeps its existing conservative busy-town planning policy and uses the
same production scheduler. Automatic structures retain placement at completion.
Switching the modification off makes existing work obey the serial policy again.

[Game modifications](modifications.md) documents persisted preferences, CLI,
front-end/pause-menu toggles, logged live changes, save metadata and lockstep.
The panel retains scrolling slots, individual progress/cancellation/refund and
unit order. Placed active constructions show active/paused markers and support
Shift-click pause/resume; the main pause control targets one item. All future
simulation inputs are hashed, including initial and current modification sets.
AZS3 replay version 4 records initial modifications and rejects older versions.

**L1 attempt, no new M/V reference evidence:** a private game copy with resource
symlinks, private Maps/Saves and copied settings was prepared in
`work/parallel-production-game`. The supplied reference runtime was requested via
`tools/zstat_run.sh zparallel work/parallel-production-probe 12000` with command
and state logs, dummy SDL drivers and `unshare -rn`. It failed at loopback setup:
`Cannot open netlink socket: Operation not permitted`. The game did not launch;
no record/replay or placeholder observations were produced. Parallel construction
is explicitly a product modification, not a parity claim. Exact placeholder
visuals, worker involvement and completion alignment remain P. A functioning
normalized reference observer/runner is still needed for measured L1 comparison;
implementation and synthetic tests do not depend on that missing evidence.

**Synthetic coverage:** default serial training and visible unpaid placeholders,
waiting cancellation and released placement; two independent paid sites with
serial units in parallel mode; duplicate-site rejection; individual pause,
resume, cancellation and refunds; cash stalls and payment speed; queue hash
sensitivity; save metadata and per-tick continuation/replay equality in both modes;
logged mode changes; preference parsing/persistence and UI toggles; initial
lockstep mismatch rejection. Existing default AI economy smoke coverage remains.

## Windows package and laptop readiness (2026-11-01)

**Construction evidence, no simulation parity claim:** Windows release build and
fresh-runner zip smoke jobs target `windows-latest`. The package stages the
independent executable, allowlisted SDL2/MSVC runtime DLLs, public rules, GPL
license, dependency notices, SDL2 source and installation README. The smoke job
removes compiler/vcpkg directories from PATH. Windows DLL execution and release
publication still require an actual successful workflow run; this Linux worktree
cannot establish those results.

Both application modes accept literal `game=DIR` settings, including CRLF,
backslashes, drive letters and spaces. Precedence is CLI, nonempty `AZTEC_GAME`,
settings, then `./game`. Normal slot saves, campaign progress and modification
preferences use SDL's per-user application directory; `AZTEC_USER_DIR` selects an
absolute isolated profile. No automatic migration from installation-local saves
occurs. Explicit headless output paths and `--modifications-file` remain overrides.
Fullscreen requests check SDL's result before changing the tracked display mode.
The mission frame limit now also works without requesting a screenshot.

**Local synthetic evidence:** Release build and full CTest pass (34 passed;
five private-data checks and the restricted network-process check skipped).
Pre-commit passes. A relocated local package passes under `unshare -rn` with dummy
SDL video/audio: repeated victory at tick 10, mixed-case resource/mission names,
spaces in paths, settings/environment/CLI selection and precedence, per-user
slot save/load, bounded exclusive/borderless startup, and malformed/missing input
errors. Synthetic Windows staging preserves the required files and rejects an
unallowlisted runtime path before creating a package. No Windows DLL was executed
in that staging check. The portability test checks SDL's native preference path,
literal invented Windows settings and successful dummy audio-device startup,
plus graceful failure with an unavailable audio driver. Cursor/focus/Alt/F9 and
fullscreen state/flag checks run through SDL's dummy driver, which can promote
exclusive requests to desktop fullscreen when no matching display mode exists.

**Remaining acceptance:** native Windows path and DLL loading need CI results.
Physical cursor confinement, Alt+Tab, DPI scaling, fullscreen driver transitions,
sleep/resume and speakers/headphones need Windows laptop testing. Dummy drivers
and synthetic replay do not establish hardware behavior or reference-game parity.
No task dependency is needed for these setup changes; full campaign acceptance
and reference comparisons remain separate outstanding work.
## Campaign driver fixes (2026-11-01, partial task 50)

**Engine construction / synthetic evidence:** headless scripted move and
attack-move selection/validation now accept flying units as well as ground
mobiles, matching the ordinary command handler. Previously flying IDs were
rejected or omitted. Automatic exercising now filters hostile destinations using
the existing weapon result's known getters and positive known damage, so an
ineligible nearby target cannot divert a unit from an eligible one. No combat,
AI or script rule was changed. Both regressions failed before their fixes;
synthetic checks cover flight orders, exact save continuation and automatic
versus explicit eligible-target orders including rolling tick digests.

**Private evidence:** all 15 missions were run twice with computer AI and private
regroup orders under `unshare -rn` with both SDL dummy drivers and private game
copies. Final 24-tick pairs matched throughout; fourteen fresh-process tick-12
save continuations matched. Mission 11 lost at tick 1 before saving. No crashes,
wall timeouts or static unreachable findings occurred in this short sweep; no
mission won. Mission 4 also reproduced flying-ID refusal with the baseline and
movement with the fixed executable. Mission 7/9 orders address computer troops
because the current loader places no player mobile units; transport/cargo
behavior remains unexercised. See the [current campaign matrix](playthrough.md#campaign-fix-iteration-2026-11-01-partial)
for per-mission action execution counts and longer bounded runs. Raw plans,
hashes, saves, executable/rules snapshots and diagnostics remain under ignored
`work/parity/`.

The final 1,200-tick sweep with tick-600 saves and a 20-second process limit
produced paired defeats in missions 1, 3, 4, 10 and 11; paired tick limits in
2, 5, 6, 7 and 9; and paired wall timeouts in 8, 12, 13, 14 and 15. All ten
completed pairs matched throughout; six save continuations matched. No crashes
or victories occurred. Completed reports had no static unreachable findings or
stalled movement IDs; the five wall-limited pairs have unverified hash/save
evidence. These are coverage-driver observations, not validated player routes
or proof of hangs. The full per-mission table is in playthrough.md.

**Dependencies / limits:** the initial mission-11 protected-group check refers to
town scenario membership, which the current team-tactic lookup does not resolve.
The complete town/tactic, ownership/capture and transport models remain missing;
no speculative alias or action no-op was introduced. Required transport action
sites and mission-6 missing-trigger warnings remain as previously inventoried.
Winning routes, general objective reachability and interactive progression remain
unverified. A reference recording control again failed at namespace loopback
setup before launch; no reference parity labels or V19 acceptance were promoted.

**Validation:** Release build and full CTest passed, with 34 passed and six skipped
(five default private-data checks and restricted UDP processes). Separate
private-copy dummy-SDL namespace checks passed formats, engine integration and
campaign script inventory. The new regressions are synthetic and require no game
data. All pre-commit hooks and `git diff --check` passed.
## L1/L2 rule iteration (2026-11-01, no new reference evidence)

The initial run attempted all 19 existing L1 candidates and nine recorded L2
missions; the final run attempted 26 L1 candidates and the same nine recordings.
Every reference attempt failed before runtime launch at namespace loopback
configuration with a denied netlink socket. No HP, armor, regeneration,
production, income or repair rule can be inferred from these failures. Simulation
code and M/V/P labels are unchanged.

Seven new P specifications add barracks construction, single-unit training,
organic/building regeneration, building/defence repair and farm income controls.
The optional L1 clone runner uses a separate game snapshot made before reference
execution. All 26 final clone candidate controls completed their full engine-local
observation horizons; these are setup blockers, not L1 parity passes. Commands,
damaged HP, town membership and other controls still require audited adapters.
L2 clone replay remains dependent on missing packet/identity/observer adapters;
no substitute replay or reference repeatability was claimed.

Per-case L1/L2 diffs now feed the aggregate matrix and private triage directly.
The final selected run contains 35 experiment failures and zero measured rules
or semantic divergences. See [the iteration record](parity.md#l1l2-iteration-2026-11-01-blocked-measurements)
for commands, evidence boundaries and remaining work. Asset-free controls cover
clone horizon validation, failure aggregation and independent mutable game copies.

## Conflict-resolution audit (2026-11-04)

The [task-by-task review](AUDIT_2026-11.md) maps tasks 01–48 to current code,
registered checks and remaining acceptance gaps. Duplicate range/audio rows and
superseded scalar-sight, acquisition, original-save and AI-skeleton claims were
removed or reconciled with the current implementation. This is a documentation
correction; no reference rule or simulation parameter was promoted.

A synthetic integration regression now feeds the actual L6 audio comparator into
triage/matrix generation. Equal nonempty event streams count as observed passes;
missing, extra, reordered or late events remain failures, and empty streams do
not establish a pass. These checks exercise tooling, not reference audio parity.
## Campaign input and outcome iteration (2026-11-04, partial)

This iteration addresses V06/V12/V16/V19 engine consistency; the full roadmap
rows remain open. It does not establish reference parity.

**Selected inputs:** both headless and graphical mission runners explicitly load
simulation sight from the selected `GameData` installation. `RulesData::load_dir`
loads tables without consulting `AZTEC_GAME`. Save fingerprints use the same
selected resource root, including when graphical rules fall back to defaults.
Previously an unselected environment installation could change sight inputs or
refuse an otherwise compatible mid-mission save. An invented-data regression
failed before the correction and passes afterward; a changed selected resource
still refuses loading. Another regression loads a headless tick-5 save through
the real mission UI and continues to synthetic victory. This establishes the
exercised continuation, not ordinary campaign completion or original-save resume.
Older saves may be refused if their resource fingerprints or replay state differ.

**Outcome diagnosis:** the script runner retains the first human mission outcome's
trigger ID, action source line and simulation tick. Headless JSON exposes it as
`outcome_origin`, or null if no local script outcome occurred. Foreign-side and
later conflicting results do not overwrite it; initialization clears it and
command replay reconstructs it. This bounded diagnostic does not affect gameplay
hashes or script order. Synthetic controls cover these cases and fresh-process
resume. The new multiline regression also exposed source locations recorded
before leading whitespace was skipped. Conditions/actions now report their own
first-token line, with blank-line and CRLF coverage. Line metadata is excluded
from simulation hashes.

**Private observations:** isolated dummy-SDL campaign sweeps used private resource
symlinks under `work/parity/`, both SDL dummy drivers and `unshare -rn`. Each of
15 missions was run twice for up to 24 ticks with a tick-12 save. The runs report
fourteen tick limits and mission 11 defeat on tick 1; no winning route is claimed.
Mission 11's first terminating action is now identified directly by provenance.
It tests a protected tactic represented by town scenario membership, absent from
the engine's team-only lookup. The town/tactic, gifting and ownership/capture
contract remains unresolved; no guessed alias has been added. All 15 paired
transcripts match; 14 save continuations match, while mission 11 ends before its
save. These bounded observations do not prove objective reachability or balance.

A separate mission-1 probe saved at tick 20 and resumed in a new process with a
conflicting environment installation through tick 60. Every suffix tick hash
matched; both runs reached the tick limit. Separate private-copy format, engine
and campaign-inventory checks passed. The reference control failed before game
launch at namespace loopback setup (`Cannot open netlink socket: Operation not
permitted`). Record/replay, measured town identity and original campaign outcomes
remain unavailable. Raw reports, saves, scripts and captures stay private.

**Remaining steps:** establish the town/tactic identity and gifting contract,
including production and service ownership; complete water/transport movement
and command adapters; measure required transport actions; then obtain winning
routes with fresh-process saves through outcomes, retry and progression for all
15 missions. V02/V05/V10 dependencies remain incomplete in this checkout. V17-V20
native CI, package, hosted sanitizer and campaign acceptance evidence is pending.

**Validation:** Release build and full CTest, plus the isolated private checks
above, are recorded for this iteration. The default suite passes 37 tests and
skips six (five private-data checks and restricted UDP processes). The repository
asset/English hooks and `git diff --check` pass. Full pre-commit cannot initialize:
the default cache is read-only and a worktree-local cache cannot fetch hooks
because GitHub DNS is unavailable. This is an uncompleted gate, not a hook pass.

## V12/V19 transport and objective diagnostics (2026-11-04, needs live run)

**Construction / synthetic evidence:** script execution now retains bounded
per-source-trigger counts and first/last dispatch ticks, exposed as per-action
observations. Repeated trigger IDs retain separate execution histories and shared
enablement. Histories are reset on initialization, reconstructed by replay and
excluded from gameplay hashes. Terminal enablement and false condition lines
are snapshots, not historical explanations. A disabled local victory trigger
now appears among unmet objectives even when its conditions are true.
`playthrough.py` validates the histories, reconciles unsupported dispatch totals,
and writes private findings for every unsupported/unobserved action. Headless
object observations with `--order-observations` include order, pixel destination,
target, tactic and scenario alongside fixed-point positions for investigating
stalled-order IDs. Their header declares the added fields; these are engine
identities, not reference mappings.

`script_unit` has separate controls for BoatActivate, JohnkaEnable, JohnkaDisable,
SubmarineGo, SideBuildCommand, SideMoveCommand and SideCameraDeactivate, covering
unobserved sites, dispatch, self-disable and reset. Duplicate IDs and replay
history are covered. `playthrough` checks source-site identity, malformed counts,
private findings, disabled objectives, resumed history and observation neutrality.
No unsupported branch became a successful no-op. Gameplay script/transport rules
and existing timing conventions are unchanged.

**Read-only private data evidence:** all 15 supplied scripts still parse, with
19 required unsupported transport sites in missions 2/7/9/15 and the two existing
mission-6 missing-trigger warnings. The [site disposition table](campaign_scripts.md#transport-follow-up-2026-11-04-needs-live-run)
accounts for every unsupported campaign site. No site has evidence that the
original never fires it. The other four unsupported grammar branches have no
sites in this campaign; campaign absence is not original behavior evidence.

**Dependencies / status:** V02 identity observers, V05 water movement and V10
command/ownership contracts remain incomplete. No measured loading, boarding
order, unload point, route choice, activation timing or objective completion
rule was available to implement. Mission 11's town-derived protected tactic,
gifts and immediate engine defeat remain unresolved. Live reference runs were
explicitly denied and were not attempted. The [exact live run sheet](EXPERIMENTS.md#campaign-transport-and-objective-run-sheet-needs-live-run)
prepares separate record/replay copies and normal campaign navigation for all
15 missions, followed by transport/objective controls. V12 and V19 remain open;
this task's status is **needs live run**.

**Validation (2026-11-05):** Release build and the full 46-entry CTest suite
passed with 40 passes and six skips (five private-data entries and restricted
UDP processes). The default parity observation contract passes its existing
scenario checks; order details require the explicit diagnostic option. The final
playthrough regression rerun and all pre-commit hooks passed after correcting a
codespell finding in test assertion names. `git diff --check` passed. Bash and
Python snippets in the live run sheet passed syntax checks without executing
reference commands. No campaign completion or reference parity was measured.


## V06/V10 measurement preparation (task 90, 2026-11-06)

Status: **needs live run**. **S:** headless `--tactic-trace` snapshots include
assignment, mode, follow, destinations, command selection, all ten groups, side
cash/counts/queues and the task 73 shared visibility gate before each tick's
commands. An invented two-side battle proves trace repeatability and unchanged
per-tick hashes. `tactic_measure` validates full audited captures and rejects
incomplete fields/ticks, mismatched setup/settings/packet fingerprints and
nonidentical original repeats before reporting a clone divergence. Existing
`engine_unit` group/follow/mode, population-above-110 and map Difficulty 0/100
checks remain synthetic engine controls.

**P:** `tests/parity/tactics.json` supplies 21 cases across four numeric modes,
with separate global difficulty enumeration, population boundaries, bounds/
assignment/reveal-phase sweeps, group/follow packets and fog selection controls.
The task 73 consumer is present; the V02/task 64 original packet/readback,
tactic graph/bounds/scan, selection/picking and global-settings adapters are
not complete. The headless order-file route only supports move/attack-move;
semantic recipes do not imply executable paired scripts. Engine traces require
an audited identity/field adapter and cannot be relabelled as original captures.

No live original execution was attempted under the sandbox restriction. No
nearest/lower-ID/four-tick acquisition, centroid/six-tick follow, distance,
population or difficulty policy was changed. Inspection notes do not satisfy
this task's requirement for two original measurements. No bonus, cap or scan
formula was invented. See the [run sheet](EXPERIMENTS.md#v06v10-groups-tactics-and-difficulty-run-sheet-task-90-needs-live-run)
and [capture contract](../tests/parity/README.md#v06v10-tactics-comparison-task-90).

Verification: Release configure/build passed; all 48 runnable CTest entries
passed, with six private-data/network checks skipped. Full pre-commit plus
explicit checks of new untracked files passed. A missing-capture CLI control
returned 77 with `needs live run`. No private game data or original runtime was
used for these task 90 checks.
## V08/V09 synthetic comparison follow-up (2026-11-06)

**S:** `engine_unit` compares every saved continuation tick across resource-clock
wrap/global pause for farm/mine/hall, two-town training/spawn, service destruction
and all-race hall upgrades including precompletion build-tree availability.
Existing suites retain construction pause/damage/refund, activation, town-local
service/protection and non-map recipe coverage. `town_compare` uses invented
captures to test first differing tick/field across economy/service classes,
ordered events, repeated-reference instability and fail-closed late validation.
No engine behavior changed; parallel construction remains default off.

**P / needs live run:** `tests/parity/town.json` adds five ztest_all case families
and control variants alongside tasks 66/67. The town observation contract and
[run sheet](EXPERIMENTS.md#economy-and-town-service-run-sheet-needs-live-run)
are ready for the orchestrator. Task 64 audited inputs/semantic observers, task 66
placement acceptance and audited reference save/load observations remain missing.
No reference was launched here; no new M evidence, measured engine fix or parity
claim follows from the synthetic checks. Worker travel, stages/timing, original
payment/refunds, sound/selection/rally, training/spawn, service/protection/HP and
hall/tree transitions through saves all still require two repeated live runs.

## Task 151: economy creation, completion and lane gates (2026-11-21)

**V reference contracts / synthetic implementation:** paid building work now starts
from zero HP and advances actual HP for automatic structures and in-place hall
upgrades as well as explicit placements. Units retain integer work accumulation.
The existing first-start-tick increment, no-builder/no-tick-drain path, 8.8 refund
of the actual payment, unpaid waiting cancellation, serial town lanes and absence
of a population gate remain covered. Missing prerequisites now allow admission
while leaving the head unpaid and blocking the lane until completed prerequisites
exist. Disabled entries, unique reservations and waiting capacity still apply.

Gold samples terrain and initializes its clock at completion, before the first
active update; existing completed mission structures initialize at mission start.
Repair initializes its own rate/clock at creation/admission and retains that age
through waiting and construction; only active updates accumulate credits. Disabled
income accumulators remain empty. Existing descriptor dates, rounding, warehouse
scope/non-stacking and day-wrap tests are retained. New synthetic checks isolate
these boundaries, automatic-building damage/refunds and upgrade cancellation.
Queue creation clocks and previous hall types participate in state hashes/replay.

**G / observation boundary:** automatic placement and failed-site policy, waiting
one-HP placeholders, hall HP/clock restoration on cancellation, completion versus
active-object dispatch ordering and shared-cash town arbitration still require
native observation. Optional parallel modifications remain clone settings. The
static formulas are not a live parity claim. Task-64 observer and task-97 aligned
production/income measurement dependencies remain open; no original executable
or reference runtime was run. See [implementation and measurement list](economy_implementation.md).

## Task 150: original building placement predicate (2026-11-21)

**V static reference / implemented:** radius 8 with strict squared distance for
all value-2 mask cells, ignored values 0/1, buildable layer 0, empty object tiles,
four orthogonal building neighbours and a side byte greater than 32. Required
income channels come from non-defence d60/d61, with an OR across channels and body
cells and a nonzero threshold. The preview and click use one shared query.
Admission and production start do not re-run it; admission reserves without
payment. Replay semantics version is 14. See [placement_rules.md](placement_rules.md)
for addresses, detailed tests and the optional helper policy.

**G / outstanding:** mapping side +0x690 to engine brightness, exact original
object-grid membership, missing-mask fallback, anchor guards, hall prerequisites,
reservation representation, AI site ranking and diagonal planner gaps. The audited
observer and live placement captures remain missing dependencies; no reference
process was run and no live placement or whole-game parity is claimed. Synthetic
regressions use invented maps/descriptors and software-renderer pixels. Older task
47/101 statements about radius 16, clearance masks, mineral terrain exceptions
and waiting-site revalidation are superseded by this update.
## RNG and tick order (task 158)

**V reference facts / engine construction:** seed-zero MSVC sequence, independent
simulation state, shared supported combat/decorative/planner/scene sound draws,
newest-first logical unit pools with LIFO reuse, effects before scripts,
projectiles after units and global time at completion. Scene sound metadata is
loaded in both runners regardless of playback, and selected sound/pitch survives
visibility and mixer filtering. Replay semantics are 14; standalone logs are
AZC2. The [implementation and full random-use inventory](rng_and_tick_order_implementation.md)
separate these properties from consumption parity.

**G / incomplete:** wind/loading draws, descriptor-only sounds, game-thread input
speech mapping, native constructor/scatter/AI consumption, source-order script
creation mapping, interleaved town callbacks, sequential movement and per-object
sound emission, original allocation addresses and original-save RNG state. No
reference runtime or original executable was launched; no live parity claim is
made. These missing producer/observer dependencies do not prevent the independent
generator and deterministic replay changes.

Validation: Release build and all 79 CTest entries complete with 73 passes and
six expected skips; five focused ASan/UBSan checks pass. Pre-commit covers tracked
and new files. Private-copy `formats`/`engine` checks pass with dummy SDL inside
`unshare -rn`; repeated mission-1 clone hashes agree through tick 120, including
all 61 checkpoints after save-at-60/reload. These are engine checks, not original
measurements; private artifacts stay under ignored `work/rng_tick_validation/`.

## Capture and facility service (2026-11-21)

**V reference facts:** the newer [special mechanics evidence](special_mechanics_residual.md)
corrects the older ownership summary: ordinary buildings do not defend a town;
units and Defence objects do. Candidate distance is squared tile distance <5,
defence radius is 4/8 tiles, and capture has no hold timer. Buildings/towers
change side; existing town units join an old-side receiving group. Waiting work
is discarded. Active buildings continue with their progress except in the
computer-to-human cancellation/refund branch; active trainees are discarded.
Refunds go only to the old computer owner in that branch. No cash is transferred.

**V engine construction checks:** automatic capture now preserves old-side units,
continues active building work, discards trainees/waiting placeholders and gates
refunds by owner control type. Tests cover all four human/computer combinations,
continued completion without a second payment, selection/group cleanup,
speed/repair resets, allied versus own-side defender scans and existing
candidate/Defence/airborne/radius boundaries. Script gifts retain their existing
separate policy and tests.

Workshops service machines, including boats, Johnka, ordinary buildings and
Defence objects. Stables service the six non-soldier Marine animal types. Air
facilities participate only above 256 objects and require a workshop or stable.
Ordinary service refreshes the regeneration window to 10; Building, Defence and
Headtruck overrides instead add capped +10 HP. This is separate from paid repair.
Synthetic tests cover all three races' facilities, category and allied/dead
eligibility, completion, both range branches, object counts 256/257, instant
caps, countdown expiry and replay. Existing explosion 152, debris 113 and smoke
105 sprite metadata is retained and regression-tested. Debris has 20 one-frame
variants, not a twenty-frame animation; smoke has one thirty-frame sequence.

**G implementation limits:** insertion-order scan winners, numeric receiving
groups, implicit standalone-object tactics, nonremoved-object pool counts,
large-pool inclusive pixel boxes and town/object update interleaving stand in
for the full reference model. Continuing work keeps its frozen rate and payment
metadata. Modifications can expose several simultaneous active lanes, to which
the same capture policy is applied. Computer-town alarm cancellation before
capture, exact pointer/tactic lifetimes, rally/parent resets and capture
messages/voices/pings remain incomplete. The audited reference adapter and
aligned town/service observations remain missing dependencies. Independent
corrections and synthetic checks were completed without them; no reference
runtime or original executable was run and no original parity is claimed.

See [capture_and_service_implementation.md](capture_and_service_implementation.md)
for V/G boundaries, the comparison with prior engine behavior and test coverage.
Baseline Release build, 73 runnable CTest checks (six skipped) and pre-commit
passed. A final clone-only mission 1 smoke completed 120 ticks using a private
installation copy, dummy SDL drivers and `unshare -rn`; its logs/report remain in
ignored `work/capture-service/`. Final Release build, all 73 runnable CTest
entries (six skipped), all-file pre-commit and explicit hooks for the new document
passed. Separate `formats` and `engine` game-data checks passed with that private
copy under dummy SDL and `unshare -rn`. The bounded smoke reached its requested
120-tick limit with no script errors; it did not complete the mission. No private
assets, executable-derived code or probe artifacts are included in the changes.


## Ferry motion and submarine decoration (2026-11-21)

Task 159 supersedes the earlier ferry route/arrival policies in task 129.
**V reference rules, implemented and checked synthetically:** independent boat
states 0..6 and ferry phases 0..5; five pixels per axis on the NE-SW diagonal;
no water route or stored home dock; stop/snap when the next diagonal tile is
Small-walkable; turns toggle direction only at animation completion; boarding
scans the 3 by 3 block and limits newly boarded passengers to five. Preloaded
passengers are hidden regardless of editor metadata and do not increment that
boarding count. Landing uses square tile rings and a fresh unselected mode-3
group on the stored side. Requester death/cancellation preserves cargo; boat
loss kills cargo and cancels followers. Script activation overrides any phase
without turning. Submarines have a looping idle decoration replaced by one
finite animation on go, then no visible object.

**G / unresolved:** centroid group anchors, shore routing and continuous occupancy,
per-unit clearance, bounded blocked-landing retries, fallback side after a repeated
activation, native click behavior for enemy boats and the retained synthetic
combat policy. Finite resource durations are used when loaded; absent-resource
fallbacks remain eight ticks for ramps and 28 for turns/submarine replacement.
The historical samples do not prove exact native within-tick scheduling. See
[ferry_implementation.md](ferry_implementation.md) for the full mapping, regression
coverage and evidence limits. No original executable was run or imported.

Validation (2026-11-22): Release build, all 79 CTest entries (73 passes, six expected
skips), and pre-commit passed. Separate private-data format/engine checks and all
19 campaign transport dispatch sites passed inside `unshare -rn` with dummy SDL
and a private installation copy. A clone-only mission-2 headless smoke reached
720 ticks. Private logs remain ignored; these checks establish no live reference
parity.

PR 285 integration repair (2026-11-22): the separate integration tree combines
ferry work with develop `41e0f56` (including tasks 161/162), preserving submarine
pool retirement and all ferry tests. The merged Linux Release suite has 82 entries
(76 passes, six expected skips). Fixture profile setup now uses the executable's
C runtime on Windows; drawable-size checks support HiDPI. Fresh native CI remains
unverified because connector approval policy blocks publishing. See the repair
notes in [ferry_implementation.md](ferry_implementation.md).


## Campaign flow rules update (2026-11-22)

V static reference rules / synthetic engine construction: campaign entries stay
in numeric order, show Mission/status labels and omit missions above the current
progress. Progress starts at 1 and advances on victory to
min(16,max(mission+1,progress)); defeat and earlier replays cannot reduce it.
The engine record uses version 2 scalar storage and migrates old completion
masks without writing original profiles. Malformed records and failed writes
preserve existing data. Updates now occur at mission exit, once per attempt.

Campaign wins below the final threshold and campaign losses no longer play
custom win/lose movies. Final-threshold wins select final.avi; explicit custom
worlds select win.avi or lose.avi. Fresh campaign launches alone play the mission
AVI. Objectives use the third .mit section; editor Notes are not substituted.
Existing script outcome latching and local-side propagation are retained, with
new no-script/empty-world and conflicting-outcome regressions.

G: original result UI, automatic termination and the visible return chain remain
unresolved. The current frozen result overlay, Retry and next-briefing shortcut
are engine policies. V known / G missing implementation: named original-style
profiles, separate campaign/custom roots and launcher identity, and imported-save
campaign progression still depend on task 77 and task 94/V16 work absent here.
See [campaign flow implementation](campaign_flow_implementation.md) for the full
V/G comparison, persistence migration, return paths and dependency boundaries.

Release build, all 82 CTest entries (76 pass, six skip) and pre-commit pass.
Tests use invented data only, including the actual mission-exit persistence path;
no original executable/reference runtime was run. These checks establish neither
real campaign reachability nor original UI, profile or save-continuation parity.
## Original in-game layout mode (2026-11-22)

V reference facts from interface_geometry.md, interface_ingame.md and
worldview_mouse_cursors.md now drive an opt-in `--layout original`; modern
remains the default. Shared draw/hit geometry covers the L-shaped panel,
180x180 minimap box and integer map scale, squad stance row (2/1/3/0), town
service/current-item row, 4x3 member cells, 3x3 build cells and three queue cells.
Original mode uses the documented cursor precedence/indices, strict >16-pixel
box threshold, rectangle overlap, one-unit Shift-click, Inv digit assignment
without modern Ctrl+Shift append, and right-release orders after a drag.
Money/message anchors and tooltip placement follow the documented geometry;
engine debug HUD starts hidden in original mode.

G: other resolutions use a documented integer-scaled/bottom-anchored adaptation.
Native object rectangles and bucket picking, joined subteams, persistent Ctrl
selection detachment, filter callers and raw-left-Shift digit routing remain
missing. Box rectangles use an explicit 48x48 proxy unless supplied by a caller.
List ordering/paging, button state art, fonts, money smoothing/coin/emblem,
minimap colors/cadence, selection art and outcome/overlay integration remain
partial. Existing simulation packet adapters do not establish native payload
compatibility. No missing dependency is assumed integrated. The complete V/G
comparison and outstanding values are in
[interface_implementation.md](interface_implementation.md).

Synthetic `interface_layout` checks geometry, half-open boundaries, gaps,
minimap scaling, selection/modifier behavior, cursor precedence and submitted
commands. Existing modern layout tests remain applicable. No original executable
or reference runtime was launched; synthetic success is not visual parity.

Validation: final Release build and 83 CTest entries completed with zero failures
(77 passed, six skips); pre-commit passed for tracked and new files. The two
private format/engine checks and a three-frame original-layout clone capture/trace
also passed with dummy SDL under `unshare -rn` using a private installation copy.
These are clone checks, not a reference comparison.

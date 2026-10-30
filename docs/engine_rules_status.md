# Engine: status of the simulation rules (verified / measured / placeholder)

Simulation: `src/engine/sim/game_sim.*` (12 Hz, fixed-point harvests and original float32 price factors, driven by the command queue in `commands.hpp`). Data comes from `docs/data/*.csv`, loaded at run time (`rules.*`). The reference specification is [rules_summary.md](rules_summary.md) with details in [rules_types.md](rules_types.md) and [rules_ai.md](rules_ai.md). New reference findings below do not imply engine implementation changes.
Movement and pathfinding: `src/engine/sim/path/` (movement agents; the interface is swappable: `GameSim` only uses `MoveSystem`).
Legend: **V** = verified against the original (code or screenshot), **M** = measured on the recompiled original, **P** = placeholder (invented, to be replaced).

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
| Command order inside a tick: ascending side, then submission order | P | original: per-side queues in `0x4577b0` (order not verified) |
| Descriptor primary/max and secondary HP | V fields / engine checks | All nine descriptor families loaded; mission/spawn maxima never depend on other instances. Positive mission HP is preserved. Original-save +0x3c is secondary HP; maximum comes from rules at simulation initialization. See V04 below |
| Descriptor regeneration | V inspected thresholds/countdown/overshoot / P facility producer | Marine, Truck, Zeppelin, Bird, Headtruck, Spider and Cannon recover below natural thresholds, or full HP while the hashed service countdown is positive. Pre-add check permits overshoot; countdown expires after the update. Facility coverage/refresh and exact tick alignment remain open |
| Scripted object health and placement identity | V zero maximum getters / engine identity checks | Boat, Johnka and Camera maximum getters return zero; Submarine remains an unsupported marker. Town membership uses section numbers, including sparse IDs; team scenario/tactic, defence and vessel provenance remain distinct. Live reference identity still depends on V02 |
| Descriptor movement speed | V getters / synthetic integration | Marine d5, Headtruck/Spider d7, Zeppelin/Truck d5 in px/tick, converted to 24.8. Historical averages no longer override established descriptor speeds |
| Measured speed fallback | M | `speed_measured.csv` (mean mid-run speed), used only without a verified speed getter; these averages include steering/sampling effects |
| Bird speed and transport routes | P / unresolved | Bird and unknown speeds default to 8 px/tick. Boats and Johnka remain stationary; neither receives a ground agent. Scripted transport routes are unimplemented |
| Unit radius (collision disc) | P | 14 px, large 20 px |
| Movement class small / large (`.dat` 1 = small only) | V code facts / synthetic integration | Marine d11 low byte selects small when nonzero; Spider/Truck/Headtruck use large. Bear is small; Knight/Bizon/Snowman/Elefant large. Missing Marine descriptors retain an explicitly provisional small fallback |
| Air orders and ground collisions | engine / P reference integration | Aircraft use straight fixed-point movement without terrain/contact blocking, including engine attack-move orders. Ground agents retain continuous disc avoidance/pushing and flow fields. Reference tile reservations, vehicle turning, formation/path decisions and exact arrival timing remain unimplemented or unmeasured; see [V05 progress](rules_movement.md#8-v05-partial-integration-2026-10-28) |
| Price and build time | V formula / synthetic checks | `types.csv` with outpost/developed hall, runtime speed, hostile-to-everyone and computer branches. Float32 factors evaluated before truncation after +0.5; developed fast computer 100 -> 160 gold / 53 time units. No lifted 161 rounding |
| Production countdown | engine / O parity | Computed time units drive tick countdowns; one payment at acceptance, none at spawn. Exact original start/completion tick ordering is unverified |
| Build tree (who can build what, prerequisites) | V | `buildtree.csv`; ordinary prerequisites require completed objects in the producing town, A entries use side-wide counts. Only map-construction entries reject completed/in-progress/queued copies in that town; flag-0 resource structures and defences are repeatable. Hall upgrades retain their separate reservation guard; DisabledID and adjusted affordability are checked |
| Town upgrades and non-map structure production | engine / V tables, P placement | Flag-0 structures use adjusted costs/timers and the production queue. Hall completion preserves identity and installs its race/level tree; farms/mines/defences can be queued repeatedly, are automatically placed on free tiles and activate descriptor effects. Exact reference placement, HP transition and completion ticks remain unverified; see validation below |
| Side `DisabledID` blocks menu entries | V | `docs/formats.md` |
| Weapon damage | V documented arithmetic / engine synthetic checks | Marine d7 and Defence d59 select weapon IDs. Classes 1/2/13 apply each matching 8.8 multiplier in sequence, then armor term; arrow zero-air suppresses armor. Classes 4/5/6/10 constants implemented; 8 delegates and 12 sums known child damage. Historical duel entries are diagnostics only. Other descriptor weapon mappings/armor retain placeholders |
| Weapon reload getters and cooldown | V Marine reload+1 / P other scheduling | Classes 1/2/13/4/5/6/10 use documented reload/range. Simulation schedules reload+1; this is verified only for Marine cooldown, not universal hit intervals. Composite scheduling remains unresolved |
| Projectile and unresolved weapon behavior | P / unresolved | Projectile speed is distinct metadata, not added to reload. Descriptor damage feeds provisional straight projectile flight and impact timing; direct weapons hit immediately. Reference flight/animation timing remains unverified. Classes 0/3/7/9/11, complete burn behavior and composite scheduling are unavailable calculations; known unresolved descriptor weapons do not silently fire generic damage |
| Weapon range | V getters / P geometry and fallback | Established weapon getters replace generic ranges; Euclidean centre distance plus mobile source radius and placeholder target radius/footprint remains. Types without loaded weapon descriptors retain 56/288/336 px placeholders |
| Sight range (target acquisition, fog) | P | Rewrite: 288 px (6 tiles), buildings 8 tiles. Reference: per-type interpolated light-mask IDs, daylight and brightness >32; separate tactic scan and weapon checks |
| Auto-attack of enemies in sight (idle and attack-moving units) | P | nearest enemy, ties = lower id; hostility from the relation matrix |
| Weapon range | V getters / P geometry and fallback | Established weapon getters replace generic ranges; Euclidean centre distance plus placeholder target radius/footprint remains. Types without loaded weapon descriptors retain 56/288/336 px placeholders |
| Sight data and visibility predicate | V facts / synthetic checks | Descriptor mask IDs replace the scalar radius. Per-side interpolated brightness must exceed 32; daylight and frame glow contribute. User lighting resources enable these rules; asset-free acquisition retains a P six-tile fallback. See the [sight and fog notes](sight_fog.md) |
| Auto-attack of visible enemies (idle and attack-moving units) | V visibility gate / P tactic integration | Side brightness >32, hostility and known weapon eligibility gate candidates. Nearest enemy, lower-id ties and four-tick per-object scanning remain placeholders; original tactic bounds, 100-entry arrays and 15..23-tick scans are not implemented |
| Death and corpses | P / V effect metadata | Marine corpses remain 480 ticks; vehicles spawn 152 and ground debris 113, known-radius structures select rubble 116/115/114 for 250 ticks; unknown radii retain provisional rubble 116. Airborne altitude remains open |
| Simulation fog brightness and exploration | V formulas / synthetic checks | Every active tick: global daylight plus saturated glow (maximum 152), minimum side vision darkness, nonnegative difference; previous exploration capped at 32. Raw brightness replaces invented 0/96/152 states. Renderer integration remains separate |
| Building construction: placement on free tiles, progress = adjusted build time, stages = animation slot 0 | V formula/stages, engine placement | Hall/speed/computer time applies to placed construction. Original placement/worker and exact completion semantics remain open |
| Building footprint in tiles | P | desc d4/d5 (px) rounded up, centred on the clicked tile |
| Town production: construction and units share one active item per town | V inspected control flow / engine regression, P exact timing and spawn | FIFO waiting orders are unpaid; payment and speed/time selection happen at start. Capacity is 50 waiting items plus active work. Active pause is town-local; cancellation refunds unbuilt paid work. Missing/incomplete halls suspend timers. See task 36 below for evidence and limits |
| Building income and repair supply | V formulas / synthetic checks | Descriptor occupancy cells equal to 2 weight resource channels A/B with d60/d61; periodic/scheduled 7200-tick clock from StartTime. Gold and d62/d320 repair supplies have separate accumulators. Completed same-town warehouse applies once to farms only. Missing resource map contributes zero terrain bytes |
| Starting money = side `StartMoney` | V | `.mp` |
| Garrison | engine / V semantic fact | Extracts live mobile town members into a fresh tactic and selects it. Explicit mission town membership and hall identity, invalid-owner checks, empty/repeated extraction, hash and save/replay checks. Reference packet field layout and live comparison remain unverified |
| Repair/destroy mode and list commands | engine / partial | Modes 0/1/2; mode changes clear lists, same-mode commands preserve them. Eligible completed town structures toggle membership; halls, mobile units, foreign-town targets and construction are rejected. Off mode collects damaged ordinary buildings. Dead, repaired or reassigned targets are pruned. Commands do not delete or change HP. Harvested credits now drive priority-group repair/destruction and are cleared after service; exact tick alignment remains unverified |
| Attack modes | V numeric modes / partial engine | Direct 0, defensive 1, aggressive 2, balanced 3 (default). Command, ownership/tactic propagation, direct range-only engagement, movement/follow gates and replay/hash state implemented. Group distance limits, attached-tactic graph and full assignment remain pending; see investigation below |
| Selection and follow commands | partial engine / P formation | Deterministic control-group recall and side-local tactic selection, persistent ground/air follow, ownership/cycle checks and save continuation; engine payload conventions and centroid tracking are not reference packet/formation parity. See V10 update |
| Universal population cap | O reference / engine correction | Task 13 removes the unsupported AI-only cap 80 in develop. Synthetic human/AI production above 110 units passes. Queue 50 and assignment arrays 100 do not establish a side cap; no live population measurement succeeded |
| Difficulty bonuses | V map-key exclusion / O other effects | Map Difficulty stays editor metadata; synthetic 0/100 inputs produce identical simulation hashes. No HP, damage, income or price bonus is invented. Global difficulty effects remain unverified |

## Projectiles and effects

| Rule | Status | Source / notes |
|---|---|---|
| Projectile objects at 12 Hz, integer fixed-point flight, previous/current render interpolation, hash/replay coverage | engine | Arrow, Axe, BallExpl and FireThrow objects carry source/target ids, snapshot endpoint, speed, payload and animation age. No renderer writes simulation state |
| Type-to-weapon dispatch | V Marine/Defence / P Spider/Headtruck | Marine d7, Defence d59, Spider/Headtruck d9; CSV class/parameter lookup. Composite class 12 retains both visual components but cannot fire until scheduling and damage are known; class 8 delegates its visual to its child. Composite damage provisionally splits the existing total equally (integer remainder to the first child). Bird/Truck/Cannon/Zeppelin weapon fields are not guessed |
| Arrow speed/reload/range | V table / P firing schedule | Arrow d1/d2/d3; descriptor reload+1 schedules shots, without duel overrides. Shots start at the combat event, not the animation fire marker. Flight timing parity remains open |
| Ball/Axe speed/reload/range | P | d1/d2/d3 interpreted as flight speed, reload and range; zero speed falls back to 1 px/tick. These getters/trajectory semantics need reference measurement |
| Flame and splash behavior | P | class 7 uses anim 157, 24 px/tick, range 96, reload 12. Child class 7 d0 selects 150/151; d6 is provisionally a splash radius. Full burn sequencing, armor/category damage, friendly splash, falloff and secondary weapon damage are unresolved. Known descriptor damage is applied once at impact. Class 7 and other unresolved damage/scheduling remain disabled; visual defaults do not enable a shot |
| Projectile trajectory and collision | P | straight flight toward the launch-time target position; no gravity, homing, terrain collision or interception. The original target receives the hit even after moving; splash additionally hits living hostile objects inside the radius. Source death does not cancel flight |
| Reference trajectory, interception, burn and splash measurements | blocked / no measurements | The 2026-10-28 isolated probe failed before game launch: namespace loopback setup returned `Cannot open netlink socket: Operation not permitted`. This is not a no-hit observation. Ballistic parameters, terrain/obstacle tests, burn cadence and per-category splash damage remain unresolved; see the [measurement protocol](EXPERIMENTS.md#projectile-measurement-protocol). No simulation rules were changed from this failed probe |
| Effect animation ids, slots, durations and hotspots | V resource metadata | `effects.hpp`, `AnimSet`, `sprites_mapping.md`; effects read frame/light timelines from installed Units.xbe. 150/151/152 last 11 ticks in slot 0 and 10 in slots 1/2; debris/rubble 250; shockwave 105 lasts 30. Arrow/Axe/Ball use dir32 looping slots, flame uses a one-shot slot |
| Damage fire | V selection / engine descriptor HP inputs | built structures at <=75/50/25% HP use 154/155/156 at ground position +(0,1), looping; repair/death removes the fire. Descriptor maxima are supplied by V04; synchronized reference rendering remains open |
| Explosions, debris, muzzle and beam | V selection metadata / P spawn timing | vehicle death 152, ground debris 113; Arrow d13=105 creates shockwave; immediate descriptor d10/d11 enables muzzle 106 at +20 px, beam 107 at approximately 30-px steps, impact 110. No extra turret muzzle sprite is invented |
| Smoke and unknown owners | open | smoke contained in known animation frames renders through the same effect path. Standalone anims 104/153 remain unassigned: their owners/spawn conditions are unverified, so no smoke spawning rule is invented |
| Height and displacement | P flight altitude | Axe uses frame height, debris/rubble height -1; vehicle explosions retain explicit render height; other transient heights default to zero. Ball light reduction with altitude, altitude production and per-frame simulation displacement remain unresolved |
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
only in-range targets and never issues pursuit movement. Defensive mode engages
while idle and preserves movement orders; aggressive/balanced modes can interrupt
movement and resume it after target death. Explicit attack orders remain available.
The range geometry, four-tick acquisition cadence and nearest-enemy policy remain
existing approximations. This is a useful partial integration, not full tactic
parity: aggressive/balanced currently share that policy. Attached-tactic graphs,
group centres, mode-specific group distance enforcement, formation return and
mode inheritance across later tactic joins/spawns still require the tactic model.

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
`side(local)` means the mission's human side (`World::script_side`: the first side that is neither `OnlyComp` nor neutral), identical for every peer; the viewing side (`World::local_side`) never influences the simulation.

| Rule | Status | Notes |
|---|---|---|
| Every trigger is evaluated each tick; actions run when all conditions hold; `trigger(N).disable` turns one off | P | inferred from usage (`attacked != -1` plus self-disable) |
| Triggers enabled by another trigger start disabled; initialised in `GameSim::init()` (not tied to tick 1) | P | initial state undocumented |
| `localTime` / `globalTime` = ticks since mission start | P | |
| Timers: start at 0 and count every tick; `set(V)` sets and starts, `stop`, `kill`; `timer(0)` exists from the start | engine synthetic / P reference | Timer 0 is initialized before tick 1; stop/start retains values, kill suppresses comparisons. Fixed-width 64-bit timer values; see [campaign audit](campaign_scripts.md) |
| `side(S).units` = living non-buildings; `towns` = living town halls (outpost / village / town / fortress types); `enemies` = units and town halls of hostile sides | P | |
| `tactic(N)` = living objects of the side with `[Team#] ScenarioID = N`; `valid` 0/1; `idle` = all idle; `attacked` = id of the last attacker or -1 | P | |
| `attackTown(id)`, `moveTo(x,y)` = attack-move of the whole group (tiles -> pixels, clamped to the map) | P | |
| `joinTactic`, `giftToSide`, `kill`, `lock/unlock`, `setPath/setCycledPath/...` (successive points of `[Paths]`) | P | |
| `setFriendlyTo/NeutralTo/EnemyTo` changes the relation one-way (8x8 matrix) | P | |
| `addMoney/takeMoney`, `printString`, `centerView`, `cameraActivate(i,v)` (jump to `Camera#`), `victory/defeat` | P | the mission ends for the script side |
| `boat(N).activate`, `johnka(N).enable/disable`, `submarine(N).go`, `buildCommand`, `moveCommand`, `cameraDeactivate` | not done | Shared runner/headless/inventory classification; 19 unsupported campaign sites in missions 2, 7, 9 and 15. Build/move/deactivate and Johnka disable are absent from the audited campaign |
| Defences from `[SideN] Defence#` appear as objects (towers); `defence(N).valid` = alive | engine identity checks / P reference | Explicit defence provenance excludes colliding team scenario IDs and accidental town membership. Dedicated reference tactic behavior remains unimplemented. Camera records are separate; see `rules_ai.md` |

V12 progress (2026-10-28): the [campaign script audit](campaign_scripts.md)
records every condition/action kind across 15 private campaign scripts (92
triggers, 129 conditions, 363 action sites). All parse; 19 action sites remain
unsupported. `script_unit` covers every condition kind, enablement/source order,
timer lifecycle, path variants, alliances, gifts, defence identity, outcomes and
replay continuation with synthetic data. Gifting now stops the movement agent
and rejects invalid recipients; defence provenance and per-side outcomes are
hashed. These are engine consistency fixes, not measured reference semantics.
The isolated reference control failed during namespace network setup before
launch. V02/V05/V10/V11 adapters and mission objective validation remain open;
V12 is not complete. Camera activation still ignores its second argument.

## Computer player (`ai/planner.*`, `sim/ai.cpp` adapter)
Controlled sides have `OnlyComp=1` and are not neutral. The pure planner reads
world state and validated purchase/placement offers, returning its next state and
commands. It does not read the viewing side, wall clock or global RNG. Internal
commands are regenerated during replay rather than added to the player command log.

| Rule | Status | Notes |
|---|---|---|
| Town states 0..23, science timer, production suspension, ordinary-building development sum clamped to 9, normal/fast spending comparison | V control flow / synthetic checks | Active construction or a town's queued production suspends transitions; science and speed continue. State 0 has no shared side-plan arbitration yet. Requested-type overrides and autonomous hall-upgrade selection remain open; hall-upgrade commands are supported |
| Income and spending estimates | P estimate / V comparison | Actual cumulative harvest and purchase spending are tracked per town; average harvest substitutes for the unresolved instantaneous rate estimate. Starting cash is divided among the side's towns. Speed is normal below twice spending, fast otherwise |
| Economic requests | V preference / P placement | More than two farms and no warehouse prefers an available warehouse, otherwise farm, then mine. Uses existing resource-map harvests; no passive AI grant or worker-gather command is invented |
| Development and production composition | P (G in reference rules) | Cheapest available combat/development item with UGI tie break. Existing prerequisites, disabled types, prices and affordability apply. Does not reproduce recursive scoring or randomized ties |
| Intelligence | V discovery/cadence / P visibility | Records visible live towns, own ordinary buildings and visible foreign membership including nearby towns within eight tiles on both axes. Town discovery refreshes on first use and every `(side+1)*1000` updates. Previously discovered living enemy towns remain valid destinations through fog. Computer layers initialize at 24; uses engine visibility values <=24; exact MapExplorer weighting and visibility parity remain open |
| Attack waves | P (G fallback) | States 1/2 prepare purchases and release idle autonomous combat units toward a known hostile non-outpost town. Local and threatened force targets use documented `3*(development+1)` and `7*(development+1)` counts. Offensive purchase count is a fallback, not a verified launch threshold |
| Defence reactions | P (G fallback) | A new live visible hostile attacker of a town structure redirects eligible autonomous town troops, including moving troops. Idle reinforcements also respond to the ongoing alert while production is busy, without resetting active orders or the production plan. Eight-tile threat bounds and plan entry are explicit fallback choices. Script-owned tactics are preserved. Placement failure disables defence attempts; unaffordability does not |
| Placement | P (G fallback) | Stable radius/row/column search within eight tiles, weighted resource bytes for terrain harvesters. Failed placement produces no purchase, payment or reservation. Map construction uses placement offers; flag-0 farms/mines/defences use queued automatic placement, while hall upgrades replace the existing hall |
| Population and tactic limits | open | Removed the invented 80-unit cap. No difficulty bonuses or universal population limit. No reference kill-time assignment, 15..23-tick tactic scan or full side-response arbitration is claimed |
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
reference payment/refund semantics remain incomplete. V02's audited object
identities and aligned normalized AI observers are absent, although packet
validation and comparison tooling exist. Town-command and hall-upgrade support is
present; autonomous hall-upgrade selection, exact economic queue semantics, tactic
assignment and reference timing parity remain open. Existing unsupported boat/johnka/submarine, build/move script
actions and camera semantics remain listed in the mission-script section.

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
| Rule | Status | Notes |
|---|---|---|
| Effects from `Snd.xbe`: sound set (`sound_set`) from field [2] of an animation frame, played on the first tick of the frame, random member of the set | V (format), P (random choice uses a local LCG, not the simulation RNG) | |
| Pan = (x - view centre x) clamped to +-4000 hundredths of dB; attenuation: none up to 500 px, linear to -4000 at 800 px, silent beyond | V (`docs/formats.md`), P (100 units = 1 dB) | Scene dispatcher uses the active view visibility predicate; exact reference fog semantics remain open |
| Music: `Music=N` -> track N, otherwise `MusicMask`; next track when one ends | V (format and ascending cyclic order), V construction / synthetic checks | `music_allowed_mask` in `core/audio`; no simulation RNG or mission-name salt |
| Animation state loops, building/projectile/effect sounds | V construction / synthetic checks; simulation timing remains P where unresolved | Header loops and frame entries use a read-only scene snapshot; visibility/distance/load/exit cleanup checked in `audio_events` |
| Independent effects/music/speech gain, acknowledgement suppression and speech WAV/caption API | V construction / synthetic checks, P replacement/duration policy | Separate speech gain, UTF-8 captions, missing-WAV/muted/no-device fallback; six presentation seconds, outside World/hash/replay. Selection/order/announcement producers still need evidenced IDs; see [audio evidence](audio.md) |

## Saves
| Rule | Status | Notes |
|---|---|---|
| Engine saves: versioned AZS3, bounded transactional parsing, explicit little-endian encoding, content/script identity, atomic slot replacement and refusal of replay hash mismatch before replacing the active world | V construction / synthetic tests | `engine_save` and `engine_save_processes`: malformed inputs, repaired-checksum fuzzing, write-failure preservation, fresh-process 14,400-tick save plus 300 matching continuation ticks, camera metadata and script victory. Ctrl+S / Ctrl+L remain mission-local. [Format, limits and remaining V16 acceptance](engine_saves.md); AZS2 is refused. V03 full-state audit and native Windows/campaign continuation remain unverified |
| Original `*.sav` format | not done | waiting for the savegame documentation |

## Resource and production validation (2026-10-26)

**V specification / synthetic implementation checks:** `test_engine --pure` uses
invented maps and objects plus public numeric descriptor tables. It checks an
asymmetric mine mask (16 gold/100 active ticks), a channel-A-only Rus farm
(363 gold at clock 1000), the once-only warehouse payout (484), mixed A/B
rounding (367), repair credits without cash, StartTime 1200 partial cycles,
periodic wraparound, Chinese-style date-zero cursor reset, local versus side-wide
prerequisites, duplicate structures, insufficient funds, speed-dependent costs,
single payment at spawn, cancellation and serialized command replay over a day.
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

- Boat activation, johnka enable/disable, submarine dispatch, side build/move
  script commands and camera deactivation reach unsupported action branches.
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
at acceptance, with one payment and a frozen countdown. Queue reservations block
competing hall upgrades in the same town. Only map-construction entries reject
duplicate structures; queued farms, mines and defences allow multiple copies. DisabledID,
affordability and local/side-wide prerequisites still apply. Completion replaces
the owned live hall in place, preserving its ID, town and selection references,
updates its descriptor HP and navigation footprint, resets its harvest clocks,
and installs the race/level build tree. No generic troop-stat bonus is applied.
Other flag-0 structures complete in their producing town and use the existing
resource-map income, warehouse, repair-supply and prerequisite rules. Town-member
command selection resolves mission section numbers independently of vector slots;
repair supply uses the corresponding town slot. The AI purchase skeleton continues
to select mobile units; autonomous development remains a separate task.

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
the host supplies glibc and drivers. Windows uses static SDL2/MSVC runtime linkage
and carries SDL2 source and license. Publication also includes the exact public
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
collection, clean-runner Linux package execution, static Windows linkage and
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
completion. The transition resets the phase age to zero. V12 still lacks Boat,
Johnka and submarine activation/travel adapters, so campaign scripts cannot yet
initiate these phases. Johnka retains its existing orientation loop.

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
profile, `Saves/engine_campaign.txt`, with a version and fifteen completion bits.
The campaign list marks completed missions without imposing unverified locks.
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
coverage/refresh is still absent: its small/large-pool category and spatial branches
need validation before a universal service radius is introduced.

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
reference comparison. V04 remains open for these dependencies, facility service,
Hypno behavior and campaign-wide live transitions.

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
traces across repeated runs. The six command buttons currently retain sprite
frame zero on press; the trace reports their actual hover/highlight/enabled state.
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
## L1 rule experiment preparation (2026-10-29, partial task 43)

**Construction / synthetic evidence:** 19 P scenario specifications cover siege
speed, damage/category controls, reload, range and projectile flight, mine build
time/price/income, sight, regeneration and serial/two-town production. They use
`docs/PARITY_TEST_PLAN.md` because task 42's harness is absent. Tasks 36/37/40
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


## Task 36: mixed construction and unit queues (2026-10-29)

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

**Limits:** the engine still uses countdown completion; the reference increments
structure HP or mobile training work. Exact start/completion ticks, damage during
construction, hall-upgrade HP transitions, automatic placement and spawn locations
remain unresolved. Refund quantization follows the inspected rule using the
engine's countdown progress; this is not live timing/HP parity. A completed item
leaves its successor to the next engine tick. Shared-cash town arbitration retains
stable storage order pending measurements. No normalized reference observer or
successful record/replay control is available in this checkout.

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

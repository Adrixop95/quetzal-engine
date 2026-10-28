# Engine: status of the simulation rules (verified / measured / placeholder)

Simulation: `src/engine/sim/game_sim.*` (12 Hz, fixed-point harvests and original float32 price factors, driven by the command queue in `commands.hpp`). Data comes from `docs/data/*.csv`, loaded at run time (`rules.*`). The reference specification is [rules_summary.md](rules_summary.md) with details in [rules_types.md](rules_types.md) and [rules_ai.md](rules_ai.md). New reference findings below do not imply engine implementation changes.
Movement and pathfinding: `src/engine/sim/path/` (movement agents; the interface is swappable: `GameSim` only uses `MoveSystem`).
Legend: **V** = verified against the original (code or screenshot), **M** = measured on the recompiled original, **P** = placeholder (invented, to be replaced).

## Core simulation
| Rule | Status | Source / notes |
|---|---|---|
| 12 Hz step, 24.8 fixed-point positions, state hash after every tick, replay from the command log | V (construction) | no comparison with the original yet; the engine hash differs from the recompilation's hash |
| State hash covers town race/level and objects (incl. secondary HP, regeneration selector, destinations, orders, construction timers, town membership/hall identity and harvest accumulators/cursors), sides (money, queues including paid costs/town ownership, groups, selected tactic, build pause), day clock, town speeds, repair credits, repairer modes/lists, spawn counter, transient projectiles/effects (flight endpoints, payloads and animation clocks), relations, per-town AI states/known towns/harvest and spending totals, script state and the full movement system (agents, orders, slots, steering caches) | engine | collection sizes are mixed in |
| Command types (PCT_FRAME_*) and their numbers | V | `docs/object_types.md`. Implemented: PAUSEON/OFF, CREATEGROUP, ADDTOGROUP, MOVE, ATTACK, BUILD, BUILDMODE, DESTROYHERE (list toggle), GARRISON, REPAIRMODE, QUEUEKILL, PAUSEBUILD. Logged only: SELECTGROUP, ATTACKMODE, SELECTBYNO, FOLLOWTEAM |
| Stop (0x50) and AttackMove (0x51) | P | engine extensions; the original has no such packets |
| Command coordinates / tiles from packets | engine | clamped into the map before any fixed-point conversion; invalid tiles are ignored |
| Command order inside a tick: ascending side, then submission order | P | original: per-side queues in `0x4577b0` (order not verified) |
| Marine primary/max HP = d1; separate secondary HP = d4 | V fields / engine replay | CSV loader, initial objects and trained spawns retain distinct values; primary mission HP is preserved when supplied. Hypno kill-time assignment is not implemented |
| Marine regeneration | V threshold facts / P state integration | Active living Marines add d2 when below selected d1/d3 threshold; no extrapolation to other kinds. Explicit hashed `regen_alternate` selector defaults to d1. The supplied specification does not identify the reference state transition selecting d3 or establish clamp/overshoot behavior; current pre-add threshold behavior is a construction choice pending reference evidence |
| Primary HP of other types | V descriptor fields / P elsewhere | Building d1, Defence d3, Bird/Zeppelin d1, Headtruck/Spider d3 loaded. Headtruck/Spider initial and max fields agree in current tables. Other types retain highest mission HP fallback |
| Marine speed = desc d5 px/tick | V | `desc_marine.csv`; overridden by `speed_measured.csv` when present (parsed with integer arithmetic, locale independent) |
| Measured speed (all types with a measurement) | M | `speed_measured.csv` (mean mid-run speed) |
| Speed of fliers, boats, unknown types | P | 8 px/tick; fliers move in straight lines, boats stand still |
| Unit radius (collision disc) | P | 14 px, large 20 px |
| Movement class small / large (`.dat` 1 = small only) | V (meaning of `.dat`), P (type assignment) | large: trucks, bear, elephant |
| Price and build time | V formula / synthetic checks | `types.csv` with outpost/developed hall, runtime speed, hostile-to-everyone and computer branches. Float32 factors evaluated before truncation after +0.5; developed fast computer 100 -> 160 gold / 53 time units. No lifted 161 rounding |
| Production countdown | engine / O parity | Computed time units drive tick countdowns; one payment at acceptance, none at spawn. Exact original start/completion tick ordering is unverified |
| Build tree (who can build what, prerequisites) | V | `buildtree.csv`; ordinary prerequisites require completed objects in the producing town, A entries use side-wide counts. Completed/in-progress/queued structures prevent duplicates in that town; DisabledID and adjusted affordability are checked |
| Town upgrades and non-map structure production | engine / V tables, P placement | Flag-0 structures use adjusted costs/timers and the production queue. Hall completion preserves identity and installs its race/level tree; farms/mines/defences are automatically placed on free tiles and activate descriptor effects. Exact reference placement, HP transition and completion ticks remain unverified; see validation below |
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
| Unit training: side queue (max 8), first item progresses, 100 % refund on cancel | P queue/refund policy | Items retain producing town and actual payment; cancellation refunds that payment even after speed changes. Spawn below that town hall. Independent simultaneous town queues remain unimplemented |
| Building income and repair supply | V formulas / synthetic checks | Descriptor occupancy cells equal to 2 weight resource channels A/B with d60/d61; periodic/scheduled 7200-tick clock from StartTime. Gold and d62/d320 repair supplies have separate accumulators. Completed same-town warehouse applies once to farms only. Missing resource map contributes zero terrain bytes |
| Starting money = side `StartMoney` | V | `.mp` |
| Garrison | engine / V semantic fact | Extracts live mobile town members into a fresh tactic and selects it. Explicit mission town membership and hall identity, invalid-owner checks, empty/repeated extraction, hash and save/replay checks. Reference packet field layout and live comparison remain unverified |
| Repair/destroy mode and list commands | engine / partial | Modes 0/1/2; mode changes clear lists, same-mode commands preserve them. Eligible completed town structures toggle membership; halls, mobile units, foreign-town targets and construction are rejected. Off mode collects damaged ordinary buildings. Dead, repaired or reassigned targets are pruned. Commands do not delete or change HP. Repair credits are harvested, but service allocation and destruction damage remain unimplemented |
| Attack modes | V numeric modes / partial engine | Direct 0, defensive 1, aggressive 2, balanced 3 (default). Command, ownership/tactic propagation, direct range-only engagement, movement gates and replay/hash state implemented. Group distance limits, attached-tactic graph and full assignment remain pending; see investigation below |
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
| Damage fire | V selection / P HP inputs | built structures at <=75/50/25% HP use 154/155/156 at ground position +(0,1), looping; repair/death removes the fire. HP maxima retain the pre-existing uncertainties |
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
| Timers: start at 0 and count every tick; `set(V)` sets and starts, `stop`, `kill`; `timer(0)` exists from the start | P | |
| `side(S).units` = living non-buildings; `towns` = living town halls (outpost / village / town / fortress types); `enemies` = units and town halls of hostile sides | P | |
| `tactic(N)` = living objects of the side with `[Team#] ScenarioID = N`; `valid` 0/1; `idle` = all idle; `attacked` = id of the last attacker or -1 | P | |
| `attackTown(id)`, `moveTo(x,y)` = attack-move of the whole group (tiles -> pixels, clamped to the map) | P | |
| `joinTactic`, `giftToSide`, `kill`, `lock/unlock`, `setPath/setCycledPath/...` (successive points of `[Paths]`) | P | |
| `setFriendlyTo/NeutralTo/EnemyTo` changes the relation one-way (8x8 matrix) | P | |
| `addMoney/takeMoney`, `printString`, `centerView`, `cameraActivate(i,v)` (jump to `Camera#`), `victory/defeat` | P | the mission ends for the script side |
| `boat(N).activate`, `johnka(N).enable`, `submarine(N).go`, `buildCommand`, `moveCommand`, `cameraDeactivate` | not done | counted as `unsupported` |
| Defences from `[SideN] Defence#` appear as objects (towers); `defence(N).valid` = alive | P | Reference loader creates each defence with a dedicated tactic and optional scenario ID. Camera records are separate; see `rules_ai.md` |

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
| Defence reactions | P (G fallback) | A new live visible hostile attacker of a town structure redirects eligible autonomous town troops, including moving troops. Eight-tile threat bounds and plan entry are explicit fallback choices. Script-owned tactics are preserved. Placement failure disables defence attempts; unaffordability does not |
| Placement | P (G fallback) | Stable radius/row/column search within eight tiles, weighted resource bytes for terrain harvesters. Failed placement produces no purchase, payment or reservation. Map construction uses placement offers; flag-0 farms/mines/defences use queued automatic placement, while hall upgrades replace the existing hall |
| Population and tactic limits | open | Removed the invented 80-unit cap. No difficulty bonuses or universal population limit. No reference kill-time assignment, 15..23-tick tactic scan or full side-response arbitration is claimed |
| `Tendency`, `BuildSpeed`, `Difficulty` from `.mp` | no effect in rewrite | Runtime speed is selected by the strategist; mission loader keys do not override it |

`ai_unit` checks synthetic economic preferences, development, speed changes,
construction/queue suspension, fog, wave release above 80 units, defence alerts,
script ownership, sparse town IDs with queued structures and harvest accounting,
and `.scn` attackTown/moveTo/path actions across viewing sides.
`ai_skirmish` runs twenty simulated minutes (14,400 ticks) twice, comparing every
tick hash across viewing sides, then checks command-log replay. Activity assertions
require construction, income, production, combat orders and casualties. This is an
asset-independent headless engine test, not reference parity.

The task 03 resource-map and pricing work is present, but its independent town
queues and start-payment/refund semantics are still incomplete. Task 01 normalized
oracle comparison tooling is absent. Town-command and hall-upgrade support is
present; autonomous hall-upgrade selection, exact economic queue semantics, tactic
assignment and reference timing parity remain open. Existing unsupported boat/johnka/submarine, build/move script
actions and camera semantics remain listed in the mission-script section.

The private mission probe used a private game copy, dummy SDL drivers and
`unshare -rn` through `tools/zstat_run.sh`. Namespace setup failed with
`Cannot open netlink socket: Operation not permitted`; no reference run or parity
result is claimed. All private inputs and probe outputs remain under ignored work/.

## Audio (`src/engine/audio/`)
| Rule | Status | Notes |
|---|---|---|
| Effects from `Snd.xbe`: sound set (`sound_set`) from field [2] of an animation frame, played on the first tick of the frame, random member of the set | V (format), P (random choice uses a local LCG, not the simulation RNG) | |
| Pan = (x - view centre x) clamped to +-4000 hundredths of dB; attenuation: none up to 500 px, linear to -4000 at 800 px, silent beyond | V (`docs/formats.md`), P (100 units = 1 dB) | the "fog cell >= 0x21" condition is not applied |
| Music: `Music=N` -> track N, otherwise `MusicMask`; next track when one ends | V (mask selection), P (order = seeded by mission name, cyclic) | `music_allowed_mask` in `core/audio` |
| Volumes, speech / subtitles, UI sounds, state loops (sequence header), building / projectile sounds | not done | `--volume`, `--no-audio` |

## Saves
| Rule | Status | Notes |
|---|---|---|
| Engine save: mission + tick + hash + camera + hash of the `.scn` + full command log; loading replays deterministically on a fresh world and verifies the hash | engine | `Saves/engine_slot1.azs`, Ctrl+S / Ctrl+L and the pause menu; a different script file is refused |
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
non-map structures use automatic placement, while concurrent town queues and exact
original placement/spawn ticks still need integration/evidence. Repair credits are exposed per town for the current
tick; spending them belongs to the separate repair-service task.

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
covers secondary HP and the explicit regeneration selector. Old engine saves
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
Repair controls remain disabled pending repair-service integration; credit
production is available separately.

**Engine construction checks:** synthetic invalid town/side/target, multiple-town
extraction, repeated empty extraction, duplicate/repeated toggles, mode-change
clearing, same-mode preservation, full-HP repair rejection versus destroy
acceptance, hall/mobile/construction exclusions, off-mode collection, stale-target
cleanup, hash sensitivity and serialized save/command-log replay pass. Tests do
not establish reference tactic numbering, off-mode service timing or packet
compatibility. Hash changes mean older saves with recorded hashes are incompatible.

**Dependency boundary after merge:** resource-based income, per-tick repair-credit
harvesting, town-local prerequisites and producing-town attachment for new spawns
are implemented by the economy work described above. Production queues remain
side-wide. Damage-priority groups, repair-budget allocation and destruction
damage remain pending. Hall-upgrade completion/build-tree changes and upgrade
save/replay scenarios are covered by the subsequent flag-0 production entry. Available credits do not yet drive repair service. The mission
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
duplicate structures and competing hall upgrades in the same town. DisabledID,
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
reference placement algorithm. Production waits at zero remaining time if its
hall is dead or no footprint fits; queue cancellation remains available. Hall
upgrades require a free replacement footprint and assign the new descriptor's
full HP on completion. Original HP transitions, active-building suspension and
exact start/completion ticks remain open. The existing side-wide eight-entry
queue and full cancellation refund are unchanged placeholders. Map construction
continues to use the build-tree construction flag; flag-0 commands ignore map
coordinates. These engine command conventions do not establish packet parity.
Town levels are now hashed, so older saves with stored hashes are incompatible.

**Synthetic validation:** all supported race/level upgrades, sparse town section
numbers, preserved hall identity and footprint blocking, build-tree unlocks,
competing/repeated requests, paused timers, flag-0 farms/mines/defences, adjusted
costs, completed/queued duplicates, resource accumulation after completion,
DisabledID, insufficient money, invalid town members, dead halls, cancellation,
serialized command replay and upgrade save/replay are covered in `engine_unit`.
These checks validate engine construction, not measured original behavior.

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

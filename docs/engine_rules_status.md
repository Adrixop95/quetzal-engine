# Engine: status of the simulation rules (verified / measured / placeholder)

Simulation: `src/engine/sim/game_sim.*` (12 Hz, fixed-point harvests and original float32 price factors, driven by the command queue in `commands.hpp`). Data comes from `docs/data/*.csv`, loaded at run time (`rules.*`). The reference specification is [rules_summary.md](rules_summary.md) with details in [rules_types.md](rules_types.md) and [rules_ai.md](rules_ai.md). New reference findings below do not imply engine implementation changes.
Movement and pathfinding: `src/engine/sim/path/` (movement agents; the interface is swappable: `GameSim` only uses `MoveSystem`).
Legend: **V** = verified against the original (code or screenshot), **M** = measured on the recompiled original, **P** = placeholder (invented, to be replaced).

## Core simulation
| Rule | Status | Source / notes |
|---|---|---|
| 12 Hz step, 24.8 fixed-point positions, state hash after every tick, replay from the command log | V (construction) | no comparison with the original yet; the engine hash differs from the recompilation's hash |
| State hash covers objects (incl. secondary HP, regeneration selector, destinations, orders, construction timers), sides (money, queues including paid costs/town ownership, groups, build pause), day clock, town speeds, repair credits and harvest accumulators/cursors, spawn counter, relations, script state and the full movement system (agents, orders, slots, steering caches) | engine | collection sizes are mixed in |
| Command types (PCT_FRAME_*) and their numbers | V | `docs/object_types.md`. Implemented: PAUSEON/OFF, CREATEGROUP, ADDTOGROUP, MOVE, ATTACK, BUILD, BUILDMODE, DESTROYHERE, QUEUEKILL, PAUSEBUILD. Logged only: SELECTGROUP, GARRISON, REPAIRMODE, ATTACKMODE, SELECTBYNO, FOLLOWTEAM |
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
| Town upgrades and non-map structure production | not done | Structure entries with flag 0 (including farms/mines) remain skipped by the existing menu; preplaced farms/mines use the new income rules |
| Side `DisabledID` blocks menu entries | V | `docs/formats.md` |
| Weapon damage | V documented arithmetic / engine synthetic checks | Marine d7 and Defence d59 select weapon IDs. Classes 1/2/13 apply each matching 8.8 multiplier in sequence, then armor term; arrow zero-air suppresses armor. Classes 4/5/6/10 constants implemented; 8 delegates and 12 sums known child damage. Historical duel entries are diagnostics only. Other descriptor weapon mappings/armor retain placeholders |
| Weapon reload getters and cooldown | V Marine reload+1 / P other scheduling | Classes 1/2/13/4/5/6/10 use documented reload/range. Simulation schedules reload+1; this is verified only for Marine cooldown, not universal hit intervals. Composite scheduling remains unresolved |
| Projectile and unresolved weapon behavior | P / unresolved | Projectile speed is distinct metadata, not added to reload. Hits remain immediate: animation/flight delay is not implemented. Classes 0/3/7/9/11, complete burn behavior and composite scheduling are unavailable calculations; known unresolved descriptor weapons do not silently fire generic damage |
| Weapon range | V getters / P geometry and fallback | Established weapon getters replace generic ranges; Euclidean centre distance plus placeholder target radius/footprint remains. Types without loaded weapon descriptors retain 56/288/336 px placeholders |
| Sight range (target acquisition, fog) | P | Rewrite: 288 px (6 tiles), buildings 8 tiles. Reference: per-type interpolated light-mask IDs, daylight and brightness >32; separate tactic scan and weapon checks |
| Auto-attack of enemies in sight (idle and attack-moving units) | P | nearest enemy, ties = lower id; hostility from the relation matrix |
| Death and corpses | P | death animation from `Units.xbe`, corpse for 480 ticks, then removed; buildings vanish at once |
| Fog of war: tiles in sight = lit, previously seen = 96 | P | values 0 / 96 / 152 from `.vis`, updated every 4 ticks |
| Building construction: placement on free tiles, progress = adjusted build time, stages = animation slot 0 | V formula/stages, engine placement | Hall/speed/computer time applies to placed construction. Original placement/worker and exact completion semantics remain open |
| Building footprint in tiles | P | desc d4/d5 (px) rounded up, centred on the clicked tile |
| Unit training: side queue (max 8), first item progresses, 100 % refund on cancel | P queue/refund policy | Items retain producing town and actual payment; cancellation refunds that payment even after speed changes. Spawn below that town hall. Independent simultaneous town queues remain unimplemented |
| Building income and repair supply | V formulas / synthetic checks | Descriptor occupancy cells equal to 2 weight resource channels A/B with d60/d61; periodic/scheduled 7200-tick clock from StartTime. Gold and d62/d320 repair supplies have separate accumulators. Completed same-town warehouse applies once to farms only. Missing resource map contributes zero terrain bytes |
| Starting money = side `StartMoney` | V | `.mp` |
| Garrison, repair, attack modes, population limits, difficulty levels | not done | Reference garrison extracts town troops; repair/destroy use marked building lists and repair credits. Universal population cap and other difficulty bonuses remain open |

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

## Computer player (`sim/ai.cpp`, skeleton)
Controlled sides are those with `OnlyComp=1` that are not neutral (a mission property, the same on every peer).

| Rule | Status | Notes |
|---|---|---|
| Passive income | V formula / synthetic checks | Supplied by active completed buildings, including halls; removed the extra computer-side +60/300 grant |
| Buys an affordable unit when its queue is empty (deterministic random choice) | P planner | Removed the invented universal 56-tick gate. Runtime town speed and original price/time formulas apply; automatic strategist speed selection remains unimplemented |
| Unit cap 80 | P | |
| Defence: a hit building sends idle units within 10 tiles (attack-move) to the attacker | P | |
| Attack waves | P, scripts only in rewrite | Reference town states 1/2 autonomously prepare/activate offensive plans in addition to mission scripts; planner scoring still open |
| `Tendency`, `BuildSpeed`, `Difficulty` from `.mp` | no effect in rewrite | Reference town loader ignores Tendency/BuildSpeed keys, but runtime strategist selects speed. Difficulty is not in inspected price/time formulas; other effects open |

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
normal speed; the AI strategist, active-building suspension semantics, non-map
structure placement, concurrent town queues and exact original spawn ticks still
need integration/evidence. Repair credits are exposed per town for the current
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

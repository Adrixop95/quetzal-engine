# Ground weapon investigation

Evidence date: 2026-11-18. **V** means a fact read from the original's private
factory, methods or existing public rule records. **G** identifies a clone policy
or an unmeasured integration detail. No original program was run in this task.
Private decompilations and the copied Ghidra project remain under ignored `work/`.
Only behavioral facts and independently written C++ are published.

## Corrections to the earlier survey

| Label | Finding and evidence |
| --- | --- |
| V | Factory `0x46d900` maps class 7 to `0x4943a4`, constructor `0x44c010`. `0x494430` is class 8, the volley scheduler; `0x44c8d0` delegates to its explosion prototype and `0x44ca60` sets up scattered emissions. It is not Demolisher's vtable. |
| V | Class 2 uses `0x495130` / `0x46e0d0`; class 13 uses `0x494f10` / `0x46ed90`. The older survey interchanged their factory links. Both do direct category damage plus an optional armor-dependent helper; the latter effect additionally creates a visual trail. |
| V | Marine virtual `+0x9c`, `0x471220`, adds to `+0x84`. Marine Think `0x433db0` decrements this field and sets it to reload plus one at attack initiation. This is the **attack cooldown**, not facing. Marine facing is `+0x80` (`0x433020` returns it times 32). The earlier turning claim used another object's layout. |
| V | Snowman scans the rectangle from attacker position minus (48,48) to attacker position plus (48,48), with capacity 128 (`0x46eb10`, `0x470130`, `0x46b360`). Candidate hit rectangles must overlap it; this is not a circle around the victim. Non-allied ground targets matching either Marine category receive **18 additional cooldown ticks**. Movement speed, facing, ownership and secondary HP are unchanged. |
| V | Marine virtual `+0x104`, `0x4029d0`, adds primary HP and caps at maximum. Jaguar heals computed damage divided by three, including overkill. Its 1.25 category multiplier is an OR predicate and applies once. |
| V | The common hit rectangle (`0x46ba50`) is x minus 28 through x plus 28, y minus height minus 48 through y minus height plus 16. Point queries include left/top and exclude right/bottom (`0x46b230`). Virtual `+0x20` (`0x402440`) returns half its width, 28, independently of the clone's provisional collision disc. |

## Damage and effect contracts

| Class / ids | Formula, area and scheduling (V) | Helpers and other state (V) |
| --- | --- | --- |
| 0 / 2,30,32,40,43,46,50 | Ballistic carrier: damage getter delegates to `rec[4]`; speed `rec[1]`, reload `rec[2]`, range `rec[3]`. Speed zero is a vertical drop. No contact damage. At landing, invokes the payload's point effect (`0x406c00`). Flight includes a two-pixel aim-y offset. | Carrier object, followed by class 7 explosion or class 9 point effect. No status or side change. |
| 3 / 5,6,22,24,26,28 | Per moving tick, every live object's hit rectangle containing the projectile point takes `rec[4]`, multiplied by 176/256 for non-organic targets. Airborne and allied contact damage is zero. The getter estimates `floor(48/speed) * contact + payload`, not guaranteed total damage. Speed `rec[1]`, reload `rec[2]`, range `rec[3]`; aim is clamped to range. At destination, invoke `rec[5]` once (`0x4050f0`). | Moving projectile and visual termination phase; payload helper. The scan also calls virtual `+0x118` with the segment and 48; Marine's implementation `0x402a00` is empty. No Marine status or ownership change. |
| 7 / 3,17,18,25,29,37,38,41,42,49,53 | Require target airborne flag equal to `rec[5]`. Let I = `rec[1]+28`, O = `rec[2]+28`, D = `rec[4]`, d = integer pixel distance from explosion. Return zero beyond O; return D at or inside I, bypassing armor. Otherwise q = `floor(256*(O-d)/(O-I))`; form D*q in 8.8, multiply by 204/256 if category `+0xe8`, then by virtual `+0x108`, retaining 8.8 precision until returning integer damage. Reload `rec[6]`, range `rec[3]`. No alliance filter. | One explosion animation/sound helper, immediate area damage once. Demolisher invokes it directly at the target; it does not launch animation 157 or kill itself. No cooldown/status applied to victims. |
| 9 / 21,23,27,45 | Base `rec[0]`, sequential 8.8 multipliers `rec[3..7]` for air, non-organic, f4, e8, structure, then add armor term `rec[1]*physical_armor/256`. Point effect `0x4440f0` applies individually to all overlapping live objects, including allies. It has zero standalone reload/range getters and is used as a payload. | No persistent status or extra gameplay helper. |
| 14 / 0 | Absent-chain sentinel. The ordinary factory has no class 14 constructor and returns null; no campaign descriptor selects id 0 as its active weapon. | Save dispatch class 14 represents the optional damage-over-time helper, not an additional usable weapon prototype. |
| 4,5 / 8,13 | Knight and Ninja: 3000, or 1500 if non-organic OR structure; zero against allied or airborne targets. Reload 19, range 72. Effect `0x46e8b0` scans a radius of 96 around the attacker: central sector width 20 angle units at full damage; two sectors width 30 centred 25 angle units either side at half damage. Sector edge scans remain separate. | Temporary weapon/query objects; no spawned combat unit, status, healing or side change. |
| 2,13 / direct family | Shared getter sequentially applies record multipliers and includes the armor term in its kill-time estimate. The effect applies the category/base part immediately. If the armor term is nonzero, it creates helper `0x48f6c4`, which distributes that term across `rec[3]` ticks (`0x46f170`, `0x404730`). Every current public direct record has armor coefficient zero, so campaign weapons in this family create no damaging helper. Reload `rec[0]`, range `rec[4]`. | Optional record-driven muzzle/impact decorations and sound; `0x46ed90` also emits trail animation 107. Helper updates only primary HP and stops when its target dies. No conversion or secondary HP change. |
| 6 / 16 | Snowman: direct 18000, or 9000 against non-organic; zero against air; reload 25, range 10. After the hit, apply the cooldown-area rule above, independently of damage to bystanders. | Additive cooldown only. No ice unit, freeze duration, movement slow or rotation. |
| 10 / 11 | Jaguar: direct 1800, or 1125 against non-organic OR structure; multiply once by 320/256 if f4 OR e8; zero against air. Reload 11, range 10. | Heal attacker by nominal damage/3, capped at its primary maximum. No other persistent status. |

Ordinary damage also invokes the shared hit reaction/tactic notification and, at
zero primary HP, ordinary death and occupancy cleanup. These shared reactions
are distinct from a weapon-specific status; no weapon here converts ownership
or damages secondary HP.

All reload values are weapon getters. Marine attack initiation adds one tick;
vehicle/animation scheduling can differ. The class 0/3/7 and Knight/Ninja CanFire
method uses centre distance, without adding movement collision radii.

## Clone implementation and evidence limits

`RulesData` retains the weapon graph and distinguishes class 3's kill-time estimate
from actual per-tick contact damage. `GameSim` evaluates each payload victim using
its own categories and armor. Both point payloads and explosions can damage allies;
only the class 3 contact component suppresses allied damage. The new transient
weapon identity participates in deterministic state hashes and replay. Point and
Snowman queries retain the 128-candidate limit; candidate ordering uses stable
clone object order until the original spatial traversal is matched.

**G:** animation-trigger timing, object update ordering, original spatial-query
ordering under very dense groups, source height and discrete ballistic
landing arithmetic still need synchronized live comparisons. The clone retains its
existing attack-animation scheduling and 24-pixel ballistic launch-height policy.
The optional direct armor helper is documented but has no campaign activation in
the current tables; nonzero future/modded records need separate scheduler work.
Synthetic replay equality establishes internal determinism, not game parity.

Class 8 volley scheduling and class 11 burning remain task 141 work. They are not
resolved by enabling their class 7 payload getters; the clone keeps class 8
scheduling unavailable until that dependency is integrated. The requested ground
projectile, explosion, point, melee, healing and cooldown paths are independent.

## Live run sheet

Do not run the original in this sandbox. On a namespace-capable host, use a private
symlink installation copy and dummy SDL inside `unshare -rn`, following
[EXPERIMENTS.md](EXPERIMENTS.md). Keep captures and derived state private.

| Probe | Record on two independent runs | Purpose |
| --- | --- | --- |
| Mage and crossbow tower against stationary and moving Marines/vehicles | Launch, each contact tick, aim point, impact, HP, category, armor; allied bystander and air control | Check pulse count, per-target point overlap, impact location and damage rather than assuming the damage getter equals a hit |
| Demolisher against groups at inner/outer boundaries | Centre positions, query membership, full/falloff HP deltas, attacker HP, launch and repeat ticks | Confirm 28-pixel extent, rounding, friendly fire and direct timing |
| Knight and Ninja in a fan | Victim bearings at central/flank edges, distance 96/97, armor/category and HP | Verify quantized angles, inclusive edges and possible multiple scan membership |
| Snowman with organic, mechanical, allied and airborne neighbours | Marine cooldown `+0x84` every tick, position, speed, facing `+0x80`, HP and owner | Confirm additive 18 ticks, square query bounds and update-order offset |
| Injured Jaguar against high and near-zero target HP | Attacker primary/max HP and target damage including overkill | Confirm nominal-damage heal and cap |
| Direct class 2/13 and ballistic/point payloads | Attack animation event, effect creation/destruction, actual landing position, HP/secondary HP and owner | Separate cosmetic helpers, hit scheduling and damage |

## Verification

Baseline: Release build, 63 CTest passes, six skips and pre-commit passed.
Final: Release build, 64 CTest passes, six skips (70 entries total), and all
pre-commit hooks passed, including explicit checks of the new untracked files.
The skipped entries are formats, engine, lockstep_processes, save_import_private,
script_campaign_inventory and parity_rules_private. No reference record/replay
or private-data experiment was run; the run sheet remains needs live run.
Rules regeneration produces the same JSON bytes, including its final newline.

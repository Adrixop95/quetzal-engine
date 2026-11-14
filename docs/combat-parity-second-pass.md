# Combat parity second pass (task 117)

Status: partial implementation; needs live run. The changes below close isolated
formula and Marine sequencing differences. They do not complete tactic assignment,
all complex weapon lifecycles, or campaign combat parity.

## Evidence and scope

V means a fact checked in the private lifted instruction stream; G means an
assumption, approximation, or still unverified interpretation. Engine behavior is
identified separately. Function addresses identify research evidence, never code
to include in this public repository. Only independently written implementations,
formulas and synthetic fixtures belong here. No original executable was run.

The baseline Release build and pre-commit gates passed on this checkout. CTest
passed all 63 runnable entries; five private-data entries and the restricted UDP
process check skipped. This is an engine regression baseline, not game parity.
The worktree was clean when the interrupted task resumed; no previous task notes
or partial implementation were present.

## Attack pipeline comparison

| Stage | Original evidence | Clone comparison and disposition |
|---|---|---|
| Weapon creation and selection | V: `0x46d900` dispatches weapon ID through the class/parameter table; `0x4166a0` rejects absent weapons and damage <= 0 for the actual victim. | `RulesData::weapon_impl`, `attack`, `GameSim::run_orders` load descriptor bindings and reject unknown/nonpositive results. Composite and secondary weapons require distinct dispatch, not one universal hit. |
| Category arithmetic | V: `0x46dfe0` rounds the integer after each category factor for classes 2/13. Arrow `0x403e10` instead starts with base damage times 256, retains eight fractional bits through each `0x404810` multiplication, and shifts once at return. `0x403da0` adds armour separately. | Fixed: `weapon_impl` now distinguishes Arrow's retained fractions from integer `category_damage`. No universal minimum damage or subtractive armour rule is established. |
| Fixed weapons | V: `0x46e580`, `0x46ead0`, `0x46ec90` apply their category predicates independently of descriptor damage. Class 10 uses one bonus if either +0xf4 or +0xe8 is true. | Fixed: `weapon_impl` applies that bonus once. Air and allied exclusions remain family-specific. |
| Range and minimum range | V: Arrow/Axe/Ball share `0x4038a0`, comparing `0x461fe0` of the source-to-aim-point vector against the maximum getter. No positive minimum appears there. Classes 2/13 use `0x46d660`, with descriptor byte d13 selecting centre (nonzero) or edge (zero). Classes 6/10 use `0x46d750`, subtracting both object radii. | Fixed: `WeaponResult::edge_range`, `combat_distance` and `weapon_reach` distinguish the branches. Combat radius is half the bounding width, independently of the movement disc. G: range still uses the current target centre rather than the reference predicted aim point. |
| Visibility / line of sight | V: `0x433db0` drops an invisible pursuit target and validates visibility/CanAttack for assignment. `0x43e9f0` checks liveness, target-dependent weapon and CanFire. The inspected `0x4038a0` predicate is a range test, without a terrain ray cast. | Fixed: `run_orders` drops invisible Marine pursuit when lighting inputs exist, in addition to visibility-aware acquisition. G: the six-tile asset-free acquisition fallback is an engine policy. No terrain ray cast was invented. |
| Attack animation and release | V: Marine Think `0x433db0` starts direction slot 9..16, retains a separate release target, starts reload+1, and releases when the current frame's fire field is nonzero, on each tick that flag remains set. Frame movement is applied before release. Animation completion returns to idle. | Fixed for Marine: `RulesData::attack_frame` reads runtime animation records; `GameSim::advance_attack` retains direction/target, applies displacement, releases from frame flags and waits for completion. Both app entry points load the archive independently of daylight/executable availability. G: asset-free simulations retain immediate release as an explicit fallback; vehicle release/turn state remains incomplete. |
| Cooldown | V: Marine decrements a positive counter before testing, then initializes reload+1 at attack start. `0x460a10`/`0x42c880` have separate vehicle timing paths. | Marine fallback scheduling already used reload+1. The independent animation clock must also prevent restarting an unfinished attack. Vehicle turn and release timing remains separate work. |
| Projectile speed and aiming | V: Arrow speed is descriptor field 1; `0x403d50` estimates flight using the reference norm divided by speed. `0x403940` calls the target prediction accessor; `0x403f40` initializes a fixed velocity and clips the aim to range. | `fire_weapon` snapshots the current target position and uses Euclidean normalization. G: current Ball gravity/aim and Axe movement are approximations, not measured trajectories. |
| Projectile hit test | V: Arrow Think around `0x40421b` advances fixed velocity and tests impact against the living target's current half-open bounding rectangle, accounting for projectile height. A secondary weapon dispatch precedes the direct-hit branch. `0x46ba50` supplies the generic rectangle: x [-28,28), y [-48,16), offset by object height; `0x411330` and `0x41e220` use Building d2..5 / Defence d5..8 instead. | Fixed: `advance_effects` checks Arrow against the current rectangle and height; moving away can now cause a miss. `load_dir` loads structural combat bounds separately from tile footprints. G: flight height/prediction and complete secondary dispatch still need integration. These bounds are logical rectangles, not sprite pixel masks. |
| Area damage | V: `0x44c130` checks airborne category, adds victim combat radius to inner/outer distances, returns full damage in the inner band, and uses an 8.8 falloff ratio outside it. The outer-band 204/256 factor checks **+0xe8**, not structure +0xec; the victim blast multiplier follows. | Fixed: `blast_damage` uses +0xe8, quantized 8.8 falloff and retained fractions. `advance_effects` uses the integer pixel norm and evaluates victims independently, including allies. Unknown family-specific blast-multiplier getters still use the existing 256 fallback. |
| Healing | V: `0x433db0` checks the natural/service HP threshold before adding regeneration, permitting overshoot. | `regenerated_hp` models this, with a malformed-data overflow bound. Town service is a distinct explicit HP operation, not a negative generic weapon. G: no complete active-healer weapon behavior is established by this pass. |
| Kill estimate and priority | V: `0x46fac0` uses ceiling(HP/damage) times reload, at least one hit; +0xf8 attackers use secondary HP. `0x451c30`/`0x452c70` perform group assignment, CanAttack checks and allied-damage weighting. Ordinary buildings are omitted from the normal global candidate list; Defence passes +0xfc. | `estimated_kill_time` exists, but nearest distance and stable ID ties every four ticks remain G. The clone merges global/local/explicit-town assignment in `nearest_enemy`; a universal building filter would also remove the town-attack fallback. Complete the distinct tactic assignment paths before applying that verified global filter. |
| Retaliation and chasing | V: `0x433db0` drops pursuit beyond reference norm 1152; `0x455b10` suppresses following in direct mode. `0x45ece0`/`0x461a30` notify town/side tactics. | Fixed: `run_orders` drops excessive Marine pursuit, stopping its old path or resuming interrupted movement. G: path refresh and nearest firing tile remain independent movement policies. `last_hit_by` supports attack notification, but full group retaliation is not implemented. |
| Fleeing | G: a universal HP-percentage flee threshold is not established. Reference tactic/path state must be distinguished from weapon minimum range. | No universal flee rule should be added from conjecture. Tactic retreat state, trigger and destination remain to be traced and measured. |
| Kill credit | V: projectile objects retain a reference to their originating object; damage is dispatched to victim-specific handlers (+0x124). | `last_hit_by` preserves an ID even after shooter death, but is an attacked-condition input, not an implementation of the full reference kill/experience/side notification contract. Victim-specific handlers and conversion remain open. |

## Formula checks with invented inputs

Use synthetic tables and animations only. In particular, distinguish integer
category damage from Arrow's retained fraction: base 2 with two factors 192/256
gives 0 for integer-per-stage arithmetic and 1 for Arrow. Class-10's bonus applies
once even if both predicates are true. For a blast, quantize the falloff ratio to
8.8 before multiplying damage, retain its fraction through the category/victim
factors, then take integer HP. Check inner/outer boundaries and allied victims.

Animation tests must include delayed release, multi-tick fire flags, attack
completion longer than reload, pause, death before release, movement between
start and release, command retargeting, and command-log replay. Deterministic duel
and group outcomes only establish the tested formula/state contract; they do not
establish full original fights with movement or tactic assignment.

`tests/test_combat.cpp` creates its own small CSV tables and animation bank in a
temporary directory. Its single-duel fixture has 40 HP versus 20 HP, damage 7,
reload 2 and a four-tick animation whose fire flag follows two wind-up ticks.
V formula: reload+1 starts at attack initiation, while animation completion also
gates the next attack. The expected starts are ticks 1, 6 and 11, with releases
at 3, 8 and 13. G: the engine processes these synthetic objects in vector order,
so the first attacker survives with 26 HP after the opponent's third release is
prevented by death. In the two-versus-two fixture, explicit focus and retarget
commands kill one opponent on tick 8 and the other on tick 18; survivors retain
12 and 40 HP. Every intermediate state matches command-log replay.

Additional checks cover centre/edge ranges and axial norm boundaries, separate
combat/movement radii, Building/Defence descriptor bounds, outer-band fractional
blast arithmetic, Arrow half-open collision bounds and escape, retained release
target after a stop, frame displacement, pause, held recovery frames, shooter
death, and missing animation archives. Existing engine tests now check the
corrected siege envelope, combat radius and blast category/rounding.

## Remaining code dependencies

- Task 90's distinct tactic assignment/attachment state is absent: the existing
  scanner cannot faithfully implement global filtering, group damage weights,
  saturation, response propagation or flee state. A trial universal building
  filter made the synthetic AI waves enter the provisional capture radius and
  end production early. That trial was removed; the original stage distinction
  must be implemented instead of weakening the sustained-opposition test.
- Task 93/123's vehicle turning, altitude and synchronized animation state are
  incomplete. This change deliberately implements only the inspected Marine
  release state. It does not call an eight-tick visual hold an original cooldown.
- Task 110's unit-specific handlers/conversion are not present here. Chained
  Arrow armour/burn effects, Axe per-flight victims, FireThrow, active healing,
  Hypno side changes and kill/side notifications need those separate lifecycles.
  Potential damage from a getter is not evidence that one instantaneous generic
  hit correctly implements a special weapon.
- Flight still uses snapshot aiming and the existing independent trajectories;
  reference target prediction, strict arrival phase and height interpolation
  remain different. The rectangle check closes the guaranteed-hit error without
  claiming that these trajectories match.

These are code gaps as well as measurement gaps. The Windows run sheet below is
not a substitute for completing their implementations. This task remains open
for full pipeline acceptance; it has not been moved to `tasks/done/`.

## Maintainer Windows run sheet: needs live run

No captures have been collected here. Use an isolated private installation copy
and separate profile/save/map directories for each native run. Follow the
isolation/record-replay requirements in [EXPERIMENTS.md](EXPERIMENTS.md) on a
namespace-capable reference host; this Codex environment must not launch the
reference binary or bypass isolation. On Windows, retain the exact native build,
mission/map setup, speed, seed and command inputs privately. Nothing extracted
from the installation, including numeric observation dumps, belongs in this repo.

For every case collect two independent native runs and two clone runs. Record
logic ticks, frame/slot/fire flag, source and victim identity/type/side, positions,
HP/secondary HP, cooldown, projectile creation/impact and assigned/pursuit target.
Compare normalized event sequences, not engine/reference raw state hashes.

| Case | Setup and boundary samples | Question still requiring native evidence |
|---|---|---|
| Each weapon family | Duel with a stationary target, then moving target; close, exact boundary and beyond range, including diagonal approaches. | Exact range geometry, prediction, minimum-range exceptions, first release and repeated hit intervals. |
| Attack animation | Slow-motion Marine attack; cancel/retarget before and after the fire frame; kill shooter/victim during wind-up. | Command interruption, release-target lifetime, repeated fire flags and frame advancement phase. |
| Occlusion | Repeat identical orders with terrain walls, a building, darkness and allied vision separately. | Visibility versus true projectile obstruction; do not equate an unreachable path with blocked sight. |
| Projectile collision | Move target across each edge of its sprite rectangle before impact; repeat with airborne victim and changed animation. | Current rectangle/height, strict arrival comparison, secondary payload on a direct miss. |
| Blast group | Separate inner band, annulus and outer boundary; mix +0xe8, structure, air, allies and neutrals. | Spatial enumeration, exact norm/radius, integer HP transitions and same-tick ordering. |
| Healing/conversion | Natural and serviced recovery around threshold; active healing/Hypno against organic, vehicle and structure targets. | Secondary-health application, side conversion, credit and notification. |
| Group priority/retaliation | Two attackers and mixed weak/strong targets; repeat reversed placement/order, allied focus fire, attack on a town and ordinary building. | Assignment weights, scan RNG cadence, saturation, response propagation and explicit-building orders. |
| Chase/flee | Target crosses visibility and 1152-norm boundaries; direct/defensive/aggressive/balanced modes; vary HP and group losses. | Exact drop/resume tick, tactic anchor, retreat trigger and destination. |
| Kill credit | Shooter dies before impact, simultaneous lethal shots, splash ally kill and secondary burn kill. | Credited object/side, kill counters, overkill and victim-handler notifications. |

Task 87's audited all-weapon observer and the normalized reference comparison
remain prerequisites for claiming measured all-family parity. Their prompts and
run sheets are present, but no successful native combat captures accompany this
checkout. The code-only work above does not depend on running that observer.

## Validation of this worktree

After the changes: Release configure/build with `AZTEC_GEN_DIR=` and no game
installation passed. CTest passed all 64 runnable entries out of 70, including
the new synthetic combat check and the twenty-minute AI opposition/replay soak.
The same five private-data checks and restricted UDP process check skipped.
`pre-commit run --all-files`, explicit pre-commit checks of both new untracked
files, and `git diff --check` passed. No reference run, native Windows observation
or campaign victory was performed. All changes remain uncommitted because git
metadata is read-only. The protected roadmap/experiments/parity files and public
rules tables were not edited.

# Marine shot period implementation

Evidence labels here: **V** = inspected reference control flow, archived log
observation, clone construction or resource fact (identified below);
**G** = inference or unresolved original behavior. V does not claim live parity.
No original executable or reference runtime was run for this task.

## Duration and tick pacing (V)

The clone can derive finite attack lengths from the user's animation archive.
`RulesData::load_combat_animations` loads `Res/Units.xbe` through `SpriteDb`.
Marine types use the body animation from `kTypeAnims`, slots `9 + facing`
(9..16). `AnimSet::frame` reads each frame's duration; `AnimSet::at_tick`
uses their sum without a time conversion. `GameSim::tick` increments
`attack_tick` once per unpaused logic update, before processing attacks.
Frame zero is tested in the attack-entry update. Thus a finite sequence of
total duration L occupies elapsed ticks 0..L-1 and ends at elapsed tick L.
The end update clears the retained release target and returns; a new attack
can start at L+1. Reload decrements before the ready check in each update.

The nominal simulation rate is 12 Hz. Interactive speed changes wall-clock
spacing between updates, not the number of logic ticks in a sequence.
Neither rendering interpolation nor wall-clock milliseconds multiply L.

The following aggregate lengths were checked with the clone's `SpriteDb` and
`RulesData` readers against the locally supplied installation and `docs/data`.
No frame records, sprites or generated fixtures are included here. Ranges
cover all eight facings. Periods assume a continuously available, stationary,
fireable target and unchanged facing; target acquisition, movement and delayed
damage can add time.

| Type id | Marine type | L (logic ticks) | Reload r | Existing effective period | Explicit max period |
|---|---|---|---|---|---|
| 0, 36 | Footman, Spider Axethrower | 7..8 | 9 | 10 | 10 |
| 1 | Rifleman | 10 | 20 | 21 | 21 |
| 2 | Demolisher | 12 | 30 | 31 | 31 |
| 5 | Bear | 12..14 | 16 | 17 | 17 |
| 6 | Knight | 19 | 19 | 20 | 20 |
| 27 | Archer | 11 | 12 | 13 | 13 |
| 28 | Mage | 5 | 20 | 21 | 21 |
| 29 | Tuzem | 7 | 27 | 28 | 28 |
| 31 | Jaguar | 6..7 | 11 | 12 | 12 |
| 32 | Bizon | 6 | 33 | 34 | 34 |
| 53 | Ninja | 3 | 19 | 20 | 20 |
| 54 | Hypno | 30 | 20 | 31 | 31 |
| 55 | Chief | 11 | 6 | 12 | 12 |
| 56 | Snowman | 9 | 25 | 26 | 26 |
| 57 | Elefant | 15 | 40 | 41 | 41 |
| 82 | Abdula | 3..5 | 11 | 12 | 12 |

Previously the numeric cooldown used the reload term, while `release_target`
blocked new attacks through the animation's end update. For these Marine
resources, those two gates already produced the effective maximum above.
The implementation now also sets Marine cooldown explicitly to
`max(1, reload + 1, L + 1)`, using the selected facing's resource duration.
The retained-target gate and animation-marker release remain in place.
This makes the cooldown describe both constraints without changing the
established finite-sequence launch cadence. Other class schedules are unchanged.

`marine_attack_ticks` returns -1 for missing resources/sequences, non-Marine
types, holding or negative durations, or a sum without room for the re-entry
tick. Such inputs do not establish a finite L. The existing reload fallback
and release-target handling remain: missing resources release immediately;
a valid holding recovery frame continues to block re-entry. No measured
26/27-tick constant or hard-coded animation length is substituted.

## Reference attack path (V, static inspection)

Read-only inspection of the private full decompilation establishes the following
path in Marine Think `0x433db0`. Addresses identify evidence; no recovered source
or frame records are reproduced here. Offsets below are bytes, as in
[unit_state_machines.md](unit_state_machines.md).

1. Except in stand state 1, advance the animation once with `0x403460`.
   In every living state except dying state 4, decrement nonzero reload `+0x84`
   once, including while attacking, walking or fidgeting.
2. Validate chase `+0x8c` and fire `+0x90` targets. Death, visibility, pursuit
   distance and can-fire checks can discard them. When fire target is empty and
   reload is zero, call tactic `+0x120` **in this update**, before attack entry.
3. Tactic callback `0x455ad0` calls Local_assign `0x452c70` synchronously when
   its assignment block exists and the movement/mode gate permits it. Local_assign
   selects a live, hostile, currently fireable enemy from the existing table,
   then calls the unit fire setter `+0xd4` (`0x433650`). That setter only replaces
   `+0x90` and adjusts references. Neither callback installs a countdown.
   The tactic's separate periodic discovery/replan can delay finding a new enemy;
   it is not a per-shot wait when an eligible enemy is already assigned.
4. With reload zero and a valid weapon/target, enter attack state 3 unless
   currently stepping (state 2) or attacking (state 3). Facing `+0x80` is set
   immediately, without a turn timer. Start sequence `9 + facing` with
   `0x4034c0`, set reload to weapon getter `+0x60` plus one, retain target at
   `+0x94`, and clear `+0x90`. The same update tests the first attack frame.
5. State 3 applies the current frame's position offsets, then tests its fire
   marker and launches the weapon through
   `0x46d900` and weapon `+0x7c`. Sequence end releases `+0x94`, sets stand
   animation/facing and returns to state 1. There is **no state 3 -> 5 recovery**.
   Entry was already tested earlier in this update, so an animation-bound attack
   needs one further update. This is the existing L+1 term, not an added idle tick
   after a longer reload has expired.
6. On a subsequent stand update, attack entry is tested **before** the state-1
   tactic member-step callback `+0x11c` (`0x455a90` -> `0x455b10`). While reload
   remains positive, that callback may instead start another tile step via
   `+0xcc` (`0x433820`). Stepping is refused during attack/dying states.
   During state 2, reaching firing range does not interrupt the current tile:
   the arrival branch snaps to the destination, sets state 1, tests attack entry
   immediately, then calls `+0x11c`. It can also request a next tile early when
   near arrival and no shot is ready. Thus movement can outlast reload.

There is no second Marine attack countdown in these paths (V). `+0x88` is an
idle fidget timer, reset to -1 in non-stand states and reseeded to 60..259 in
stand. Only its expiry in state 1 can enter state 5, if a fidget sequence exists.
State 5 still runs the reload/acquisition/attack-entry prelude; it can be
interrupted by a ready attack and otherwise ends in state 1. It adds no mandatory
post-shot delay. `+0x98/+0x9c` are step velocities, not timers. The animation's
own `+0x14` is time within the current frame: `0x403460` adds one per think,
advances at the frame duration, and resets it to zero; `0x4034c0` resets it on
sequence start. The clone's `attack_tick` represents elapsed animation time,
not another original-unit reload field.

## Rifleman discrepancy explained by movement (V / G)

**V, archived observations:** the private `hm/zbr/units.log` underlying
[the published measurement](measured_rifleman_shot_period.md) was inspected
without launching any process from the original. Its Rifleman enters state 3
at logged frames 146, 173, 199 and 226; the Bear loses HP at 151, 178, 204 and
231. Each hit follows attack entry by five frames. The attack animation occupies
10 frames each time. Between attacks the Rifleman returns to stand for one
sample, then walks; it is in state 2 throughout frames 157..172, 184..198 and
210..225. The Bear also changes position during the measured interval.
The historical description of a stationary target is therefore incorrect.
These are aggregate observations from one archived run, not a new measurement.

**V, logging construction:** `runtime/oracle.c` labels samples with the command
oracle clock (`0x4c5730`) at command fetch, before that update's simulation work.
`RT_UNIT_EVERY=1` requests every frame. The constant observation-boundary offset
cancels between starts and between hits; a fixed five-frame release offset also
cancels. It must not be added to reload on every shot.

**G, accounting using the inspected reload rule and r=20:** reload is not in
this log, so its expiry frames below are projected from attack entry. The observed
state-2 intervals cover each projected ready frame through the frame immediately
before the next attack. Arrival, already a firing gate in the code, accounts for
exactly the excess in all three gaps:

| Attack entry frame (V) | Projected ready frame, entry+21 (G) | Next entry frame (V) | Waiting beyond reload (G) | Hit gap (V) |
|---|---|---|---|---|
| 146 | 167 | 173 | 6 ticks, still stepping | 27 |
| 173 | 194 | 199 | 5 ticks, still stepping | 26 |
| 199 | 220 | 226 | 6 ticks, still stepping | 27 |

The 10-tick Rifleman animation and 21-tick stationary attack-start period remain
consistent with this evidence. The resource number 18 is an animation id, not
18 attack frames. No universal 5..6-tick term, re-aim wait, extra frame timer or
26/27-tick Marine cooldown follows from the inspected path. With a changing
facing, marker offset or projectile travel, hit gaps can also differ from launch
gaps; here the equal five-frame offsets make that unnecessary to explain them.
A controlled stationary launch trace with reload/target fields is still needed
to verify the 21-tick period live; this run cannot establish it for every tactic.

## Clone consequence (V / G)

**V, construction:** `GameSim::run_orders` already decrements cooldown during
movement and refuses firing while the movement agent has an unfinished tile
step. Target assignment on a ready tick adds no fixed wait. No simulation change
is needed for the identified movement gate. Footman remains 10 ticks and Rifleman
21 for stationary, continuously fireable targets. The explicit maximum above
continues to cover finite animation recovery.

**G, integration limit:** the clone's tactic scoring, chase/formation destinations
and movement are not established as identical to this reference run. Its existing
tile-completion gate does not promise to reproduce these particular three gaps.
Adding a constant would incorrectly slow a stationary Rifleman. The timing rules
in [animation_timing.md](animation_timing.md) remain applicable under their
stationary-target assumptions, including animation-bound Hypno and Chief.

## Regression coverage (V, synthetic)

`tests/test_combat.cpp` uses invented animation banks and weapon descriptors
for both Footman and Rifleman, across all eight facings. It checks reload-bound,
animation-bound and equal limits, the numeric cooldown, four consecutive
attack starts and repeated damage intervals, including reload inputs 9 and 20
that require periods 10 and 21. Missing resources preserve the
reload fallback. Missing slots, holding frames, negative durations and invalid
type ids do not supply a finite duration. A separate Footman/Rifleman regression
starts a tile step after attack recovery, requires reload to expire while still
walking, forbids firing before arrival, and checks that stationary attacks resume
with periods 10/21. Existing held-frame, marker,
retargeting, pause, death and replay regressions remain applicable.

Validation (2026-11-23): bootstrap with `--no-hooks`, Release configure/build
with `AZTEC_GEN_DIR=` (empty `GAME_DIR`), and full CTest passed before and after
the follow-up (78 passed, six skipped each time). Skips were the five private-data
checks and restricted UDP process creation. Pre-commit passed for all tracked
files. No original executable or reference runtime was run. The original duration
work used read-only resource inspection; this follow-up inspected static control
flow and existing private logs, without copying derived code or log files into
the repository. Native Windows/macOS execution was not available locally.

# Marine shot period implementation

Evidence labels here: **V** = checked clone construction or resource fact,
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

## Rifleman discrepancy (G)

[The existing headless measurement](measured_rifleman_shot_period.md) reports
three hit-point-drop gaps of 27, 26 and 27 ticks. These remain observations
from that run, not newly verified launch periods. The proposed inference
that Rifleman's attack animation takes 25..26 clone ticks is unsupported:
the supplied animation 18 has eight frames per attack facing, totalling
10 ticks. Here 18 is the animation id, not the attack frame count.

The source of the original/clone discrepancy remains unresolved. Establishing
the original update cadence, tick alignment, launch and impact events and
target re-entry behavior requires further reference evidence. The clone data
does not establish an additional pacing factor. This task therefore does not
claim to reproduce the measured 26..27-tick hit gaps.

The computation agrees with the Marine periods in
[animation_timing.md](animation_timing.md), including the animation-bound
Hypno and Chief rows and the Knight tie. Its period column and note are left
unchanged because the computation does not prove them wrong.

## Regression coverage (V, synthetic)

`tests/test_combat.cpp` uses invented animation banks and weapon descriptors
for both Footman and Rifleman, across all eight facings. It checks reload-bound,
animation-bound and equal limits, the numeric cooldown, four consecutive
attack starts and repeated damage intervals. Missing resources preserve the
reload fallback. Missing slots, holding frames, negative durations and invalid
type ids do not supply a finite duration. Existing held-frame, marker,
retargeting, pause, death and replay regressions remain applicable.

Validation: bootstrap with `--no-hooks`, Release configure/build with
`AZTEC_GEN_DIR=` and `GAME_DIR=`, and full CTest passed before and after the
change (76 passed, six skipped in each run). Skips were the five private-data
checks and restricted UDP process creation. Pre-commit passed for all tracked
files and separately for this new document. A read-only resource probe also
checked the duration helper against the summed frame durations for every
Marine facing. This was resource inspection, not a live game experiment.

# Mission startup group formation

Status: **implemented guess; mission 1 acceptance remains unmet**. The best
candidate found matches 22/31 initial walkers in mission 1 and 6/6 in mission 3.
Mission 1 does not reach the requested 90% threshold. This replaces the earlier
measurement-only outcome with actual startup movement, without claiming that the
original formation algorithm has been identified.

The supplied private offline dataset is the oracle. Its repeated measurements
are accepted as evidence for this task; no reference process was launched.
No captures, campaign fixtures, private dataset, assets or executable-derived
implementation are included. The older task-95/98 documents describe the earlier
measurement work; this document records the implemented candidate and its limits.

## Implemented candidate

Fresh mission initialization groups placed ground units by their source town or
team section, independently of script tactic numbers. The first team placement
supplies the team anchor; a town supplies its own coordinates. Original-save
initialization bypasses startup formation. Ships and aircraft are not assigned
this ground movement policy.

`GameSim::init` submits these groups to `MoveSystem::order_formation`. The mover
uses its ordinary integer vector normalization, per-type descriptor speeds,
`NavGrid` movement classes and `FlowField` path costs. Formation has a tile
approach mode instead of ordinary commands' continuous destination lattice:

1. Seed a field with every passable tile in the anchor's central 3 by 3 square.
   Use the existing 10/14 cardinal/diagonal costs, without the steering field's
   extra wall penalty. Rebuild fields if navigation blockers change.
2. Process agents in reverse placement order. This is a deterministic guess at
   pool traversal, not a claim that guest handles equal placement indices.
3. At a tile centre, choose an unoccupied neighbour with strictly lower remaining
   field cost. Minimize the complete edge plus remaining cost. Break ties in
   N, NE, E, SE, S, SW, W, NW order. Diagonals obey the shared terrain corner rule.
4. Reserve both source and destination until a step finishes. Ground units
   outside the group also reserve their occupied tiles; other initial objects
   obstruct their centre tiles. Buildings additionally use their loaded footprint
   masks. Whole-tile steps land exactly on centres at the shared per-type speed.
5. Tick 1 establishes reservations. Displacement starts at tick 2. Stop in the
   central square. If no downhill neighbour is available, wait up to the existing
   mover stuck window (24 ticks), then settle at the current centre. This waiting
   duration and the stopping policy are guesses, not measured original timers.

The waiting window lets a follower advance after another unit vacates a tile.
It improved the production-code comparator from 20/31 to 22/31 in mission 1 and
from 5/6 to 6/6 in mission 3. Immediate stopping, a special dominant-axis stop on
the second ring, and forbidding diagonals between two occupied side cells were
also tried; none improved both missions. No endpoint table or mission-specific
branch is used. There is no fitted speed multiplier or per-member start delay.

Explicit movement/stop commands cancel formation mode. Startup route state,
reservations and obstacle coordinates participate in the mover hash. No random
numbers are used. Balanced and Defensive idle target acquisition is unchanged;
the PR-138 idle leash tests still require stationary units through tick 60.

## Offline comparison

Build and run from the repository root:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DAZTEC_GEN_DIR=
cmake --build build --target formation_probe --parallel 2
python3 tools/formation_fit.py /home/adrox/aztec-re/work/formation/cases.json \
  --maps /home/adrox/aztec-re/game/Maps --probe build/formation_probe
```

The Python tool reads the dataset by the supplied absolute path and sends only
initial placements, anchors, types and real `.dat` passability to the offline C++
probe through stdin. The probe loads the engine's public rules and runs the
production `MoveSystem`. Oracle walking state is used only to select the scoring
cohort; final tiles, step destinations and subsequent samples never select a
candidate route. Add `--details` for private residual diagnostics. Keep its output
in ignored `work/`, not in public fixture files.

“Before” is the no-formation candidate: each member stays on its initial tile.
“After” executes the implemented formation mover. These are movement-only
comparisons, not full mission runs with combat, scripts or moving anchors. In
particular, the probe freezes objects outside the formation as initial obstacles.
The initialization regression separately verifies that real mission placement
records actually activate this mover mode. This does not establish whole-game
parity or a full prior-engine-versus-new-engine mission transcript.

| Mission | Walkers | Final tiles before | Final tiles after | After exact | Tick tiles before | Tick tiles after |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 1 | 31 | 0/31 | 22/31 | 71.0% | 120/868 | 624/868 |
| 2 | 11 | 0/11 | 1/11 | 9.1% | 56/308 | 92/308 |
| 3 | 6 | 0/6 | 6/6 | 100% | 27/168 | 162/168 |

Each tick-tile denominator is walker count times the 28 supplied ticks: 1..24,
30, 36, 48 and 60. The JSON report also gives exact matches at each individual
tick and final matches for the initially idle cohort. Matching a final tile does
not mean its complete trajectory matches: mission 3 still has six differing
walker tick samples. Marine-cohort counts here are determined by this dataset;
they are not substituted from older reports about other mobile kinds.

## Residual explanations

The following identifies every mission-1 endpoint residual using anchor and
initial offset, without publishing the private trajectories. These explanations
state how the candidate fails; the original branch causing each difference is
still unresolved. They are not asserted discoveries about the original code.

| Anchor | Initial offset(s) | Candidate limitation |
| --- | --- | --- |
| (49,15) | (-2,-3) | The candidate can continue diagonally into the central square; the original settles on the second ring. Its stopping predicate is missing. |
| (49,15) | (-4,+1) | Strictly descending field costs prevent the lateral detour around the nearer member. Waiting does not supply that detour. |
| (71,5) | (-2,-2) | Another shortest-path tie is selected and the unit continues inward instead of stopping beside the blocked approach. |
| (5,4) | (+4,-2), (+3,-2), (+4,-1) | All three compete for the same approach corridor. Exclusive reservations, placement priority and downhill-only routing give a different allocation from the original's detours. Each of these three endpoints remains wrong. |
| (78,40) | (+1,+3), (+4,+2), (+1,+4) | The fast member takes the direct central slot instead of making the observed side detour. The other two consequently stop at different reservations. All three endpoints remain wrong. |

Mission 3 has no endpoint residuals. All six tick mismatches are ticks 7..12
for the follower initially at (47,56). Reserving both ends of the preceding
unit's step delays this follower's admission to the diagonal approach. The
original admits it earlier. Shared Euclidean normalization and exact centre
snapping also remain unverified against the original's step-update arithmetic;
no global mover speed change was made to disguise timing differences.

Mission 2 is reported separately and has ten endpoint residuals:

- Six walkers starting at (57,34), (27,49), (29,49), (27,50), (36,25) and (37,23)
  continue far beyond the initial anchor region in the oracle. A fixed-anchor
  startup candidate cannot reproduce these later orders or moving anchors.
- The units starting at (109,112) and (52,117) settle through a different free
  gap in their crowded groups. Direction ties and occupancy classification are
  unresolved; the candidate chooses the opposite side of each group.
- The units starting at (77,17) and (75,16) have no immediately descending free
  step under the candidate reservations, and time out. The original first-step
  choices require a different occupancy/path admission policy.

No missing task dependency was assumed complete. Original path admission,
approximate step normalization and mission-2 moving-anchor/non-formation orders
remain unresolved; the shared mover's existing implementations are used where
available. An exact formation rule requires further inference or controlled
measurements beyond this candidate.

## Synthetic checks

`formation` uses invented placements and navigation grids to check actual
`GameSim` initialization, formation movement, tile reservations, small/large
passability, a unit outside the group, speed sharing, ordinary-command override,
original-save bypass and per-tick deterministic replay. `formation_fit` generates
its own terrain, descriptor CSV and trajectory, checks exact counts and residual
reporting, and verifies that changing an expected endpoint cannot change the
candidate endpoint. Neither test imports private campaign measurements.

# Missions 6–10 player-command playthroughs

Evidence labels (audit 2026-11-23): V = inspected source fact, with reference
or clone scope stated; M = measurement rechecked from retained captures or an
offline clone probe; G = inference, proposed procedure or uncorroborated
historical report. A clone V does not establish native parity. A marker covers
one paragraph/list item or the entire immediately following table. Unresolved
meanings remain G even inside a table whose codec structure is V.
See [label_audit.md](label_audit.md) for counts and evidence limits.

**G** (historical report). Historical scope: The mineral-blocked hall-upgrade and detached-troop transfer descriptions are historical: tasks 150 and the capture/service implementation changed those policies. The old failure is not a diagnosis of the current clone.

**G** (historical report). This task is incomplete. Mission 6 has a winning player-command route; missions
7–10 still require winning routes. Task 112's framework and task 113's code are
present, but [the missions 1–5 report](playthrough_missions_1_5.md) still marks
that prerequisite incomplete. The independent attempts here do not complete it.
The [campaign sweep](campaign_sweep.md) supplies forced-state script coverage,
which does not establish honest winning routes.

All attempts use the faithful profile, unchanged mission inputs, normal player
commands, dummy SDL video/audio, `unshare -rn`, separate private installations
and isolated user profiles. No starting resources, health, ownership, mission
conditions or outcomes are injected. Neither the original executable nor the
reference runtime was run. These are independent-engine observations, not
original-game parity evidence.

Scenarios, observations, command diagnostics, binaries and manifests stay in this
worktree's ignored `work/playthrough/`. The shared research directory is read-only
and its requested playthrough subdirectory was absent. No mission coordinates,
object identities, positions, assets or generated fixtures are distributed here.

## Results

**G** (historical report). Each row uses three fresh processes with one fixed private scenario and the same
engine binary. Anomaly totals include route failures and process diagnostics.

**G** (historical report).

| Mission | Result in each repetition | Tick | Anomalies per run | Remaining acceptance |
| --- | --- | ---: | --- | --- |
| 6 | Victory | 11778 | 0 / 0 / 0 | Winning engine route verified; original-game parity remains unmeasured. |
| 7 | Required troop selection exhausted; still running | 3037 | 32 / 32 / 32 | Complete the conquest sequence in the required order. |
| 8 | Rescue checkpoint fails; still running | 2407 | 20 / 20 / 20 | Survive the northern rescue journey and complete enemy elimination. |
| 9 | Tick bound exhausted; route incomplete and still running | 1500 | 13 / 13 / 13 | Complete the rescue and elimination route beyond this limited horizon. |
| 10 | Catapult selection exhausted; still running | 3419 | 26 / 26 / 26 | Preserve or replace the siege force and complete elimination. |

**G** (historical report). All tick/hash pairs match across the three repetitions for each row. Identical
failed routes remain failures. Earlier winning mission 6 runs with
the shorter detector window remain separate from its clean repetitions.

**G** (historical report). Mission 6 builds an armory and mineral producers, pays for a siege reserve,
witnesses production and income, then attacks through the player command path.
Its intentional reserve-building phase uses an idle bound of 43,200 ticks. Each
planned attack leg has a 1,600-tick travel window. The first exploratory winning
configuration used the default 600-tick goal detector and reported three late
arrivals; fresh repetitions verify the longer window. Stuck-order and failed-path
detectors retain their defaults, and the economy detector remains at 1,200 ticks.
These settings describe this route's planned waiting and travel.

**G** (historical report). Mission 7's first assault destroyed a hall before the capture checkpoint.
Subsequent routes continued the eastern conquest, but troop losses exhausted a
required selection. A separate exploration captures the first town at tick 712
using direct movement while holding back siege troops. This milestone has not
become a complete winning route. Mission 8's northern scouts died before their rescue contact;
its corrected construction and training orders do execute. Mission 10 loses its
catapult force before completing the conquest sequence. An empty-selection error
is a failed route, not proof that the engine ignored a valid command. Large
campaign runs can also exhaust their wall-time budgets.

Earlier probes and interrupted controls remain separate from the repeated
comparison. Detector findings include deliberate waiting, slow combat approaches,
failed assertions and incomplete processes. Their totals are not counts of
confirmed engine defects. Successful landings, town gifts or partial assaults
do not establish enemy elimination or a completed mission.

## Engine finding and synthetic regression

The route framework incorrectly submitted in-place hall upgrades as new building
placements. The ordinary simulation rejected those commands. `build` now uses
the UI's normal `Build` command for upgrades and retains placement checks for new
structures. A synthetic integration regression fails before the fix and passes
after it; it witnesses payment, queue completion and the original hall's stable
identity and position. This fixes command submission without changing upgrade
prices, prerequisites or simulation rules.

**G** (historical report). **F-PLACEMENT-LIMIT — existing fallback, original measurement unavailable.** A
later mission 7 expansion attempt submits and pays for its hall upgrade, but
completion remains blocked. Read-only cell diagnostics identify mineral and
unit restrictions; moving guards alone does not remove the mineral restriction
for the larger upgrade. The [placement policy](101-faithful-placement-and-relaxed-helper.md)
already labels these restrictions conservative and unmeasured. They remain
unchanged. An original placement control is required before changing them;
another player route might avoid this site. This is not proof of unwinnability.

Automatic target acquisition built a full-map Dijkstra field to answer a boolean
reachability query. Campaign probes repeatedly exhausted wall-time budgets.
The query now uses cached connected-region labels for each movement class,
invalidated by navigation-grid changes. It retains the existing blocked-start
relocation and firing-position clearance checks. The cache is excluded from
simulation hashes.

**G** (historical report). Four-neighbour flood fill preserves the existing field's connectivity: every
permitted diagonal has an orthogonal alternative because corner cutting is
forbidden. Synthetic regressions compare every destination with the existing
field across random maps, all three movement classes and blocked/outside starts.
Additional controls cover diagonal separation, terrain changes, overlapping
building blockers, copies, resets and map reloads. Queries must preserve the
navigation hash. No campaign map or executable-derived implementation is used
by these tests.

**G** (historical report). A completed private mission 9 control preserves all 2,001 tick/hash pairs through
tick 2,000 across the optimization. It uses the same player scenario and rules;
its unsuccessful route verdict is retained. This establishes agreement for that
bounded engine control, not original-game equivalence or campaign completion.
Replay semantics remain unchanged.

**G** (historical report). Town transfers also change ownership of detached town members in the current
engine. Mission 6's early retreats therefore did not preserve all withdrawn
troops when their home towns fell. The existing campaign-sweep documentation
labels detached-troop handling provisional. No capture, combat, health, economy
or difficulty policy was changed to obtain a campaign advantage. Remaining
detector signals need diagnosis on synthetic maps before further engine fixes.

## Reproduction and verification

**G** (proposed verification procedure). Use a fresh output directory for every attempt. For the private mission 6 route:

```sh
python3 tools/playthrough.py --game /home/adrox/aztec-re/game --mission 6 \
  --scenario work/playthrough/scenarios/mission-6.txt \
  --max-ticks 43200 --timeout 300 --idle-ticks 43200 --goal-ticks 1600 \
  --out work/playthrough/new-mission-6-repeat-1
```

**G** (historical report). Missions 7, 8 and 10 use a 16,000-tick budget, 300-second process limit and default
detectors. Mission 9's final bounded repetitions use 1,500 ticks and a 90-second
process limit. Its earlier 300-second attempt timed out while still running at
tick 4,187; later interrupted attempts have no complete reports and are excluded
from the repeated comparison. Reports retain process status, mission outcome, script
completion, scenario/binary hashes, anomalies and observations. Input manifests
also pin the mission files, rules and source revision privately. No periodic
captures were requested; these attempts provide no screenshot comparison.

**G** (historical report). Bootstrap with `--no-hooks`, the baseline Release build and pre-commit hooks
passed. The baseline and final full CTest suites each contain 74 entries: 68 pass
and six optional checks skip. Targeted path, engine, transport, framework and pathing
stress checks pass. Separate format and engine checks pass against individual
private installation copies with dummy SDL under `unshare -rn`. All pre-commit
hooks and the final whitespace check pass.

## Label audit corrections (2026-11-23)

Evidence checked: Reference: 0x409940 and capture rules do not certify these engine routes. Clone: `tools/playthrough_route.py`, `app/playtest.cpp`, `sim/path/navgrid.cpp`, `sim/game_sim.cpp` and `tests/test_playthrough_framework.py`. Historical route ticks/anomalies and before/after control hashes were not rerun; they remain G, not native measurements.

1. The mineral-blocked hall-upgrade and detached-troop transfer descriptions are historical: tasks 150 and the capture/service implementation changed those policies. The old failure is not a diagnosis of the current clone.

No original executable or reference runtime was run for this audit. Historical
validation totals retain their original dates; they are not this
audit's test results. Private captures, derived source and probe output remain
outside the tracked documentation.

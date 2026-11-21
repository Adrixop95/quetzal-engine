# Missions 11–15 player-command investigations

Task 115 is incomplete. No winning route for missions 11–15 is established by
these attempts. The framework and prior implementation work are present, but
[task 114's report](playthrough_missions_6_10.md) still marks its winning-route
acceptance incomplete. Independent investigations here do not complete that
prerequisite or the [full campaign run](playthrough_full_campaign.md).

## Method and private evidence

These attempts use task 112's player-command runner, the faithful profile,
unchanged mission inputs and ordinary selection, group and movement/attack
commands. Every launch uses dummy SDL video/audio inside `unshare -rn`, a separate
private installation with asset/map symlinks and ordinary Maps/Saves directories,
and an isolated user profile. No resources, ownership, health, mission conditions
or outcomes are injected. No original executable or reference runtime was run.

The shared directory `/home/adrox/aztec-re/work/playthrough/` was absent and is
outside this worktree's writable scope. Private scenarios, observations, command
logs, copied profiles and input/rules/binary digests stay in this worktree's
ignored `work/playthrough/`. No mission coordinates, object identities, unit
positions, map fixtures or captures accompany this report. There were no requested
captures or screenshot comparisons.

The initial pilots used a 2,000-tick bound and a 90-second process timeout.
The fixed comparison uses three fresh processes per mission, a 1,500-tick bound
and a 180-second process timeout. All detector thresholds retain their defaults.
The longer player scripts contain later assaults and a victory checkpoint;
a bounded or aborted opening does not execute those later steps.

## Results

The anomaly totals include route assertion failures, unfinished goals and process
diagnostics. They are not counts of confirmed engine defects. The engine outcome
is retained separately from the runner's failure verdict.

| Mission | Repeated engine result | Tick | Anomalies per repetition | Remaining acceptance |
| --- | --- | ---: | --- | --- |
| 11 | Defeat; runner rejects incomplete route | 274 | 4 / 4 / 4 | Preserve the protected hero through the journey, obtain the town transfer and finish conquest. |
| 12 | Running; required troop selection exhausted | 1017 | 9 / 9 / 9 | Preserve or replace assault troops and complete enemy elimination. |
| 13 | Running; required troop selection exhausted | 1031 | 74 / 74 / 74 | Correct the opening's selection mistakes, sustain the defence and complete elimination. |
| 14 | Running; required troop selection exhausted | 1041 | 99 / 99 / 99 | Preserve or replace the assault force and complete the longer battle. |
| 15 | Running; tick bound exhausted and route incomplete | 1500 | 30 / 30 / 30 | Complete conquest and the diplomacy/vessel sequence, then reach the natural victory. |

Every tick/hash pair, scenario/binary digest, outcome, terminal tick and anomaly
list matches across each mission's three repetitions. All observation sequences
are contiguous from tick zero. None of these fifteen processes reaches its wall
timeout. Repeatable failure is not a winning route or game parity.

Mission 11's journey loses the hero before its transfer checkpoint. This causes
the mission's natural defeat; the waiting checkpoint then fails and leaves the
scenario incomplete. Missions 12–14 stop at an empty selection during a later
assault. An exhausted troop type cannot receive that command. None of these
observations proves that a valid command was ignored or a mission is unwinnable.

The mission 13 opening also attempts to move two production-building types.
Five ignored-order findings trace to those immobile buildings, so they are route
errors. A separate corrected scenario excludes them: it still exhausts a required
troop selection at tick 1027, with 64 findings and the mission running. That
single diagnostic run is separate from the three-repetition comparison.

Mission 15's earlier 2,000-tick pilot exhausts its 90-second wall limit after
tick 1721 while still running. Its incomplete observation stream and findings
are retained privately. It is excluded from the bounded repeated comparison;
no outcome beyond its observed horizon is inferred. A separate opening save/load
control matches fresh-process continuation as described in the full campaign
report; that control also does not win the mission.

Goal deadlines and pending movement goals account for most findings on the
larger battles. The default 600-tick goal window can expire during combat or a
long approach. Missions 13 and 15 each report two stuck orders. Slow movement and
detector findings require a reachable-goal, casualty and combat audit before
attributing them to pathfinding. No new engine defect was isolated on a synthetic
map. The opening attempts do not exercise complete
economy, construction, training or mission-specific transport plans.

No engine behavior was changed. There is no diagnosed engine fix requiring a new
synthetic regression in this iteration. Existing provisional combat, capture,
AI, placement and transport policies retain their evidence limits; no new rule
or campaign-specific advantage was introduced to obtain a win. Remaining route
and movement investigations remain open rather than being labelled parity or
silently disabled in the detectors.

## Reproduction

The private scenario filenames use `mission-N-initial.txt`. For each mission,
replace the mission number and choose a new output directory for each repetition:

```sh
python3 tools/playthrough.py --game /home/adrox/aztec-re/game --mission 11 \
  --scenario work/playthrough/scenarios/mission-11-initial.txt \
  --max-ticks 1500 --timeout 180 \
  --out work/playthrough/new-mission-11-repeat-1
```

The fixed reports are under `work/playthrough/repeated/mission-N-R/`, where R is
1, 2 or 3. Each retains the runner verdict, actual mission outcome, tick,
anomalies, scenario/binary digests and complete tick observations. The private
input manifest also pins mission inputs, rules and the source revision. Keep
failed pilots and corrected-route experiments separate from the fixed comparison.

## Verification

Bootstrap with `--no-hooks`, the Release build with `AZTEC_GEN_DIR=` and all
pre-commit hooks pass. Baseline CTest has 78 entries: 72 pass and six optional
checks skip. The final build and full CTest run repeat these results. Separate
private-copy format and engine tests pass with dummy SDL
inside `unshare -rn`. Synthetic campaign-flow and save-continuation results are
described with their limits in the full campaign report. These checks do not
establish any of the five required winning routes.

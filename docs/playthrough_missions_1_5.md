# Missions 1–5 player-command playthroughs

This task is incomplete. Only mission 5 currently has three winning repetitions.
Failures and bounded experiments below are not winning campaign coverage or
original-game parity. The original executable and reference runtime were not run.

All attempts use task 112's route runner, the faithful profile, normal player
commands, dummy SDL video/audio, `unshare -rn`, separate private installations and
isolated profiles. No mission scripts, resources, starting state, ownership,
health or outcome are changed by the routes. Private scenarios, observations,
captures, command diagnostics and reports remain in the ignored worktree
`work/playthrough/`. The shared private research directory is read-only here.
No real mission coordinates, identities, positions or fixtures accompany this
document.

## Results

Each row uses three repetitions of one fixed private route and a pinned engine
binary. Anomaly totals include failed route checkpoints and process diagnostics;
they are not counts of confirmed engine bugs. Earlier exploratory attempts are
excluded from this comparison.

| Mission | Result in each repetition | Tick | Anomalies per run | Remaining acceptance |
| --- | --- | ---: | ---: | --- |
| 1 | Defeat; route assertion fails | 5335 | 4 / 4 / 4 | Construction, mineral income, training, groups and a sortie are exercised. A winning defence or elimination route remains open. |
| 2 | Bound exhausted; mission still running | 4000 | 8 / 8 / 8 | The corrected expedition boards, crosses and unloads. Stalled pursuits and a winning conquest route remain open. |
| 3 | Capture checkpoint fails; mission still running | 1464 | 3 / 3 / 3 | The diversion/capture attempt fails. A complete economy/conquest route remains open. |
| 4 | Escort selection depleted; mission still running | 1261 | 3 / 3 / 3 | The corridor assault stops before bringing the protected hero through. A complete escorted route remains open. |
| 5 | Victory | 2088 | 0 / 0 / 0 | Surviving cargo is evacuated, with substantial casualties before boarding; this is not a complete rescue. |

Mission 5 uses idle/economy bounds of 6000 ticks for its bounded retreat; it does
not claim economic development or harmless inactivity from those settings. The
default movement, stuck-order and failed-path detectors remain enabled. Winning
checkpoints require surviving vessels and embarked survivors, excluding the
earlier false-positive retreat in which vessel destruction satisfied the mission
script. Every tick/hash pair matches across all three repetitions of each row,
including the unsuccessful rows. Engine and scenario identities also match.
Repeatable failure is not acceptance of a mission.

## Engine findings

**F-TRANSPORT-COMBAT — labelled fallback, native eligibility unmeasured.** The
existing rule tables deliberately assign zero maximum health to vessel
controllers. Ordinary automatic targeting, explicit attacks, direct hits,
projectiles and spatial blasts nevertheless treated these controllers as damage
recipients. This could destroy ferries and cargo on the first damaging hit, and satisfy
the retreat script without rescuing anyone. The fallback excludes zero-maximum
vessels from ordinary combat targeting/damage without inventing health. Scripted
destruction and passenger cleanup remain available. Positive-health synthetic
vessels retain ordinary damage semantics. The original target eligibility and
vessel destruction protocol still require a controlled reference measurement;
zero maximum health alone does not prove immunity. This fallback is not a claim
that ferries are invulnerable in the original game.

**Automatic pursuit across disconnected terrain.** Side-wide visibility could
select a land target without reachable firing ground, repeatedly initiating
failed searches across water. Automatic selection now checks reachable firing
positions with the attacker's movement class, weapon reach and collision radius.
Visibility alone does not establish reachability. Explicit player orders retain
their existing semantics; a bad route can still request an unreachable move.
This is an independent engine policy, not measured original target selection.

**Residual mission 2 movement blocker — not fixed.** The final repeated attempt
still reports four failed-path findings, two stuck-order findings and two
unfinished movement goals. A remaining ranged unit and siege unit retain attack
targets without progressing. The acquisition fix addresses disconnected target
selection; it does not establish that the mover reaches every selected firing
position. The remaining steering/pursuit cause has not been isolated on a
synthetic map, so this engine blocker and the task remain open. No teleport,
unstick modification, forced casualty removal or altered mission condition is
used to pass it.

Synthetic river tests cover disconnected land and a connected bridge. They
require acquisition only in the reachable control and no repeated failed path
searches. Transport regressions cover acquisition, explicit attacks, direct
damage, projectile splash, spatial blasts, ordinary damage recipients and
positive-health vessel controls. Existing transport destruction tests continue
to cover cargo cleanup. No real campaign map is used by these tests.

Replay semantics use version 11 after integration with develop's version 10
because the combined combat and pursuit policies change deterministic outcomes.
Earlier engine saves are rejected by the existing version check rather than
replayed with different rules.

The unsuccessful routes do not establish a pathfinding, economy or AI defect
merely because the player loses. Route errors, intentional waiting and detector
signals must be distinguished from diagnosed engine failures. Balance, night
visibility, acquisition policy and full transport parity remain unverified;
they must not be changed into campaign-specific advantages to obtain wins.
In particular, failed selection/capture assertions are route failures, and an
economy-stall signal without attempted economic activity is not evidence that
harvesting is broken. Missions 1, 3 and 4 still require route work rather than
unsupported claims that their defeats prove an engine defect.

## Verification

Bootstrap with `--no-hooks`, the Release build with `AZTEC_GEN_DIR=` and an empty
game-data test directory, and the full CTest suite pass: 66 passed, six optional
checks skipped. Targeted engine and transport regressions, all pre-commit hooks,
and `git diff --check` also pass. Separate private-copy format and engine checks
pass with dummy SDL under `unshare -rn`. Original observations and the remaining
mission acceptance stay open; a green synthetic suite does not complete the
campaign task.

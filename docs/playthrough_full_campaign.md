# Full campaign player-command playthrough

Task 115 remains incomplete. No continuous winning campaign from mission 1 to
mission 15 was run. Briefings, results, successor selection, intermediate saves,
loads and quitting/resuming have not been verified together on the real campaign.
No original executable or reference runtime was launched.

## Prerequisites in this checkout

Task 112's player-command framework and the implementation work from tasks 113
and 114 are present. Their winning-route acceptance is incomplete:

| Missions | Available evidence | Missing prerequisite for this campaign run |
| --- | --- | --- |
| 1–5 | The [task 113 report](playthrough_missions_1_5.md) records three wins for mission 5 and unsuccessful routes for the other four missions. | Winning routes for missions 1–4, and current-build validation of all five routes. |
| 6–10 | The [task 114 report](playthrough_missions_6_10.md) records three wins for mission 6 and unsuccessful routes for missions 7–10. | Winning routes for missions 7–10, and current-build validation of all five routes. |
| 11–15 | The [task 115 mission report](playthrough_missions_11_15.md) records independent bounded investigations. | Five complete winning routes; none is established by these attempts. |

These earlier wins are historical observations from their respective reports,
not routes rerun or chained here. The shared private directory
`/home/adrox/aztec-re/work/playthrough/` was absent. The checkout did not contain
the private task 113/114 scenarios. Neither public reports nor the forced-state
[campaign sweep](campaign_sweep.md) can reconstruct those player routes.

Independent mission investigations and synthetic checks were performed despite
the missing prerequisite. A full campaign was not replaced by fifteen direct
mission launches, forced victories, copied unlock masks or altered objectives.

## Existing flow coverage and its limits

The current automation has two additional limitations relevant to the requested
continuous run:

- `run_engine` exits after front-end selection when `--menu-script` is supplied.
  That interface tests a menu route but does not then play its selected mission.
- Gameplay `--script` input starts again when a mission is entered. It has no
  per-mission scenario manifest for chaining different private routes. Scripted
  victories also deliberately suppress campaign-progress writes.

These are inspected automation contracts, not newly observed game failures.
Bypassing their safeguards or writing completed-mission bits would not verify
quitting and resuming an earned campaign. A future continuous driver must retain
ordinary player commands, observe actual mission/screen transitions, select the
appropriate route and preserve the distinction between test profiles and earned
progress. The absence of that adapter does not prevent manual campaign play.

The existing synthetic checks passed in this checkout:

| Check | What it exercises | What it does not establish |
| --- | --- | --- |
| `campaign_flow` | Invented missions 1–15, briefing/objective screens, victory/defeat, retry and successor transitions; separate progress API persistence. | A continuous real campaign or progress earned by an uninstrumented player session. |
| `campaign_save_processes` | Fifteen invented missions, fresh-process command-log continuation, every continued tick/hash, camera/selection restoration and save/load confirmations. | Continuation of any of the real winning mission routes or intermediate campaign progression. |
| `campaign_ui` | Campaign numbering, progress, briefing and outcome-menu contracts. | Original UI, final cinematic or complete mission parity. |

The generated fixtures include ordinary script-triggered victories; the retry
control additionally uses a test simulation. None is a winning route on private
mission data. Green synthetic checks and deterministic engine runs add no
verified continuous campaign completions.

## Bounded real-mission save continuation

An independent mission 15 opening control uses normal player attack-move orders,
saves through Ctrl+S at tick 100 and continues without loading. A second engine
process loads that slot through Ctrl+L and continues without further orders.
Both processes use a private installation, the faithful profile, dummy SDL and
`unshare -rn`, with a hard wall timeout. No original save or forced state is used.

All 201 contiguous resumed tick/hash pairs, ticks 101–301, match the uninterrupted
control. Both processes exit successfully, the saved file remains byte-identical,
and no campaign-progress file is written. The mission is still running. The
private scripts, observations, UI traces, save and process manifest remain under
`work/playthrough/save-control/`.

This is one bounded opening save/load control on the independent engine. It does
not finish the mission, exercise front-end slot selection, save between missions,
or resume an earned campaign with earlier missions completed. It adds no
continuous campaign completion or original-game parity evidence.

## Remaining acceptance

Recover or author the missing private routes, finish tasks 113/114, then win and
repeat missions 11–15 through ordinary commands. Pin the engine, rules, mission
inputs and route identities before chaining them. Revalidate older routes when
engine semantics change, especially transport and save replay.

The eventual campaign evidence must retain observed briefing, start, objective,
result and next-mission transitions; mid-mission and inter-mission save/load
boundaries; a process exit followed by front-end resume from an earned save; and
the final mission result. Compare resumed continuation with its uninterrupted
control. Keep all scenarios, profiles, saves, captures and transcripts private.
Diagnose engine failures on independently invented maps before changing rules.
No campaign-specific advantage or forced state is an acceptable substitute.

Current verdict: **no completed continuous campaign; prerequisite routes and
continuous automation remain unavailable**. This report records the boundary of
the work performed, not completion of task 115 or original-game parity.

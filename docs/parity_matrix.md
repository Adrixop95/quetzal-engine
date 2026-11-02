# Parity matrix

Counts of scenario results in the selected diff reports. Repeated runs count
separately. Pass means equality of the observed contract only; synthetic
passes do not establish original-game parity. Unreported scenarios are excluded.

| Layer | Scenarios | Pass | Fail | Skip |
| --- | ---: | ---: | ---: | ---: |
| L0 | 0 | 0 | 0 | 0 |
| L1 | 26 | 0 | 26 | 0 |
| L2 | 9 | 0 | 9 | 0 |
| L3 | 0 | 0 | 0 | 0 |
| L4 | 0 | 0 | 0 | 0 |
| L5 | 0 | 0 | 0 | 0 |
| L6 | 0 | 0 | 0 | 0 |
| L7 | 0 | 0 | 0 | 0 |
| L8 | 0 | 0 | 0 | 0 |
| Unknown | 0 | 0 | 0 | 0 |
| Total | 35 | 0 | 35 | 0 |

| Category | Results |
| --- | ---: |
| equal_observed_fields | 1 |
| skipped_missing_private_inputs | 1 |

L3 investigation (2026-11-01): the [movement iteration](rules_movement.md#10-l3-disconnected-selections-and-observation-budgets-2026-11-01)
adds synthetic coverage for disconnected group destinations and delayed order
budgets. All 48 existing movement configurations retain their hashes and metrics
(656 broad-radius arrivals, zero stuck). Reanalysis accepts 14 historical
open/gap reference runs; 105 runs fail unit matching and 11 manifests have no
position logs. The fresh corridor probe failed before runtime launch at namespace
loopback setup. These diagnostics are not selected paired scenario diff reports,
so the L3 counts above remain zero. Numeric arrival/path acceptance tolerances,
boat/flight and formation parity, and V02/V04/V05 dependencies remain unresolved.
Private evidence stays in ignored `work/parity/l3/`.
## L6 focused controls (2026-11-01; outside paired-scenario totals)

| Synthetic regression | Failing assertions before | Failing assertions after |
| --- | ---: | ---: |
| Fallback decoration requests | 17 | 0 |
| Decoration loop source lifetime | 2 | 0 |
| Simulation-owned fire duplication | 3 | 0 |

| Synthetic event-log comparison | Requests | Differences after |
| --- | ---: | ---: |
| Decoration request order and ticks | 37 | 0 |

| Isolated capture control | Attempts | Completed | Failed | Skipped |
| --- | ---: | ---: | ---: | ---: |
| Reference audio capture | 1 | 0 | 1 | 0 |
| Clone smoke, audio enabled/disabled | 2 | 2 | 0 | 0 |

| Live trigger class | Paired comparisons |
| --- | ---: |
| Selection | 0 |
| Orders | 0 |
| Combat | 0 |
| Construction/building | 0 |
| Production announcements | 0 |
| UI | 0 |
| Music | 0 |
| Ambient | 0 |
L4/L5 iteration, 2026-11-01 (partial task 52): the scenario totals above
remain unchanged because no aligned reference/clone bundle was obtained.

| Additional control / attempt | Count | Outcome | Evidence boundary |
| --- | ---: | --- | --- |
| L4 minimap edge/clip raster configurations | 8 | pass after fix | Four sizes/scales, two synthetic font paths; border containment and caller clip restoration |
| L5 scripted command-button trace control | 1 | pass after fix | Hover, selected rotation, target completion and repeated engine trace equality |
| Private clone capture probe | 1 | capture completed | Unpaired frame/trace only |
| Private reference capture probe | 1 | experiment failed | Namespace network setup denied before launch |
| Aligned reference/clone visual/UI scenarios | 0 | blocked | Missing shared runner, fixed-tick reference camera/UI observer and audited normalization |

The synthetic failures were reproduced before their fixes. These controls do
not establish original-game parity or supply per-region game mismatch rankings.
HUD, minimap, command/selection/resource panels, tooltips/fonts/cursors,
placement colors, construction stages, health bars and selection circles remain
pending aligned comparison. Private artifacts stay in ignored `work/parity/`.
## Windows readiness evidence (2026-11-01)

The Windows packaging/setup work adds synthetic checks, not reference scenario
results; the counts above are unchanged. Local Linux Release build and CTest
pass (34 passes, six skips), and relocated local package smoke passes inside
`unshare -rn` with dummy SDL. Coverage includes literal Windows settings paths,
case-insensitive resources, precedence, per-user save/load, dummy audio startup
and unavailable-device fallback, cursor policy and fullscreen transitions.
Synthetic staging also checks runtime allowlisting. No native Windows run,
real laptop devices or engine/reference comparison was performed. The release
workflow now requires extracted zip smoke on `windows-latest`; its result is
pending execution. See [engine_rules_status.md](engine_rules_status.md#windows-package-and-laptop-readiness-2026-11-01)
and [SETUP.md](SETUP.md#windows-package-quick-start) for boundaries and instructions.
## Campaign engine consistency (2026-11-01)

These observations are separate from the reference comparison counts above.
The [campaign coverage matrix](playthrough.md#campaign-fix-iteration-2026-11-01-partial)
records the current per-mission diagnostics and remaining dependencies.

| Independent engine probe | Missions | Matching paired transcripts | Matching save continuations | Victories |
| --- | ---: | ---: | ---: | ---: |
| Final 24-tick campaign sweep with AI and regroup orders | 15 | 15 | 14 | 0 |
| Final 1,200-tick campaign sweep with AI and regroup orders | 15 | 10 | 6 | 0 |

In the longer sweep, missions 1, 3, 4, 10 and 11 end in defeat; 2, 5, 6, 7 and 9
reach the tick limit; and 8, 12, 13, 14 and 15 exceed the 20-second process limit
in both runs. Wall-limited pairs have unverified hash/save evidence. Neither
sweep reports crashes. Save continuations use ticks 12 and 600, respectively.

Mission 11 ends in defeat before its tick-12 save. The short sweep has no crashes,
wall timeouts or static unreachable-objective findings; this does not establish
full objective reachability or exclude later hangs. Unsupported transport actions
remain in missions 2, 7, 9 and 15; mission 6 retains two missing-trigger warnings.
Flying-order acceptance and eligible-target selection have synthetic regressions.
The isolated reference recording control failed at namespace loopback setup
before runtime launch, leaving reference record/replay and campaign parity
unverified. All raw reports, orders and hashes remain in ignored `work/parity/`;
no L0-L8 result or completion route is inferred from engine self-agreement.
| experiment_failed | 35 |

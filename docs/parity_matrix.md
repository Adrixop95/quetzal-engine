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

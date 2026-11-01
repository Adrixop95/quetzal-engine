# Parity matrix

Counts of scenario results in the selected diff reports. Repeated runs count
separately. Pass means equality of the observed contract only; synthetic
passes do not establish original-game parity. Unreported scenarios are excluded.

| Layer | Scenarios | Pass | Fail | Skip |
| --- | ---: | ---: | ---: | ---: |
| L0 | 2 | 1 | 0 | 1 |
| L1 | 0 | 0 | 0 | 0 |
| L2 | 0 | 0 | 0 | 0 |
| L3 | 0 | 0 | 0 | 0 |
| L4 | 0 | 0 | 0 | 0 |
| L5 | 0 | 0 | 0 | 0 |
| L6 | 0 | 0 | 0 | 0 |
| L7 | 0 | 0 | 0 | 0 |
| L8 | 0 | 0 | 0 | 0 |
| Unknown | 0 | 0 | 0 | 0 |
| Total | 2 | 1 | 0 | 1 |

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

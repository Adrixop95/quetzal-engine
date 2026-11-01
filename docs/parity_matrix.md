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

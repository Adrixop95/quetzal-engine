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

L8 remains unmeasured. On 2026-11-01 a reference mission-launch preflight using
the supplied external installation/runtime, a private game copy, dummy SDL and
`unshare -rn` failed during loopback setup with a netlink permission error before
the game launched. This is not a completed AI scenario and changes no counts
above. Private inputs and failure output are under `work/parity/l8-launch/`.
Audited AI event observers and controlled passive-player/provocation scenarios
remain absent; no measured build order, expansion, composition, attack timing,
defence latency or retreat window was obtained. The numeric L8 comparison utility
and stronger twenty-minute synthetic skirmish checks provide no reference pass.

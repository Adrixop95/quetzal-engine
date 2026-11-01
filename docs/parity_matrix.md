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

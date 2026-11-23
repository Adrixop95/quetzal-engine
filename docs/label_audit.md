# Verification-label audit

Audit date: 2026-11-23. Scope: all 27 documents named in the private
`docs_without_labels.md` checklist, including its historical process/results
reports. Only documentation changed. Each document ends with its evidence map
and a numbered correction list.

V means an inspected source fact; its scope explicitly distinguishes reference
structure from clone construction. A V clone constant, parser offset or synthetic
assertion is not a verified native rule. M means a measurement rechecked from
retained observations, or an offline clone comparison against retained reference
data. No original executable or reference runtime was launched. G covers guesses,
proposed measurement settings and historical claims that this audit could not
corroborate. Historical results were retained rather than silently rewritten as
current measurements.

A counted unit is one explicitly labelled paragraph, list item, grammar block or
whole table. A table label covers all its rows; a paragraph may contain several
related constants. Counts are therefore label counts, not individual-number
counts, code coverage or percentages of game parity. Legends, headings, executable
command examples and the final correction/evidence sections are excluded.
Unresolved meanings inside a V codec-layout table remain explicitly unresolved;
the label certifies storage, not the gameplay interpretation. Mixed historical
original/clone result tables remain G as a whole when the old clone run is missing.

| Document | V | G | M | Corrected | Result |
| --- | ---: | ---: | ---: | ---: | --- |
| [formats.md](formats.md) | 21 | 27 | 0 | 15 | Alpha/header, preview, AI resource scan, progress, result movies and stale format gaps corrected; old resource/image inventories remain G. |
| [ai_purchase_gate.md](ai_purchase_gate.md) | 0 | 28 | 5 | 1 | Retained early payment/phase samples rechecked; original long money/object checkpoints agree. Obsolete clone formula scoped to its revision. |
| [REWRITE.md](REWRITE.md) | 5 | 20 | 0 | 6 | Primary/secondary HP, tactic/animation storage, descriptor scope, factory kinds and claimed differential coverage corrected. |
| [101-faithful-placement-and-relaxed-helper.md](101-faithful-placement-and-relaxed-helper.md) | 0 | 17 | 0 | 1 | Old predicate/table retained as superseded G; task 150 is current authority. |
| [103-mine-audit.md](103-mine-audit.md) | 8 | 9 | 0 | 4 | Current synthetic table checked; frozen-rate removal result, placement authority and completion/admission clocks corrected. |
| [group_formation_rule.md](group_formation_rule.md) | 7 | 11 | 1 | 0 | Offline scores reproduced exactly; the implemented formation policy remains an unproven native candidate. |
| [unit_unstick.md](unit_unstick.md) | 5 | 11 | 0 | 3 | Clone bits, search/watchdogs checked; profiles, 60-tick tile-step and near-goal recovery branches corrected. |
| [object_types.md](object_types.md) | 5 | 1 | 0 | 0 | Factory/type metadata agree; packet-name inventory conservatively G; core type-36 codec mismatch recorded. |
| [multiplayer.md](multiplayer.md) | 17 | 4 | 0 | 1 | QLS1 constants checked in the independent protocol; native protocol compatibility remains outside those V claims. |
| [idle_tactic_walk.md](idle_tactic_walk.md) | 4 | 7 | 7 | 1 | Cached walking counts reproduced; normalization and CRT arithmetic read; later formation candidate linked. |
| [PLAN.md](PLAN.md) | 2 | 13 | 0 | 2 | Historical host goals separated from current independent engine; direct reference launch example removed. |
| [playthrough_missions_6_10.md](playthrough_missions_6_10.md) | 0 | 14 | 0 | 1 | Historical outcomes retained G; obsolete placement/capture diagnosis scoped to its snapshot. |
| [playthrough_framework.md](playthrough_framework.md) | 16 | 0 | 0 | 1 | Command, condition and detector tables checked as clone contracts; stale completion revalidation claim removed. |
| [type_stats.md](type_stats.md) | 1 | 1 | 0 | 3 | Secondary-HP column and field meanings corrected; raw word dump removed; snapshot numbers remain G. |
| [engine_saves.md](engine_saves.md) | 13 | 3 | 0 | 0 | Current versions, bounds and codec inspected; no numeric correction established. |
| [savegame_roundtrip_all_state.md](savegame_roundtrip_all_state.md) | 4 | 9 | 0 | 2 | Layout-4 acceptance, replay version and RNG inventory corrected; old sweep totals remain G. |
| [audio.md](audio.md) | 9 | 14 | 0 | 4 | Shared scene RNG and partial selection/order speech producers corrected; old mixed event table remains G. |
| [playthrough_missions_1_5.md](playthrough_missions_1_5.md) | 0 | 12 | 0 | 1 | Historical ticks/anomalies retained G; old replay and transport/capture scope clarified. |
| [group_formation_at_start.md](group_formation_at_start.md) | 1 | 8 | 4 | 1 | Cached cohorts and scalar repeats rechecked; original assignment unknown despite later clone candidate. |
| [object_fields.md](object_fields.md) | 13 | 0 | 0 | 1 | Codec offset/width tables checked; +0x3c corrected to secondary HP; unknown meanings remain unknown. |
| [determinism.md](determinism.md) | 6 | 2 | 0 | 5 | RNG, tick phase, save fingerprint and sound hash inventory corrected; native consumption still unresolved. |
| [render_integration.md](render_integration.md) | 0 | 11 | 0 | 1 | Historical missing-producer inventory scoped to its date and newer rendering/aircraft/ferry reports. |
| [playthrough_missions_11_15.md](playthrough_missions_11_15.md) | 0 | 11 | 0 | 1 | Bounded historical outcomes retained G; no native or winning-route acceptance inferred. |
| [parallel_training.md](parallel_training.md) | 5 | 8 | 0 | 1 | Modification constants checked; F9/Ctrl+F9 binding corrected; original concurrent training not inferred. |
| [AUDIT_2026-11.md](AUDIT_2026-11.md) | 0 | 30 | 0 | 1 | Historical task inventory retained G with explicit supersession; current remote CI not inferred. |
| [gap_recon.md](gap_recon.md) | 0 | 8 | 0 | 1 | Ranked early leads scoped historically; newer descriptor/weapon/aircraft integrations supersede them. |
| [frontend_parity_results.md](frontend_parity_results.md) | 1 | 7 | 0 | 1 | Historical image/trace totals retained G; clone scripting contract distinguished from missing paired reference evidence. |
| **Total** | **143** | **286** | **17** | **59** | 27 documents |

## Evidence and boundaries

The private 5,468-function decompilation and supplemental research were read
without running or publishing derived code. Direct checks included the Marine
constructor/save/step paths, base-world constructor, CRT generator, resource
scans, placement predicate, alpha blitters, factory and tick/result dispatcher.
The read-only factory metadata agrees with the published type table, including
its surprising aliases. Clone checks followed the corresponding core codecs,
rule loaders, simulation, optional helpers, protocol, audio/render producers and
invoked synthetic tests. Each document names its specific evidence and limits.
Static inspection cannot corroborate an old screenshot percentage, route outcome,
benchmark hash or timing capture; those claims stay G when retained evidence was
not available or not reprocessed.

Retained dense mission-1/2/3 startup observations reproduce the walking counts
and common-field repeats used by the idle/formation reports. The independent
`formation_probe` reproduces all published endpoint and tick-tile counts against
the private dataset. Retained mission-2/3 early queue/payment/phase diagnostics
and long original money/object checkpoints were rechecked; the long captures end
at 1600 and supply no tick-2000 original result. Private comparison output stays
under ignored `work/label_audit/`. These are bounded cached-data checks, not fresh
native captures or proof of the inferred movement/planning algorithms.

One cross-check exposed an out-of-scope implementation discrepancy:
`src/core/savegame.cpp:save_unit_kind(36)` selects Spider while the factory table,
`types.csv` and `type_anims.hpp` select Marine. The documentation preserves the
factory fact; this audit does not alter save parsing or claim its type-36 native
import behavior is verified. This needs a focused codec regression and fix.

Live placement, production, movement, audiovisual and full campaign parity still
need audited setup/input/identity/clock observers and aligned repeated captures.
Missing observers or unmerged acceptance work do not prevent this static label
and correction audit. Proposed live protocols are G; they were not executed.
Native Windows/macOS jobs and remote CI were not run locally.

## Validation

Baseline bootstrap (`--no-hooks`), Release configure/build in this worktree's
`build/`, full CTest and all-file pre-commit passed before tracked edits.
CTest: 84 entries, 78 passed, six skipped (five private-data checks and the
restricted UDP process check). Skips do not establish game-data/network parity.

Final Release build and full CTest passed: 78 passed, the same six skipped,
zero failures. All-file pre-commit and explicit checks of the new audit document
passed. A consistency check verified all 27 count rows, all 41 table labels, one
correction section per document, local links and the allowed changed-file scope.
`git diff --check` passed. Native Windows/macOS CI was not executed here.
Git metadata remains read-only; all changes are uncommitted. Protected roadmap,
experiment and parity-matrix documents were not edited. No assets, executable,
derived source, raw captures or private generated fixtures were added.

# Computer player completion matrix

Evidence reviewed: [24 phases](ai_phases.md), [tables M and W](ai_tables_and_light.md),
[purchase investigation](ai_purchase_gate.md), [assignment survey](ai_assign_structure.md),
[scoring and threat](ai_scoring_threat.md), and
[script codes and purchases](script_codes_and_ai_purchase.md).
The newer phase/table corrections take precedence over older hypotheses.
V means a verified static reference rule; G means an unresolved rule or an engine
adapter. Implementation and synthetic tests do not establish reference parity.
No original executable or reference runtime was run for this task.

`ai_target_score_implementation.md` (task 154) is absent from this checkout.
Target scoring already exists, but its native record-index discrepancy remains
open. This task completes independent production, wave and defence work without
assuming that missing dependency has landed.

## Skirmish priorities and implementation

| Priority / part | Before this task | Current implementation | Evidence boundary |
| --- | --- | --- | --- |
| Production gate | Implemented | One phase per idle town update; unaffordable FIFO head blocks strategy; payment at production start, no reserve | V; existing synthetic payment/allocation checks retained |
| Build-tree wait | Implemented prerequisite wait | Blocked FIFO head retains cash and prevents the tail from starting; restoring the prerequisite permits the independent money check | V ordinary prerequisite wait; G complete native filter markers and refresh |
| Computer scaling | Implemented | Existing normal/fast, outpost, computer 0.8 and all-enemies price/time branches retained and tested | V production factors; G global difficulty effects, no invented bonus |
| Science counter | Incremented before phase; no admission/completion lifecycle | Phase observes old counter; admission of ordinary buildings freezes at -1; cancellation/removal/completion resets to 0; increment follows handler | V; boundary, busy-town and simulation lifecycle checks |
| Economy requests | Farm/warehouse/mine preferences, no disable flag | Warehouse replaces farm after three farms when absent; failed primary falls back to mine; phase 7 disables failed economy requests; hall completion enables them | V selection/flags; G site ranking |
| Developed-town science | Cheapest building or directly offered prerequisite | Recursive missing-prerequisite traversal; persistent recommendation; general unlock ranking and seeded random ties; forced speed 1 and affordability test | V selection; G build-filter representation, see below |
| Expansion / specialization | Cheapest available development offer | Phase-4 serialization; explicit optional priority/preferences select the first unused race specialization, with 1 -> 2 -> 3 fallthrough and the race-1 final fallback; absent inputs retain a cheapest-hall-only fallback | V selection with supplied inputs; G native priority/preferences initialization, so autonomous selection is incomplete |
| Production allocation | Implemented | Reverse plan-entry scan; one request per idle eligible town; production town separate from reserved plan membership | V; existing outpost, shared-cash, cancellation and stale-generation checks |
| M, W and development rows | Implemented arithmetic | HP-normalized W; kill-time M; side maximum development; candidate row now walked from high type to low | V arithmetic/order; G prototype and weapon gaps |
| Attack composition | Implemented arithmetic, ascending candidates | Descending candidate order, including wanted-candidate and new-score ties; existing 256-entry / 32-reuse limits and batches retained; type 33 excluded only from the main pass | V; synthetic seeded composition checks |
| Attack launch | Also swept unrelated idle town troops into each wave | Only living completed plan members receive the destination order; no wave for an empty plan | V plan membership; G movement/tactic adapter |
| Plan completion | Economy request could postpone completed-plan release | Economy request and completion are processed in the same eligible phase-2/14 update | V; production still suspends later handlers |
| Threatened plan | Own record used with mode 0 | Phase 23 writes own target and uses mode 1, permitting nearby town troops; targets remain frozen in phases 16..23 | V mode/target; G tactic eligibility |
| Defence buildings | Cheapest offer | Increasing type order, `rand % candidate_count`, no affordability precheck; selected placement failure disables attempts; defence completion enables them | V selection/flags; G placement |
| Combat assignment | Existing engine tactic/combat systems | Preserved; no invented formula from structural survey | G global/local assignment arithmetic and reference scheduling |

## All 24 strategist phases

The numeric phases are implemented in `ai::step`; this table separates transition
coverage from incomplete native data/tactic adapters.

| Phase | Implemented transition / action | Remaining gap |
| --- | --- | --- |
| 0 | Idle -> 10 | Loaded side-plan arbitration; no runtime side plan is invented (V: fresh side plan is null) |
| 1 | Offensive plan -> 2, failure -> 11 | G native intelligence/tactic adapters |
| 2 | Economy request, allocate, launch completed plan -> 11 | G native tactic busy/readiness and launch movement |
| 3 | Positive counter: science -> 4; failure resets counter -> 13; nonpositive waits; zero-unlock science roots remain eligible | G initialization of outpost priority/preferences and filter marker representation |
| 4 | -> 13 | None in transition |
| 5 | Random defence -> 6; failure -> 0 | G site search; selected placement failure clears defence flag |
| 6 | -> 11 | None in transition |
| 7 | Economic request, clear economy flag when both requests fail -> 8 | G site search |
| 8 | -> 11 | None in transition |
| 9 | Specialized source with foreign non-allied target -> 1, otherwise 11 | G native record identity |
| 10 | Counter <= 100*(dev+1) and no recommendation -> 13, otherwise 3 | None in transition |
| 11 | Economy flag and harvest < 2*spend -> 7, otherwise 12 | G remaining income/descriptor fidelity |
| 12 | Defence disabled -> 11; enough defences -> 0; otherwise 5 | None in transition |
| 13 | Non-defence membership < 3*(dev+1) -> 15, otherwise 9 | G native membership representation |
| 14 | Economy request, allocate, completed plan joins town -> 9 | G tactic join/readiness adapter |
| 15 | Own-record mode-0 plan -> 14, failure -> 9 | G native own intelligence |
| 16 | Any foreign active object within radius 8 -> 21; otherwise drop plan -> 0 | None in radius/transition; allies deliberately included |
| 17 | -> 21 | G whether this phase is reachable |
| 18 | Random defence -> 19; failure -> 16 | G placement |
| 19 | -> 16 | None in transition |
| 20 | Defence allowed and defence count < dev -> 18, otherwise 16 | None in transition |
| 21 | Non-defence membership < 7*(dev+1) -> 23, otherwise 20 | G native membership representation |
| 22 | Allocate without economy detour, completed plan joins town -> 20 | G tactic join/readiness adapter |
| 23 | Own target, mode-1 plan -> 22, failure -> 20 | G native tactic reuse predicates |

## Additional static checks and limits

A read-only instruction check of `0x45b940` during this task clarifies the older
"sum of depths" description: the recursive accumulator starts at zero and only
adds child return values; successful leaf returns are zero. No depth increment
occurs. Thus successful paths contribute zero to the general science score.
Traversal visits all missing prerequisites and its output leaf is overwritten by
each successful branch, leaving the last leaf. `0x45b720..0x45b743` confirms that
the unlock count is for the scanned root type, not the returned prerequisite.
The independent implementation uses these facts and rejects cycles in malformed
synthetic recipes. No executable-derived source or data is included.

The plan rows are filled in ascending type order (V), but the choice loop walks
the row backwards (V, scoring specification). The old clone and its tests conflated
these orders. This changes which blocked type wins the documented wanted-score
bug, random draw association, and equal-score candidate preference.

M returns raw 1 for building attackers, otherwise `255 - kill_time` through
kill_time 255, and raw 1/0 for slower or impossible building/unit victims. W is
`floor(256 * maxHP / largest_maxHP)`. The engine computes these from its immutable
rules on demand rather than retaining original process-global prototype arrays.
G: fresh building-prototype HP state, non-Marine mind HP, unresolved weapon getters
and native record indexing prevent claiming exact original M/W or target scores.

## Explicit remaining dependencies (G; do not guess)

- Outpost specialization still needs native initialization of the preference list
  and the combined priority corresponding to +0x190/+0x194. The planner accepts
  these as optional explicit inputs and hashes them; it does not populate them
  from an inferred economy score or default list. Tests supply invented inputs
  to verify ranking, race-local occupancy, no-free-preference failure and
  1 -> 2 -> 3 fallthrough. The autonomous missing-input fallback is G.
- Build-filter markers -2/-3, disabled entries, queued counts and hall-tree refresh
  have only partial engine equivalents. Science uses existing build-tree recipes
  and validated offers. Exact list reconstruction remains open.
- Loaded side plans and their save/load ownership are not represented by the
  fresh-game planner. Their existence is V; their reconstruction depends on the
  original-save/tactic adapter. No periodic runtime side-plan builder is added.
- Native tactic flags, scripted lock eligibility, holder/join/readiness lifetime,
  all-enemies tactic behavior, spatial placement and global/local combat assignment
  remain incomplete. Existing explicit-script preservation and autonomous recall/
  last-hit defence policies remain G.
- Task 154 target-score implementation is unavailable. Existing town-centre record
  positions, discovery/membership approximation and count indexing stay explicit
  gaps. This task does not adjust the score formula to fit purchase observations.
- Shared deterministic mission RNG is retained. New verified draws change later
  draws; original RNG consumption order and exact replay compatibility with old
  engine saves are not guaranteed. The save container format is unchanged.
- Global difficulty scaling remains G: no verified global difficulty value or
  bonus formula is available. Map `Difficulty` is editor metadata. The verified
  computer/non-all-enemies 0.8 price/time factor, all-enemies branches and
  harvest/spend speed choice already have synthetic coverage; these do not
  establish global difficulty effects. No HP, income or damage bonus is added.

## Synthetic verification

`ai_unit` includes a 7,200-tick invented two-computer skirmish requiring actual
building starts and completed structures, recruits and accepted attack waves on
both sides. Each side must earn harvest income in every 1,800-tick interval;
pending production must never remain unchanged for 1,800 ticks. Idle lanes after
successful reuse are legitimate. Every tick matches across
viewing sides and a fresh replay, including external pause/resume commands.
It also checks recursive recommendations, affordability equality, malformed cycles,
science timer boundaries and construction cancellation/completion, economy flags,
warehouse fallback, random defence selection, target freezing, threatened reuse,
wave membership and state hashing. A blocked-prerequisite FIFO head retains
cash and prevents its tail from starting, then resumes when the prerequisite is
restored; insufficient money still blocks until the exact price is available.
Existing synthetic 14,400-tick opposition and
per-tick replay checks remain required. These tests use no private game data.

Baseline: Release build, 77 passed / six skipped CTest entries, all-file pre-commit
passed. Final validation is recorded in `engine_rules_status.md`. Native Windows
and macOS execution and original-game parity were not measured on this Linux host.

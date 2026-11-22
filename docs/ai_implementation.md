# Computer player implementation

Evidence: [target scoring and threat](ai_scoring_threat.md),
[purchase phases](script_codes_and_ai_purchase.md), and
[combat assignment survey](ai_assign_structure.md). **V** means a reference rule
established by the cited static inspection, with independent synthetic coverage
where stated. **G** means an inferred engine adapter or an unresolved behavior.
No original executable or reference runtime was run for this change. Synthetic
determinism and replay are not measurements of game parity.

## Target choice

**V:** Every town update chooses the minimum score over known non-allied records.
In raw 24.8 units, distance is the octagonal town-to-target distance times 256,
divided by the octagonal map diagonal. Weights are maximum type HP divided by the
largest prototype HP, also in 24.8. The score adds half the mean unit weight and
half `min(unit count, 256)`. Empty records have zero force terms. For a computer
target that is not hostile to everyone, only distance is multiplied by three.
Integer truncation occurs at each fixed-point operation. Buildings do not enter
the unit mean or count. Selection is reconsidered even while production is busy
and when a previously chosen record is still valid.
An existing plan retains its construction-time destination while the town chooses
its next target; both destination and recommendation participate in state hashes.

**G:** Records use mission town centres instead of arbitrary record-object
positions. The native count-index oddity described in the evidence is not
emulated: this implementation uses the requested per-type formula. Exact
MapExplorer discovery, record membership and shared side-update ordering remain
incomplete. Remembered intelligence retains the existing discovery policy.

## Production plans

**V:** The main pass visits enemy types from 89 down to zero. Candidate unit types
are visited in ascending type order, limited by the highest development level
of any town on the side. Type 33 is excluded. `M` is the existing fixed-point
complement of kill time plus one, with the documented building/unattackable
branches. New candidates compete with `(M[t][e] * W[t] >> 8) + (rand & 63)`;
equal new scores retain the earlier candidate. Reusable candidates compare raw
matchup values, accepting equality, without a random draw.

**V:** Need is `2 * count[e] * M[e][best] - sum(M[k][e] * picks[k])`, including
picks against earlier enemy types. A nonpositive need adds nothing. A batch is
`rand % (max side development + 1) + 1`. Entries continue within a batch while
need is positive **or** the plan count is below the target's total count; thus
the final batch may exceed that total. The next main iteration stops at the
total. Storage always stops at 256 entries. Reusable stock stops at 32 per type;
exhaustion reselects a candidate rather than issuing an unchecked new purchase.
The extra airborne-target pass uses a 0..31 perturbation and one entry per
enemy, independently of the main total but within the same storage capacity.

**V bug retained:** At `0x44d701` the wanted-score bar stays zero. The last
unbuildable candidate whose positive score passes the current best-candidate bar
wins, even when an earlier unbuildable candidate scored higher. This changes
observable decisions in the synthetic fixture: weaker type 1 replaces type 0 as
the recommendation, and phase 10 takes the science path despite a short timer.
The implementation therefore preserves it. This demonstrates an effect in the
modeled build filter, not a measured campaign occurrence.

**G:** The build-filter output is represented by a missing prerequisite type.
Science resolves directly offered prerequisites and otherwise uses the existing
cheapest-development fallback; recursive science ranking is not recovered.
The extra-pass virtual predicate is represented by `flier`. Reuse preserves
explicit script tactics and locked plan members; autonomous detached members and,
in mode 1, town members provide reusable stock. Full reference tactic flags and
lifetimes are missing. The 25-tile test uses octagonal distance with inferred
town/member anchors. The isolated CRT-style RNG is deterministic; matching the
original shared RNG sequence and call order requires further work.

## Threats and money

**V:** A computer town below phase 16 enters phase 16 for one active non-allied
object of a non-computer owner inside a squared radius of 8 tiles for a developed
town, 4 for an outpost. Entry does not require visibility, damage, a force-count
threshold or a timer. It clears the town plan and cancels all its paid and unpaid
production, preserving other towns' queues. Cancellation uses the existing
partial-work refund path. Phase 16 checks for any different-side object within
8 tiles, including allied and computer objects; absence clears the plan and
returns to phase zero. Busy production still suspends ordinary phase handlers.
Production cancellation makes the strategist eligible in the entry tick, so its
single phase-16 handler immediately advances to phase 21 while the intruder remains.

**G:** Native recall eligibility cannot be expressed completely with the current
tactic model. Eligible autonomous side members receive a move toward the town;
script tactics retain their explicit orders. Existing last-hit attack dispatch
and idle reinforcement dispatch remain fallback combat policies. They are
separate from the proximity trigger and can respond to computer attackers.
Full global/local combat assignment from the structure survey is still open.

**V:** Unit plan allocation has no reserve or affordability pre-check. The only
unit money test occurs at production start; equality pays and succeeds. An
unaffordable queue head blocks later work in that town and its strategist.
Economic requests also queue without checking cash. Science pre-checks its price
at forced speed 1. Price/time factors already implemented by `production_value`
are unchanged: outpost 1/1.5; developed all-enemies 2/3 and 2; otherwise normal
1/1 or fast 2 and 2/3. The computer, non-all-enemies multiplier is 0.8 for both.
Binary32 constants and rounding after adding 0.5 retain the documented 160/53
result for a developed fast computer's base-100 item, without a lifted rounding
surcharge.

**V:** Development sums alive and queued ordinary-building weights, clamped to 9.
A construction represented in both objects and the queue counts once. Units and
defences do not raise development. Normal force, threatened force and defence
thresholds remain `3*(dev+1)`, `7*(dev+1)` and `dev`, respectively.

## Verification and remaining dependencies

`ai_unit` retains the existing scenarios and adds synthetic checks for the score
terms, target reselection, random scoring/ties, need and batches, cross-enemy
allocation, capacity/reuse/exhaustion, recommendations, airborne extras, threat
boundaries and side effects, queue blocking/payment, science, factors and caps.
`ai_skirmish` retains the 14,400-tick repeat/replay and sustained-activity checks.

**G / pending:** Exact intelligence, native tactic eligibility/assignment, shared
RNG ordering, recursive development/placement and loaded side plans depend on
models not complete in this checkout. These independent rules do not complete
those dependencies or establish original campaign timing, composition or parity.

Validation on 2026-11-22: Release build and the full 79-entry CTest suite completed
with 73 passes, six reported skips and no failures, including the unchanged
skirmish/replay assertions. All pre-commit gates passed. Separate format and
engine game-data checks passed with the private installation copy under
`unshare -rn` and dummy SDL video/audio. Two bounded mission-1 clone runs reached
100 ticks with matching hashes and no reported engine error or unsupported script
execution. Their `timeout` outcome denotes the requested tick horizon, not mission
completion. Original/reference execution and differential parity were not tested.

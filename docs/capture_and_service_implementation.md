# Town capture, facility service and effect sets

Evidence date: 2026-11-21. **V** means a reference fact established by the
existing static research, or an explicitly identified synthetic engine check.
**G** means an engine approximation or an unresolved inference. Synthetic tests
establish the implementation contract; they do not establish original-game parity.
No original executable or reference runtime was run for this task.

The newer [special mechanics findings](special_mechanics_residual.md) supersede
the defender and member-transfer interpretation in
[ownership_mechanism.md](ownership_mechanism.md). Production cancellation follows
[economy_residual.md](economy_residual.md), sections A.6 and F.

## Capture

**V reference facts:** a hostile, non-airborne candidate must have squared tile
distance less than 5 from the hall. Defence radius is 4 tiles at town level zero
and 8 when the specialization field is nonzero. Live units, including unarmed
units, boats, Johnka and aircraft, defend within the inclusive squared radius.
Ordinary buildings never defend; Defence objects do, including the unarmed
Shipyard. An active Defence construction also blocks capture, even outside the
radius. There is no hold timer. Small-pool scans accept allied defenders; the
spatial branch accepts only the owner's defenders. The branch changes above
`4 * radius * radius` objects.

**V implementation checks:** the engine uses those candidate, defender, distance,
branch and construction rules. The last eligible candidate in engine object order
wins. A successful check immediately changes the hall, buildings and towers to
the candidate's side. Existing town units retain their old side, leave the town
and join a receiving group. The waiting queue is discarded without payment.
The active trainee is discarded, including a materialized trainee object. Active
building work retains its HP, progress, remaining work and rate for the new owner.

**V refund exception:** computer-to-human capture cancels the active item and
returns its unbuilt fraction to the old owner, using the existing production
refund calculation. This also cancels an active building. All other capture
combinations discard active trainees without refunds and continue active buildings.
No side cash balance is transferred. Town speed resets to 1, repair membership
and mode reset, and old selection/control-group references to transferred or
destroyed objects are removed. Scripted town gifts retain their separate existing
policy; they are not routed through the capture-only member/production rules.

**G representation:** town `type != 0` stands in for the specialization field.
Numeric side-local receiving groups stand in for reference tactic pointers.
Retained units are stopped when assigned to that group. Engine object insertion
order stands in for reference list/spatial traversal, so competing capturers can
resolve differently. Nonremoved engine objects approximate the reference pool
count; the spatial defender branch still uses an exact squared tile radius.
The engine has implicit membership for standalone objects rather than the
reference's required non-null candidate tactic pointer. A full pointer-lifetime
model is outside this change.

**G production integration:** the engine can have multiple active lanes through
modifications; the same rule is applied to each active item of the captured town.
Continuing items retain their original payment metadata and pause state, and drop
old AI plan links. Their integer work rate is preserved, rather than recomputed
from the new owner's price or speed. Reference update order, computer-town alarm
cancellation before capture, rally/parent tactic resets, messages, voices and
minimap pings remain incomplete. The computer-town alarm can cancel production
before the hall becomes capturable in the original; this implementation only
adds the requested capture-time cancellation.

## Facility service

**V reference categories and implementation:** completed, live town facilities
activate service around their hall. Workshops 20/51/74 service machines:
vehicles, boats, Johnka, towers and ordinary buildings. Stables 15/44/77 service
non-soldier Marines: Knight, Bear, Jaguar, Bizon, Elefant and Snowman. The Marine
animal flag comes from the descriptor, not the organic damage category. Ordinary
soldiers receive no facility service. Air facilities 25/52/79 service airborne
objects only in the branch above 256 objects. A workshop or stable must also
exist; an air facility alone does not activate service. Allied live objects are
eligible even when they are outside the servicing town's membership.

**V branch contract:** at most 256 objects uses squared tile distance below 65,
with no airborne clause. Above 256 objects uses the 384-pixel half-size spatial
box and includes the airborne clause. Tests distinguish distance 64 from 65,
the box corner, and object counts 256/257. Objects beyond the box are excluded.

**V healing contract:** ordinary service sets the existing regeneration countdown
to 10. While positive, descriptor regeneration uses maximum primary HP as its
threshold instead of the natural threshold; the existing pre-add comparison can
overshoot. Buildings, Defence objects and Headtrucks instead receive an immediate
capped +10 HP and no countdown. Facility service neither spends side money nor
consumes the separate credit-funded repair/destroy budget.

**G scheduling and spatial fidelity:** the engine services facilities after its
object updates and capture check. A refreshed countdown is therefore 10 at the
end of a tick, with recovery starting on the following object update. Ten object
updates after the last service exhaust the window. Exact reference interleaving
is not measured. The large-pool spatial query is represented by an inclusive
pixel box around the hall; original spatial buckets, query caps and edge ordering
are not reproduced. Completion status is used for facility availability.

## Special objects and effects

**V existing resource facts, retained and regression-tested:** explosion set 152
has three sequences, with 11/10/10 frames. Debris set 113 has **20 one-frame
variants**, rather than a 20-frame animation. Each variant has a 250-tick resource
duration. Smoke set 105 has one sequence of 30 frames/ticks. These are transient
sprite effects, not gameplay-object spawners. The existing vehicle death path
chooses an explosion sequence and a debris variant, with debris height -1.
Tests check IDs, sequence counts, lifetimes, expiry and unchanged object count.
The names explosion/debris/smoke are descriptive **G** labels; the existing
`kShockwaveAnim` constant also selects set 105 for its established visual use.
This task does not invent new smoke emission timings or object spawning.

## Validation and remaining dependencies

**V synthetic coverage:** `engine_unit` checks all three races' facility IDs,
soldiers versus all six animal types, vehicle/boat/Johnka/building/tower service,
Headtruck overrides, aircraft gating, allied/enemy/dead eligibility, completion,
range branches, instant caps, ten-update expiry and replay hashes.
`hypno_ownership` checks every old/new human/computer capture combination,
continued building completion without another charge, queues, trainee destruction,
old-side receiving groups, speed/repair/selection resets, allied scan branches,
airborne exclusion, capture/defence radii and engine replay. Existing script gift
checks stay active. Effect checks use the existing engine death path.

The baseline Release build, all runnable CTest checks and pre-commit passed before
changes. Final validation results are recorded in
[engine_rules_status.md](engine_rules_status.md#capture-and-facility-service-2026-11-21).

**G / missing dependencies:** the audited reference adapter, aligned town/service
observations and full tactic model remain incomplete in this checkout. Their
absence does not block these independent corrections and synthetic tests. No
reference record/replay, synchronized HP/capture timing, visual parity or campaign
completion claim is made. Any clone-only private smoke result is separate from
reference verification.

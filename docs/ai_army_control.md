# AI army control and tactics (task 119)

Status: partial implementation; native measurements and side-plan allocation
remain pending. This task concerns army control, not task 108's purchase scoring.
**V** labels facts established by private instruction/code inspection. **G**
labels assumptions, engine substitutes and unresolved reference semantics.
Neither label claims a successful original-game run. All examples and tests use
invented worlds; no original scripts, assets, captures or recovered code are
included. The original executable was not run.

## Research and responsibility boundaries

**V:** Read-only research used the supplied private lifted `gen_*.c` files and
the private decompilation as a navigation aid. Instruction comments resolved
the condition callbacks and missing decompilations. Relevant entry points:

| Responsibility | Research entry points |
| --- | --- |
| Town strategist and force gates | `0x45dda0..0x45e770`, `0x45e7c0` |
| Capture/threat entry and notification | `0x45b1e0`, `0x45ece0`, `0x461a30` |
| Plan completion, activation and attachment | `0x457380`, `0x4573b0`, `0x457410` |
| Tactic lifetime, movement and notification | `0x44f5c0`, `0x454820`, `0x455570` |
| Placement dispatch | `0x45bd50`, `0x45bf10`, `0x45cd50` |
| Repair and removal bookkeeping | `0x45d270`, `0x45d4d0`, `0x45ed20` |
| Script conditions | `0x417d60`, `0x417dc0`, `0x417f60`, `0x417fd0` |
| Attacked-side getter | `0x408bd0` |

**V:** A town is itself a tactic with members. A production plan can own another
tactic, request recruits, activate movement toward a target, or attach its tactic
back to a town. A unit's original production town is therefore not a sufficient
description of its current membership. Side response iterates tactics, not a
flat list of nearby idle units.

**G / dependency:** This checkout's engine has object `town`/`tactic` fields and
a per-town planner, but no complete side-plan allocation model. Task 108's
[investigation](ai_purchase_gate.md) remains incomplete: shared requests, outpost
requests and offensive membership transfers are not implemented. The changes
here cover independent control-flow and script defects. They do not invent plan
composition, launch strength, target weighting or allocation to fill that gap.

## Town state machine

**V:** The strategist updates production speed even while production is busy.
It dispatches a strategy handler only with no active production object and an
empty queue. The science timer advances after that handler unless disabled by
the -1 sentinel. The normal branch is below 16; threatened processing occupies
16 through 23. The following table describes control flow, not recovered code.

| State | V meaning and successor |
| --- | --- |
| 0 | Wait for the side's active plan to clear, then 10. |
| 10 | With no requested type and timer within the development-dependent interval, 13; otherwise 3. |
| 3 / 4 | Request development; accepted request goes through 4 to 13. Failure resets the timer and goes to 13. |
| 13 | Live non-Defence town membership below `3 * (development + 1)` goes to 15; otherwise 9. |
| 15 / 14 | Create and progress a local plan. Completion attaches/releases it and goes to 9. Economy requests can accompany progress in 14. |
| 9 | A non-outpost source with a selected town belonging to another side goes to 1; otherwise 11. |
| 1 / 2 | Create and progress an offensive plan. Creation failure goes to 11. Completion activates/releases the plan and goes to 11. Economy requests can accompany progress in 2. |
| 11 | Enabled harvesting with income below twice spending goes to 7; otherwise 12. |
| 7 / 8 | Request economic construction; return through 8 to 11. |
| 12 | Disabled defence attempts return to 11. Defence membership below development goes to 5; otherwise 0. |
| 5 / 6 | Request/place a defence; success goes through 6 to 11. Placement failure cancels the reservation, disables attempts and returns to 0. |
| 16 | Nearby-threat check retains threatened processing via 21; otherwise release the plan and return to 0. |
| 17 | Continue threatened processing at 21. |
| 21 | Non-Defence membership below `7 * (development + 1)` goes to 23; otherwise 20. |
| 23 / 22 | Create and progress a threatened local plan. Completion attaches/releases it and goes to 20. Handler 22 has no economic detour. |
| 20 | Enabled defence attempts with Defence membership below development go to 18; otherwise 16. |
| 18 / 19 | Request/place defence; success goes through 19 to 16. Placement failure cancels, disables attempts and returns to 16. |

**V:** The force tests count live tactic members classified by the Defence
discriminator. They are neither a universal population cap nor a verified attack
wave size. Development counts eligible ordinary buildings and queued buildings,
clamped to nine. The source-town test in state 9 does not exclude an outpost as
the target. Ownership difference there is not an explicit hostility test.

**V:** Nearby nonfriendly presence can enter the threatened branch before damage
is dealt (`0x45b1e0`). Entry from a normal state notifies the side, releases the
current plan and clears production before selecting 16. A different enemy while
already in the threatened branch does not restart that entry sequence. Town
attack notification (`0x45ece0`) also passes through tactic notification and
computer-side response. These are distinct paths.

**G / engine comparison:** The engine retains its visible recent-structure-attacker
substitute for threat detection, its idle autonomous-unit membership filter and
its approximate integer production target. It does not yet implement proximity
entry, production cancellation, side-wide tactic redirection or persistent
offensive group ownership. A surviving nearby attacker recorded by `last_hit_by`
can keep its fallback alert alive; this is not evidence of the original's alert
lifetime. Neutral target planning still needs the side-plan dependency.
The engine also still advances the science timer before dispatch; purchase-phase
timing should be resolved with task 108 rather than inferred from these army tests.

**Engine changes:** A changed attacker no longer discards an existing threatened
production target. State 22 no longer diverts to economic construction. State 9
now applies the outpost restriction to the source town and allows outpost targets.
The existing hostile/known/live target filter is still G; selecting the first
known eligible town is also G. No distance, strength, visibility or RNG scoring
is newly claimed as V.

## Tactic state and combat control

**V:** The tactic constructor selects movement state 1 and balanced attack mode
3. The script idle condition tests movement states 0 or 1. State 2 waits for
member readiness before advancing to path processing state 3. State 3 progresses
route/member bookkeeping, handles delayed members and attached tactics, and can
consume queued destinations or return to idle processing. Empty tactics detach
qualifying children and can be released when references permit. Movement,
combat assignment, attack mode, script identity and lifetime are distinct state.

**V:** Attack modes are direct 0, defensive 1, aggressive 2 and balanced 3.
Periodic acquisition uses visibility, liveness, relation and mode gates. Tactic
notification records the notifying enemy side. Forced notifications and ordinary
scans differ. Combat assignment considers weapon eligibility and weighting;
ordinary buildings do not follow the normal global combat-candidate path.
See [rules_ai.md](rules_ai.md) for the established scan and mode facts.

**V:** Offensive plan completion checks plan readiness and its tactic's movement
status before activation. Local/threatened completion attaches back to the town.
This is not simply a counter reaching zero. Side defence response excludes
certain tactic categories/attachment combinations and invokes tactic attachment
operations. It has no verified flat ten-tile idle-unit policy.

**G / unresolved:** Exact readiness, attachment eligibility, combat retargeting,
formation settling and route abandonment are not reproduced by the current
engine. No inspected rule establishes a general low-HP army retreat percentage.
Returning a local plan to its town is not proof of such a retreat rule. Regroup
timing, survivor membership, return destinations and resumption of a cancelled
attack need the observations below. The engine continues individual orders and
resumes its ordinary planner cycle after an alert clears.

## Construction, repair, rebuilding and expansion

**V:** Placement dispatch distinguishes mobile production, ordinary buildings
and Defence. Ordinary building placement starts from the hall. Defence placement
can use an existing live defence as its reference before falling back to the hall.
Failed placement rolls back the queued object/reservation. Availability and
placement failure are separate decisions; temporary lack of gold alone does not
establish that defence placement must be permanently disabled.

**V:** Town removal bookkeeping updates build-tree membership and recalculates
development when an ordinary building is lost. Repairer processing, nearby unit
service and strategist processing are separate town update operations. Repair
does not imply resurrection of a dead object. Ordinary development and economic
requests can revisit missing capacity after destruction.

**G / engine comparison:** `sim/ai.cpp` searches within eight tiles using
`automatic_site`, economic resource scoring and deterministic tie order. Those
search bounds, spacing and scores remain substitutes, not a recovered placement
algorithm. Automatic repair already marks eligible damaged ordinary buildings
and consumes repair credits; this task does not replace that service algorithm.
The rebuild scenario checks a destroyed prerequisite being replaced through the
existing development/menu/construction path, not a remembered blueprint or an
exact-site rebuild promise. There is no new automatic hall resurrection rule.

**V:** Hall development changes an existing town's hall/build tree; capture
changes town ownership. **G / unresolved:** a separate autonomous new-town
founding/expansion policy, its placement, ownership arbitration and priorities
are not established here. The engine supports construction and capture but
does not gain a guessed campaign expansion schedule from this task.

## Mission-script interaction

**V:** `attackTown` names a target town by script identity and orders a tactic;
it is distinct from the town strategist's target selection. Towns and teams share
the script tactic namespace. Script idle tests tactic movement state; it does
not test the town strategist phase or simply the absence of nearby enemies.
An absent tactic returns false for idle, attacked and locked comparisons, even
when the requested comparison would match a default value.

**V:** `attacked` compares an attacking **side index**, initialized to -1, read
from tactic state. Notification stores the source side independently of source
object identity/lifetime. The engine previously compared a member's
`last_hit_by` object ID, causing conditions such as `attacked == 1` to depend on
unrelated object allocation. It now stores the side per script tactic, updates
it on combat notification, includes it in the state hash and reconstructs it
through replay. A surviving member retains the notification when another member
or the attacker dies. Later notifications replace the recorded side.

**G / engine boundary:** Positive hostile direct/projectile/blast damage supplies
the engine's notification event. Original scan/forced-notification eligibility,
neutral-side behavior and propagation through attached tactics remain incomplete.
Script `attackTown` remains an attack-move destination at the hall; it does not
gain the full original tactic attachment model. The engine idle approximation
still uses member orders plus pending paths, now rejecting an absent tactic.
Script lock/AI takeover arbitration and persistent pursuit of a moving or
captured town remain measurement work.

## Synthetic checks and verification

The new tests exercise engine behavior, with G setup/timings:

- A raid damages an AI hall; its autonomous defender receives a real attack
  order against the raider. Command-log replay reconstructs the result.
- An initially empty army produces several recruits and releases an autonomous
  wave. Observer events require actual members receiving accepted orders.
- An invented script destroys a barracks. The AI replaces it through normal
  planning, placement, payment and completed construction; development recovers
  and replay matches.
- A changed raider preserves threatened-plan progress; state 22 buys its recruit
  without an economy detour. Source and target outpost gates are distinguished.
- Script conditions distinguish side from object identity, preserve notification
  after deaths, ignore friendly damage and reject absent tactics.

Baseline: bootstrap with `--no-hooks`, Release build with `AZTEC_GEN_DIR=` and
`GAME_DIR=`, 69 CTest entries with 63 passed and six skipped. All-file pre-commit
passed using the existing read-only cache at `/home/adrox/.cache/pre-commit`.
A fresh in-worktree hook cache could not fetch GitHub due to unavailable DNS;
the existing cache avoided that fetch. Build outputs remain inside this worktree.
Final verification: Release build passed; all 69 CTest entries completed with
63 passed, six skipped and zero failures, including `ai_unit`, `ai_skirmish`,
`script_unit` and replay/determinism checks. All-file pre-commit and an explicit
check of this new document passed. A prior interrupted-build attempt could not
launch `test_save_import`; the complete sequential rebuild and suite above passed.
The skips cover private-data and restricted network checks, not verified parity.
Changes are left uncommitted because git metadata is read-only.

## Windows maintainer run sheet (needs live run)

All rows below are **G measurement requests**, not observed results. Use two
independent original-game sessions per row, then two engine sessions with the
same audited setup. Use a private installation/profile and keep recordings,
maps, saves, script bodies and detailed numeric streams outside the public repo.
Record build/revision, difficulty, modifications, relations, town identities,
seed if available, cash, visibility, initial membership and observation phase.
Use ordinary input and invented test maps; do not force AI phases or outcomes.
An unavailable observer field remains unavailable rather than inferred from a
screenshot. Windows native captures are maintainer work; Codex did not launch
the original or bypass the namespace restriction.

| Case | Controlled variation | Observe and compare |
| --- | --- | --- |
| Raid entry | Enemy approaches without firing, then damages hall/building/unit; normal town versus outpost; visible versus hidden; neutral versus hostile | First alert/phase transition, queue cancellation, side tactic responders, attacked-side notification |
| Reinforcement | Two raiders alternate hits while threatened production is active | Remaining plan, redirected groups, repeated-entry suppression, busy production |
| Wave assembly | Empty and prepopulated towns at several development levels; two towns share cash | Side/local plan ownership, recruit assignment, readiness, member transfer and actual launch |
| Targets | Two known towns at unequal distances/strength; target outpost; source outpost; neutral/allied targets | Selected town, scoring/tie behavior, ownership revalidation and target replacement |
| Alert clears | Raider dies, leaves radius, hides or becomes friendly | Threat exit, surviving unit orders, return/attachment, resumed production and cooldowns |
| Regroup | Losses before arrival and after engaging; obstructed paths; low-HP survivors | Group readiness, route retries, detach/return conditions; distinguish retreat from ordinary target loss |
| Placement | Block hall surroundings; leave a remote legal site; add a defence; vary resources and funds | Search origin/bounds, selected legal site, reservation rollback and retry/disable behavior |
| Repair/rebuild | Damage then destroy an ordinary prerequisite, economy building and defence separately | Repair marks/credits, availability loss, rebuild request and position; repeat with poor cash/busy queue |
| Expansion | Empty/captured towns, missing/destroyed hall, higher hall technology | Capture versus upgrade versus new town founding; allocation changes and resumed planner |
| Script conditions | Attack from side 1 using an object whose identity differs; destroy attacker/member; issue attackTown, path, lock/unlock | Attacked-side persistence/overwrite, idle/valid boundaries, attachment propagation and script/AI arbitration |

For each case retain both native repeats, both engine repeats, input history and
the first repeatable divergence. Capture every update around transitions if an
audited observer is available; visual wall-clock samples cannot establish exact
simulation ticks. Return aggregate V findings and unresolved G fields only in
public documentation. Do not claim army, campaign or all-fifteen-mission parity
from these synthetic scenarios. Task 108 allocation, full tactic membership and
native timing remain explicit prerequisites for that acceptance.

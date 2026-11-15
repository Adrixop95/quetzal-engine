# Group reach, idle acquisition and pursuit

Evidence: static inspection on 2026-11-15. **V** means a reference fact established
by inspected control flow or data, or an explicitly labeled clone construction
property; **G** means an inference or an independent engine approximation. Neither label reports a new live measurement. Codex did
not run the original executable. Private decompilation/lifted inputs remain
outside the public source tree; this document contains behavioral descriptions.

## Mode and distance contract

**V:** tactic virtual `+0xc` (`0x4516b0`) reads the mode at `+0x24`:

| Mode | Clone AttackMode | Group reach |
| --- | --- | --- |
| 0 | Direct | 4 tiles / 192 pixels |
| 1 | Defensive | 8 tiles / 384 pixels |
| 2 | Aggressive | 8 tiles / 384 pixels |
| 3 | Balanced | 6 tiles / 288 pixels |

The UI callback/string correspondence, constructor default 3 and mode setter
are described in [rules_ai.md](rules_ai.md#tactic-attack-modes). Campaign mode 3
therefore means Balanced. Reach belongs to a tactic, independently of its
members' weapons. Coordinates at tactic `+0x10/+0x14` are its tile anchor.
The reference compares tile displacements with its integer approximate norm
(`0x461fe0`), rather than a Euclidean pixel circle. Equality passes the inspected
reach comparisons. The pixel values above are unit conversions, not measured
pixel boundary thresholds.

## What the routines establish

- **V, global assignment (`0x451c30`):** collects up to 100 eligible members and
  enemy candidates, with a separate coarse enemy-tactic distance test below 25
  tiles. It scores damage and assignment load. Assignment through member virtual
  `+0xd0` is conditional on an anchor-to-target reach comparison for members
  whose virtual `+0xa4` is true; members with that discriminator false can receive
  the target without that distance test. The forced-target branch also contains
  this distinction. Thus the coarse scan bound and group reach are different.
- **V, local assignment (`0x452c70`):** checks attack eligibility and scores
  candidate kill times, friendly damage and assignment load; it can retain an
  existing target and writes through member virtual `+0xd4`. The inspected body
  has no direct call to tactic virtual `+0xc`. Calling both assignment methods
  simple nearest-enemy radius searches would overstate the evidence.
- **V, think (`0x454820`):** periodically revisits members and their current
  targets. For the `+0xa4` discriminator, it clears the combat target when its
  distance from the tactic anchor exceeds group reach. It also checks member
  distance from that anchor in the attachment/formation path. Its expanded scan
  bounds and randomized 15..23-tick refresh are separate from reach.
- **V, approach/follow (`0x453570`):** uses the reach in an anchor-to-anchor
  approach test, including the ferry request branch. It does not establish a
  universal six-tile enemy sight radius. See
  [ferry_control_flow.md](ferry_control_flow.md#update-group-reach-and-the-follow-order-second-reading).
  This entry is split into continuations in the supplied private analysis;
  its lifted instruction annotations were also inspected read-only.
- **V, member processing (`0x455b10`):** mode 0 suppresses combat following;
  mode 1 suppresses it while following a tactic path (state 3). In the combat
  branch, a member outside group reach is directed back towards its anchor.
  Candidate firing locations must satisfy both weapon distance to the target
  and group distance to the anchor. The inspected Marine vtable has `+0xa4`
  false, so its ground positioning path must not be confused with the
  discriminator's direct anchor-to-target cutoff.

**G, conclusion for idle ground units:** the leash limits the region in which
members seek firing positions. A target may lie beyond that region if a weapon
can reach it from inside. A useful outer acquisition bound is consequently
**group reach plus firing reach**, measured from the anchor. It is only an outer
bound: blocked terrain, tile discretization, weapon eligibility, visibility and
assignment can still reject a target. Group reach alone is not a universal
anchor-to-target cutoff, and weapon reach plus two tiles is not the leash.
Direct control can fire an available weapon without walking after a target.

## Independent engine integration

**V, construction property:** `GameSim` removes the fixed two-tile allowance.
Idle ground acquisition uses the mode reach plus the existing effective firing
reach (weapon distance and collision/target extents). Autonomous firing-position
search keeps destinations inside the mode reach from a stable idle anchor.
An idle target leaving the outer envelope is released; the member returns to its
idle origin. Direct mode retains its weapon-only acquisition and no-pursuit gate.
Explicit attack orders and attacks interrupting movement keep their existing
order/resume semantics. Movement completion and stop establish a new idle
origin; transport unloading and debug relocation reset it too. Anchor coordinates
and the idle-attack flag participate in deterministic state hashes and replay.
Without lighting inputs, acquisition uses the same bounded combat policy; it
has no separate fixed six-tile sight fallback.

**G, remaining approximations:** the clone does not yet own persistent reference
`tTactic` objects. Same-side, same-source tactic members use the centroid of their
stored idle origins; town members can share by town identity. Unassigned units
use their own origin. This prevents pursuit from dragging its own anchor forward,
but does not reproduce reference formation anchors, anchor changes on membership
changes, attached-tactic graphs or the town formation controller. Pixel-circle
geometry, tile-centre destination search, immediate release/return, nearest/lower-ID
selection and four-tick acquisition remain independent policies. The clone uses
its flier flag for the direct target-radius distinction; the reference `+0xa4`
discriminator still needs a class-by-class audit. Routes around obstacles may
leave the circle even when their firing destination is inside it. These are
explicit limits, not claims of reference trajectory or timing parity.

Synthetic checks cover the 6 versus 8 tile idle policy, a ground target just
outside the anchor radius, release and return after target escape, explicit
orders, shared side-local anchors, movement/replay and hashing. The sight test
retains PR 138's mission-1 regression: an idle Balanced Marine must neither select
nor chase a lit enemy 400 pixels away for 60 ticks. This synthetic setup protects
the early-rush failure mechanism; it does not verify mission completion. Defensive
units have eight-tile reach, so they must not inherit that Balanced expectation.
The fixed hash fixture changes because idle-origin state is now hashed.

## Live run sheet: pending, for the authorized external operator

Use the fixed-tick observer method in
[ferry_timeline.md](ferry_timeline.md), with the isolation procedure in
[EXPERIMENTS.md](EXPERIMENTS.md#shared-l0-observer-run-sheet-needs-live-run).
Headless in-game mouse clicks are relative and do not reliably select a group.
Use audited command packets or tick-bound private memory injection with readback;
label injected-state probes separately from ordinary command acceptance.

1. Audit the selected observer/runtime revision first. The supplied external
   binary may predate needed probes. Record runtime, map, rules, seed, input and
   observer hashes privately. Every process gets a fresh private game copy:
   immutable assets via symlinks, ordinary copied Maps/configuration/profile
   files and private Saves. Run with `timeout -s KILL`, `unshare -rn`,
   `SDL_VIDEODRIVER=dummy` and `SDL_AUDIODRIVER=dummy`. Do not bypass failed
   namespace/loopback setup. Codex must not launch the reference.
2. Prepare one ground member and one hostile target on open terrain, then a
   spread group, a town group and a flying member. Freeze AI/script interference,
   daylight, visibility, diplomacy and weapons. Read back mode `+0x24`, tactic
   state `+0x20`, anchor `+0x10/+0x14`, member tile, combat/preferred targets and
   actual weapon parameters. The existing L0 `anchor_tile`, `tactic_mode`,
   `tactic_state` and member probes provide part of this; target-assignment and
   input acknowledgements need an audited adapter. Missing probes are a dependency.
3. For modes 0..3, sweep target distances around 4/6/8 tiles and around each
   radius plus weapon reach. Include one-pixel offsets around tile edges, axis
   and diagonal placements, short/long weapons, and brightness 32/33 controls.
   Record actual scan, assignment, first movement, firing destination and shot
   ticks. Separate eligibility from temporary target-pointer assignment.
4. Let an idle group acquire, then move its target outward while keeping it lit.
   Observe both target distance and member distance from the unchanged anchor.
   Repeat with an obstacle, unavailable firing tiles, target death, mode changes,
   explicit attack, move, attack-move and follow orders. Test member joins/removal
   to establish when the real anchor changes. Do not infer the leash by measuring
   member-to-target distance alone.
5. Capture every tick through 240 around each transition, initially also retaining
   four-tick summaries for comparison with the ferry method. Repeat each frozen
   reference case twice in fresh processes and capture a matched clone case.
   Retain raw observations and failed attempts privately; compare identity,
   anchor, order, position and target timelines, not only aggregate counts.
6. Finally run mission 1 without orders through the early counterattack window
   twice per program. Track the original player cohort's modes, anchors, targets
   and departures, visibility and the triggering script event. Confirm that no
   distant lit enemy causes the initial Balanced cohort to march from its idle
   region. Exact timing, norm boundaries, group-anchor lifecycle, class gates and
   this campaign acceptance remain **needs live run**.

The existing tactic-model/shared input-observer dependencies are unfinished in
this checkout. Static research, the bounded clone policy and synthetic regressions
do not depend on them; live command/formation equivalence does. No live acceptance
result is claimed here.

Validation in this worktree: Release build, CTest (63 passed, six skipped) and
pre-commit on tracked files plus this new document passed. The skipped private
data and restricted network checks provide no new reference evidence.

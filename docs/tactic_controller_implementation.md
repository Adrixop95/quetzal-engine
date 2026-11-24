# Native tactic and group controller

**V** means a fact established by static inspection, or an explicitly identified
engine construction property. **G** means an independent policy, inference or
unresolved behavior. No original executable or reference runtime was run.
Private decompilation, lifted source and game data remain outside tracked files.

The task-153 controller is present in this checkout. This change extends it;
script movement and AI waves already enter `move_tactic`, rather than issuing
independent destination paths to every ground member. The prerequisite is not
missing. The remaining original town-tactic hierarchy and assignment arithmetic
are incomplete and are not prerequisites for the base-group changes here.

## Records, modes and hierarchy

**V, reference:** the base tactic constructor (`0x44f5c0`) creates a member list,
an enemy list, shared route storage and follower/waypoint lists. Its default
movement state is 1, and its combat mode is 3. A member record contains the
object reference, reported tile, readiness flag, individual-path flag, packed
short path and retry bookkeeping. The anchor is initialized by the first member;
removing or losing that member does not recenter it (`0x454090`, `0x4554f0`).

**V, reference:** combat mode, movement state and tactic kind are different
fields/discriminators. The inspected combat reach table (`0x4516b0`) has four
entries, indexed by mode at `+0x24`:

| Combat mode | Verified behavior | Tile reach |
| --- | --- | --- |
| 0 | Direct: no chase steps; listed in-range targets can still be fired upon | 4 |
| 1 | Defensive: no chase/local fire assignment while walking the shared path | 8 |
| 2 | Aggressive: can engage while the group walks | 8 |
| 3 | Balanced: member chase is allowed; group engagement is enabled when stopped | 6 |
| 4–6 | G: no combat-mode semantics established by the inspected code | Unknown |

**V, reference:** the separate tactic-kind discriminator identifies base group
0, town 1, ferry 2, submarine 3 and Johnka 4. Save dispatch (`0x4455f0`) also
handles kind 5, used for preplaced defences; this is not a fifth combat mode.
That dispatch does not establish a kind-6 controller. Boat movement states 0–6
belong to the transport controller, not this reach table. The engine continues
to reject combat-mode commands outside 0–3 instead of inventing three modes.
Transport and stationary decoration controllers retain their existing adapters.

**V, reference:** follow-and-merge (`0x452990`, `0x453570`) is same-side. The
source retains its membership until its anchor is strictly inside its own reach
of the destination anchor. Following groups replan after 36 thinks while moving.
On absorption, followers of the consumed group are redirected to the destination.
Hidden merging lost teams differ from visible joins: they can delay their parent,
inherit combat-mode changes (`0x454350`), and repopulate an empty parent.

**V, engine construction:** `UnitTactic::merging` represents pending absorption,
separately from `lost`. `script_join` and `merge_tactic` implement deferred visible
joins and the shared absorption path. A distant script join clears its previous
script path and combat assignment, then follows without remote membership transfer.
An already-close join is immediate. Visible joins cancel when their destination
vanishes. Hidden lost teams retain the previous empty-parent recovery behavior.
Mode commands traverse hidden merging descendants. The new state participates
in simulation hashing and script command-log reconstruction.

**G:** cycle rejection is a defensive engine policy. Pool/reference-count lifetime,
selection transfer, town-source join overrides, town-hall absorption and complete
hierarchical sighting/notification forwarding remain unresolved in this adapter.
There is no general native town-tactic representation yet; script town lookup
continues to use mission membership. Airborne and special transport joins retain
their existing adapters; the new join path handles ground groups.

## Per-tick movement, combat and return

**V, reference:** tactic think (`0x454820`) maintains enemies, advances the shared
anchor one route node at a time, waits up to 20 thinks for members/hidden teams,
and scans on its 15–22-think replan. Connectivity is a proximity flood around the
anchor, not a table of offsets. Each idle member asks for a tile step toward the
anchor, using its own speed. Disconnected eligible ground members beyond reach
become lost teams and follow back; merge distance is strict. See
[group formation and leash](group_formation_leash.md) and
[unit behavior implementation](unit_behaviour_implementation.md).

**V, engine construction:** player orders, script destinations/path segments and
AI `AttackMove` waves use this shared ground controller. Synthetic AI checks now
assert that released recruits share a walking tactic, in addition to existing
production, opposition and replay checks. Script idle and next-path dispatch read
tactic movement state, independently of a member's attack order. A script
can therefore observe a stopped tactic as idle while one member is fighting.

**V, reference:** global assignment (`0x451c30`) uses live hostile visible enemies
from listed tactics, with anchors strictly within 25 distance units. It computes
kill-time/danger scores and assigns chase targets. Local assignment (`0x452c70`)
reads the existing candidate table for a currently fireable enemy and retains
the chase target when it passes those checks. That preference is now implemented
and tested against a closer alternative. Mode and leash
gates apply to member movement (`0x455b10`); a ground target beyond reach is not
by itself released. Empty sightings clear automatic combat and members return
toward the persistent anchor. Damage forces enemy listing independently of the
script attacked notification.

**G:** nearest eligible enemies with lower-ID ties still replace the incompletely
recovered assignment arithmetic. Assignment
capacities, urgent/focus-driven anchor replanning, original RNG ordering and
readiness flags remain incomplete. The engine uses independent A* routing,
collision arbitration and deterministic scan RNG, as documented for task 153.
AI plan release still initializes its outgoing group through the existing movement
adapter; preserving the original plan tactic's entire lifetime remains open.

**V, reference:** inspected loss handling preserves the anchor/path of a surviving
group. Disengagement returns members toward that anchor. Neither fact establishes
a universal retreat at a particular HP or casualty percentage.

**G:** automatic low-HP retreat, threat-based destination selection and resumption
of a cancelled offensive wave have no established general rule here. The retreat
regression exercises the verified target-loss/return branch; it does not certify
an inferred morale or casualty system. Unknown rules retain the current movement
and planner policies.

## Validation

Invented, asset-independent tests cover shared walking, loss of the first member
mid-route, lost-team merge and stance inheritance, distant/immediate joins,
cycle rejection, follower redirection, vanished destinations, return after target
loss, AI wave membership and script replay during and after a join. The path
adapter tests stop the controller to model completion rather than modifying only
a member's order. No fixtures contain executable-derived code or game data.

The baseline Release build and all 79 runnable CTest checks passed; six checks
requiring local data or network process support skipped. Baseline pre-commit
passed.

Final validation on 2026-11-24: the worktree-local Release build passed. All 79
runnable CTest checks passed, including engine, tactic, script, AI skirmish and
replay checks; the same six checks skipped. Pre-commit passed for all tracked
files and the new document. Separate private format and engine checks passed
with dummy SDL inside `unshare -rn`, using a private installation copy with
immutable asset symlinks. An isolated mission-1 smoke completed 240 ticks with
zero script errors and unsupported executions. Its `timeout` status denotes the
bounded horizon, not victory or parity. No original executable was run.
Linux checks do not establish native Windows/macOS execution; the changes use
portable C++20 containers and existing controller interfaces.

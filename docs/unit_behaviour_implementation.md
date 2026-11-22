# Unit scans, chase and group walking

Task 153. **V** identifies a behavioral fact established in the supplied static
research, or an explicitly stated engine construction property. **G** identifies
an independent implementation choice, inference or remaining approximation.
No original executable or reference runtime was run for this task. The evidence
is [idle_autoengage.md](idle_autoengage.md) and
[group_formation_leash.md](group_formation_leash.md); these supersede the older
acquisition interpretation in [group_reach.md](group_reach.md).

## Discovery and combat

**V, reference:** a unit has no independent spatial scan. Even one unit belongs
to a tactic, whose persistent tile position is the scan and leash origin.
Every 15 to 22 tactic thinks the scan covers the bounding box of that position
and the member tiles, extended six tiles on the low edges and seven on the high
edges, including the far edge of the last tile. Alive, visible, nonfriendly
objects provide sightings. Kind-7 sources do not add enemy tactics. Unforced
sightings add enemy tactics in modes 0, 2 and 3; damage forces listing in every
mode. Global assignment collects eligible visible members of listed enemy
tactics whose anchors are strictly less than 25 tile-distance units away.
Local assignment asks the same table for an immediately fireable enemy when
reload is ready; it is not another spatial scan.

**V, engine construction:** `World::unit_tactics` owns anchors, membership,
scan countdowns, enemy lists, paths, waits and lost-team follow state. Numbered
tactics are side-local, including the engine AI's negative tactic numbers;
unnumbered units have singleton identities. Unnumbered mission placement groups
and multi-unit unnumbered movement selections receive separate internal group
identities. Moving a subset detaches it from unselected members and lost followers. Script-selectable tactic numbers are kept separate from these
internal identities. All controller state participates in simulation hashes and
command-log reconstruction. The former four-tick per-unit scan is removed.

**V, reference:** reach is 4/8/8/6 tile-distance units in modes 0/1/2/3. Distance
uses the integer octagonal approximation documented in the evidence, not a
Euclidean pixel circle. Mode 0 does not chase; mode 1 does not chase while its
tactic is walking. Other ground members outside reach step back toward the
anchor; chase steps must end inside reach. A ground Marine can retain a target
outside that region. Its chase target is released when dead, invisible or more
than 1152 pixel-distance units away. An empty scan clears automatic assignments
and members return toward the tactic anchor. Airborne target assignment has the
additional anchor-to-target reach gate.

**V, engine construction:** automatic ground attacks constrain member steps,
rather than dropping a target solely for being beyond reach. Damage reports
list the attacker's tactic independently of the script attacked notification.
Direct mode may fire an in-range listed enemy without walking. The mission 1
regression remains covered: a Balanced singleton does not start walking toward
a lit enemy 400 pixels away. Existing target-loss checks also preserve the
cancellation of an explicit attack's old pursuit movement.

**G:** nearest eligible enemy and lower-ID ties substitute for the reference's
kill-time/load assignment scores. The clone uses its existing side visibility
contract; the reference scan's lost side-map argument and threshold remain
unconfirmed. Without sight resources the synthetic engine permits visibility.
A disconnected-water acquisition guard remains an engine policy, pending reference
path-failure arbitration. The first scan runs on the first engine think. The timer
uses an independent per-tactic deterministic PRNG; matching the 15–22
interval does not establish reference RNG sequence or tick-phase parity.
Explicit focus attacks retain the engine's independent pursuit policy; tactic
focus/urgent path arbitration is not established here. Special transports retain
their own controllers. Unknown reference member flags, parent sighting forwarding,
assignment capacities and town-hall absorption remain open.

## Movement and detached members

**V, reference:** a group walks one path node at a time. The anchor is initialized
from the first member and is not recomputed when a member leaves or dies. Before
each advance it waits for members and hidden merging followers, with a 20-think
timeout. Members request individual tile steps toward the anchor, at their own
speeds. There is no speed matching or table of formation offsets in this path.
Connectivity is seeded near the anchor and extended through member-to-member
proximity. On the 15–22-think replan, disconnected eligible ground members beyond
reach are transferred to a lost team, which follows and merges strictly inside
reach. Members are neither teleported nor accelerated.

**V, engine construction:** player and script movement use one unsmoothed group
route and advance its tile anchor one node at a time. `MoveSystem::order_step`
sets a single neighbouring tile goal and then stops; it does not
allocate destination offsets or adjust descriptor speed. The controller uses the
anchor box and a 120-pixel proximity flood for connectivity. Disconnected ground
members beyond reach split on replan into a separate following tactic and merge
back when the anchor-distance test passes. Ordinary follow orders target the
leader tactic's anchor and moving followers replan after 36 thinks.

**G:** group routing uses the independent `legacy_astar` search, not WaveSearch or its two-slot
admission budget. This keeps member routes compatible with `order_step` terrain
and diagonal-corner checks and selects a reachable closest approach for blocked
or unreachable goals. Other movement controllers retain their existing searches.
Command destinations retain pixel coordinates while the shared path uses tiles.
The ready-to-advance test uses connectivity, disconnected movement and
anchor occupancy as a proxy for the incompletely traced member in-place flags.
Blocked member paths use the engine's own route search; the reference's packed
eight-step memory, exact disconnected-member goal and tolerance remain unknown.
The mover retains continuous collision avoidance and crowd recovery rather than
reference tile-exclusive reservations. The exact contact/stand-off approach point,
collision arbitration, lost-team
hidden arguments, excluded member flag and inherited mode semantics still need
reference validation. A lost team does not recursively split in this engine.
Enhanced crowd recovery may yield or exchange allied one-tile walkers and uses a
60-tick recovery timeout for one-tile requests (ordinary orders retain 120); this is
an engine fallback, not an original collision rule. Startup placement settling
still uses the existing independent mover formation helper; it is distinct from commanded group walking.

## Open evidence and validation

**G, unresolved:** `docs/data/auto_engage_tiles.csv` rows above seven tiles
(including bear 14 and mage 12) are not explained by the ordinary tactic scan
box. Shared member bounds, forced reports or another caller may explain them.
Do not turn these rows into per-unit scan radii without new evidence.

Synthetic regressions cover box edges, timer cadence, side-local tactics,
listed-team expansion, visibility, forced retaliation, leash boundaries, target
loss, one-tile stepping, anchor persistence, lost-team merge and replay hashes.
The original executable is prohibited in this session, so no live parity verdict
or reference record/replay result is claimed. Private game-data checks and any
namespace failures must be reported separately from synthetic verification.

Validation on 2026-11-22: the worktree-local Release build passed. CTest passed
all 74 runnable checks; six installation/network-dependent checks skipped.
Pre-commit passed for all tracked files and the three new files. Separate private
format and engine checks passed with dummy SDL inside `unshare -rn`, using an
ignored private game copy with immutable asset symlinks. A separate mission 1
headless smoke completed its 240-tick budget under the same isolation; its
`timeout` report is the bounded horizon, not a campaign victory or parity verdict.

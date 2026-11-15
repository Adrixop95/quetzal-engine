# Ferry shuttle controller

Task 129 replaces task 105's player-controlled ferry. Evidence sources are
[ferry_timeline.md](ferry_timeline.md),
[ferry_control_flow.md](ferry_control_flow.md) and
[transport_live_observations.md](transport_live_observations.md).
**V** means supplied original observations or inspected behavior; **G** means an
engine policy or an unresolved mapping. Synthetic checks establish engine
behavior, not full original-game parity. No original process was launched for
this change. Private Ghidra output and game data are excluded from the repository.

## Orders and boarding

A ferry (type 84) cannot be selected by clicking, dragging, control-group recall
or tactic selection. It has no selection ring, portrait command card, sail order
or unload button. The ferry remains pickable as a right-click target. Normal
`Attack` on an allied ferry means follow its controller; the legacy engine
`Board` opcode is an alias, and `Unload` is ignored. Johnka is a separate vessel
and no longer accepts the invented ferry boarding protocol.

**V:** the original generic right-click handler `0x447e00` gives a follow order
to an allied tactic whose kind is not 1. The ferry kind is 2. The field group
approaches the ferry anchor and requests boarding only when its anchor distance
is strictly less than its reach, the ferry is in phase 0, and the group is not
all-airborne. There is no scenario-squad whitelist or group-size rejection.
Town garrison groups ignore the follow order in the supplied measured timeline.

The tactic reach getter (virtual `+0xc`, `0x4516b0`) was decompiled locally with
`tools/ghidra_scripts/DecompAddrs.java`, following the recipe in
[weapon_class_map.md](weapon_class_map.md). The project was copied privately;
only the following factual result is published:

| Mode | Direct (0) | Defensive (1) | Aggressive (2) | Balanced (3) |
| --- | --- | --- | --- | --- |
| Reach in tiles (V) | 4 | 8 | 8 | 6 |

The integer distance approximation weights the smaller absolute tile offset and
corrects the larger offset; it is compared before shore placement. **G:** this
engine represents group anchors by the live member centroid. Its tactic model
has no independently updated original formation anchor or destination-controller
object. The requesting tactic identity is therefore retained as the destination
identity, scoped by side. Ad hoc selections without one shared tactic receive a
fresh deterministic tactic. Walking resolves the ferry anchor to a reachable
land cell within its 3 by 3 block, and retries when collision settling stops a
member early. This approach routing and the centroid remain movement-parity
limitations; they must not be used to explain the unmeasured ad hoc refusals.

**V:** local inspection of `0x408eb0` confirms an x-major, then y-major scan of
the ferry's 3 by 3 tile block. Each tile supplies one registered occupant. Only
live, non-airborne occupants belonging to the requesting tactic are transferred;
capacity is exactly **five**, independently of group size. **G:** continuous
engine movement can overlap occupants, so stable object ID selects the registered
occupant for a tile. Boarding closes when five have entered or the requesting
group has no remaining ground members. A mixed group's air members stay outside.
Extra ground members remain on land and can receive another order.

Cargo retains object IDs and health. Embarked passengers have a carrier link,
are hidden from picking, rendering, collision, vision, autonomous orders and
combat, and follow the ferry's position. Their original group identity is
restored when they land. Destruction/off-map cleanup destroys remaining cargo.

## Cycle and timing

Controller phase is independent of `transport_phase`, which selects the resource
animation. Finite animation handling cannot overwrite controller state.

| Controller phase | Action | Next phase |
| --- | --- | --- |
| 0 | Docked, ready for a group request | 1 |
| 1 | Opening, scan and accept passengers | 2 |
| 2 | Closing | 3 |
| 3 | Start pause, then straight diagonal travel until the next position fails the water test | 4 |
| 4 | Arrival opening, expanding-ring land placement | 5 when cargo is empty |
| 5 | Flip direction, turn pause, return along the same line | 0 at the stored dock |

**Exact V:** moving steps change each pixel axis by **five**, with opposite signs,
per ferry think. Capacity, scan dimensions, eligibility predicates and the reach
table are inspected constants. No pathfinder steers a sailing ferry.

**Fitted:** the start and turn pauses are **28 simulation ticks**, from original
samples spaced four ticks apart. Opening and closing each use eight ticks, fitted
to the sampled ramp transitions. These are deterministic fits rather than exact
resource-duration recovery. The engine runs at 12 Hz; the recorded original ran
at about 9 ticks per second. The Windows observations (departure about 12 seconds,
landing about 48 seconds, return 56–64 seconds) are contextual measurements, not
portable wall-clock acceptance thresholds.

**V observation / fitted placement:** the recorded passengers landed about
48–60 pixels from the arrival point. The engine starts ring radii at 48 and 60
pixels, samples twelve deterministic positions on each ring, and expands by
48 pixels thereafter. Each passenger uses its own small/large land predicate,
disc clearance and live-object collision checks. Candidates unsuitable for one
passenger can serve another. The exact original ring enumeration remains open.
No legal placement leaves that passenger aboard and retries in phase 4. Search
is bounded to cover the map. This bounded retry policy is G.

**G:** DAT zero is the current water predicate; it also includes walls. The
engine stores the initial dock and returns to that exact position. Scenario
`EndPosition=1` initializes the travel direction toward the south west, and
ordinary shore placements toward the north east; `Up` retains the visual
orientation. This interpretation follows the campaign endpoints and measured
mission-2 route, but its complete scenario-direction mapping needs live control.
Script entries can start within seven tiles outside the map and move along the
same diagonal to enter. Missing/far-outside controllers clean up cargo. A newly
blocked return route waits and retries (G); original behavior under changing
water passability remains unresolved.

## Script sites and safety policy

`boat.activate` starts phase **3** directly, including its travel animation and
pause. It does not fabricate a boarding request. Activation during a trip is
idempotent; another activation after docking starts another trip. All scenarios
use the same controller implementation, with no mission-specific route logic.
Inspection of the local campaign script inventory confirms:

| Mission | Activated ferry scenario IDs | Other relevant vessel actions |
| --- | --- | --- |
| 2 | 17, 18 (trigger 0, lines 5–6); 9 (trigger 3, line 64) | Submarine 0 |
| 7 | 6–10 (trigger 2, lines 30–34) | None |
| 9 | 9–12 (trigger 6, lines 70–73) | Submarine 30 |
| 15 | None; no `boat.activate` sites or placed ferries | Johnka 50/51 enable; submarine 21 |

The ordinary mission-2 shore ferries are not activated by those script sites;
they accept group requests. Handler acceptance does not establish that a normal
campaign route reaches a trigger, lands successfully or wins.

**G:** `kFerryInvulnerableWhileSailing` in `world.hpp` is a switchable compile-time
policy, default true. In phases 3 and 5 it suppresses explicit attack targeting,
automatic target acquisition and incoming projectile/splash damage. Cargo is
always excluded from independent targeting while embarked. Docked/loading/
arrival vulnerability is governed by existing combat rules. The supplied runs
had no enemies in range and did not measure ferry vulnerability.

## Persistence and verification

Controller phase, wait counter, ordered passenger IDs, requesting tactic,
destination tactic and side, direction, visual orientation, dock, current
position and carrier links participate in the state hash. Command-log saves
reconstruct them by deterministic replay. Replay semantics version is **12**;
older engine saves are rejected explicitly. Original-save transport import is
still incomplete and is not established by these tests.

`test_transport` generates a river map and checks the full cycle, second trip,
capacity overflow, all-air/town rejection, mixed siege/air membership, strict
reach gates, reverse-direction travel, class-specific blocked land placement,
incoming projectile immunity, player-control rejection and save restoration
while moving. Save continuation compares every tick through landing and docking.
`transport_playtest` uses invented archives and exercises the actual left-click
rejection and single right-click crossing through the game UI.

Release configure/build and the complete CTest suite passed: 63 runnable checks,
six optional game-data/network skips. All-file pre-commit and the new document's
hooks passed. Separate private-copy clone launches under dummy SDL and
`unshare -rn` accepted all 19 vessel handler sites and reached 120 ticks in
missions 2, 7, 9 and 15 with zero script errors or unsupported actions. These
short smoke runs ended at the tick horizon, with no campaign win verified.

## Decisive original run sheet: needs operator run

Codex must not launch the original executable. An operator should run the
following controls on the namespace-capable reference host using the private-copy
and record/replay procedures in [EXPERIMENTS.md](EXPERIMENTS.md). Do not bypass
namespace or observer failures. Keep all captures, packet streams, memory logs,
game files and normalized samples in ignored private output directories.

1. Prepare separate private mission-2 installations for **two independent runs**
   of each case: one foot unit, two foot units, three foot units, a mixed
   three-member foot/mounted/siege group, and the scenario siege squad. Add a
   town garrison negative control. Preserve identical settings and scenario state.
2. Create field groups through an audited `PCT_FRAME_CREATEGROUP` packet route,
   then submit the normal `PCT_FRAME_ATTACK` on the docked ferry. Record packet
   readback, membership and order tick. Headless gameplay mouse input is relative;
   successful menu clicks do not prove that gameplay `click`/`dragto`/`rclick`
   selected a group. Direct follow-field writes are a separate diagnostic and
   cannot establish the generic input path.
3. Sample **every completed tick** from before the order through docking: ferry
   phase (`tactic +0xf0`), requester (`+0xf4`), count (`+0xf8`), destination
   (`+0xfc`), ferry pixel position/direction; group anchor tile (`+0x10/+0x14`),
   mode (`+0x24`), flags (`+0x50`), follow mode/target (`+0x94/+0x98`), and fields
   `+0x9c`, `+0x9d`, `+0xea`. Record each member's tactic, airborne flag, alive
   flag and registered occupancy tile, including membership after embarkation.
4. Compare strict reach, readiness and all-air gates at the request tick, followed
   by occupancy membership at each boarding tick. Separate a request that never
   happens from an opened ferry with no registered candidates. Compare the ad hoc
   groups with the scenario squad without imposing a scenario eligibility rule.
5. Record opening/closing completion, first moving tick, each five-pixel step,
   passability stopping point, passenger landing coordinates and controller,
   direction flips and return completion. Repeat a second trip. Compare both
   independent runs before fitting any additional engine rule.

An audited group-creation packet driver and ferry-specific per-tick observer
remain prerequisites for this decisive experiment. The current L0 observer's
anchor fields alone do not expose the complete ferry phase/request contract.
The sheet deliberately does not invent a runtime environment switch or claim
that this experiment was performed.

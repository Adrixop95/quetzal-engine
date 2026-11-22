# Ferry motion, phases and submarine decoration

Task 159, 2026-11-21. This document supersedes the motion, landing and activation
policies in [ferry_shuttle.md](ferry_shuttle.md). **V** identifies a reference rule
established in the supplied research; **G** identifies an engine approximation
or defensive policy. Passing engine tests verifies implementation consistency,
not a live comparison with the original. No original executable was run.

Sources: [transport_boats_full.md](transport_boats_full.md),
[ferry_control_flow.md](ferry_control_flow.md),
[ferry_timeline.md](ferry_timeline.md), and
[ferry_eligibility.md](ferry_eligibility.md). The eligibility report corrects the
older tactic-kind and scenario-group hypotheses.

## Placement and motion

**V:** boats load at `X,Y` tile centres, with the `Up` direction bit and every
preloaded passenger hidden aboard. `EditorX`, `EditorY`, `EndPosition` and Paths
are not navigation inputs. Preloaded cargo does not contribute to the boarding
counter, whose five-passenger limit applies to the loading scan.

**V:** only boat state 5 moves. Direction 0 moves south west, direction 1 north
east: exactly five pixels on each axis per logic tick. There is no steering,
water test or stored home dock. When the horizontal distance to the current tile
centre is strictly less than five pixels and the next diagonal tile is walkable
for small units, the boat snaps to the current tile centre and becomes idle.
This same test ends both outbound and return legs. A line without a dock can
leave the map and lose the boat. `orientation` is the direction bit;
`ferry_direction` mirrors its signed displacement for existing diagnostics.

The implementation uses the shared Small navigation grid for the stop test.
**G:** its dynamic footprint updates and negative-coordinate floor division are
engine conventions; this task does not prove their equivalence to every native
grid update or signed coordinate conversion.

## Independent boat states and ferry phases

**V:** the animation slot is `direction * 5 + offset`.

| Boat state | Meaning | Offset / completion |
| --- | --- | --- |
| 0 | Dead / inert | No movement |
| 1 | Idle, ramp closed | 0, looping |
| 2 | Opening | 1, then state 3 |
| 3 | Ramp open | 2, looping |
| 4 | Closing | 3, then state 1 |
| 5 | Travelling | 0, looping |
| 6 | Turning | 4, then toggle direction and enter state 5 |

Open is accepted only from state 1, close only from state 3, and turn only from
state 1. Script travel overrides any boat state without toggling direction.

| Ferry phase | Condition and action (V) |
| --- | --- |
| 0 | Ready for a request; store requester and its side, enter 1 and ask to open |
| 1 | Wait for boat state 3; before scanning, close and enter 2 if five boarded or a nonzero boarded count has lost its requester |
| 2 | Wait for state 1; enter 3 and start a turn |
| 3 | State 1 asks to open; state 3 unloads, clears stored side/count, enters 4 and asks to close |
| 4 | Wait for state 1; enter 5 and start the return turn |
| 5 | State 3 asks to close; state 1 returns to phase 0 |

**V:** finite boat animations use resource 23 durations when available. **G:**
without sprite resources, opening/closing use eight ticks and turning uses 28,
fitted to the historical four-tick samples. The turn completes before movement
starts on the following tick. Object updates precede follow requests and ferry
phase processing in the clone; exact native ordering within a tick is not
established by the sampled timeline. Resource duration controls use invented
three-tick sequences to distinguish the asset path from the fallbacks.

## Requests, loading and passenger fate

**V:** ordinary field groups use the follow order. The request requires phase 0,
an anchor distance strictly below the mode reach (4/8/8/6 tiles), and at least
one ground member. There is no scenario, group-size or unit-type privilege.
Town controllers do not execute field-group movement. **G:** the clone retains
its centroid anchor, shore approach policy, and allied-target command adapter;
the exact native anchor, formation and enemy-boat click semantics remain outside
this change.

**V:** each open-ramp tick scans the boat tile plus/minus one, x first, then y.
Only a live, non-airborne registered occupant of the requester can board. Each
tile supplies one occupant, and at most five new passengers board. The departure
condition is checked before that tick's scan. Cargo retains unit identity and
health, but is detached from the requesting group and hidden from simulation
services that require presence. **G:** overlapping continuous engine objects
use stable object order as their occupancy-grid representative. The requester
identity is represented by its side plus tactic number.

**V:** losing/cancelling the requester with newly boarded passengers leaves them
alive and makes loading depart once the ramp is open. With a zero boarding count
(including a boat carrying only preloaded passengers), clear the
stored side and enter phase 0 if another follower remains, otherwise phase 5,
which closes the ramp before becoming ready. Remaining members of a full
requester retain their group and follow order. Boat destruction, a missing boat,
or a boat outside the seven-tile margin kills all cargo and cancels followers.
The existing synthetic ferry damage/invulnerability policy is retained as **G**;
this task does not establish whether ordinary combat can kill a native boat.

**V:** unloading creates a fresh, unselected base group in mode 3 (Balanced) on
the stored requester's side, or the boat's side for preloaded cargo. Search square
tile rings from radius one: top row left to right, bottom row left to right,
left column top to bottom, right column top to bottom, including repeated corners.
For each candidate cell, place the first passenger that accepts it. Units do not
rejoin their old requester, inherit its attack mode or keep a town affiliation.

**G:** group numbers are reserved at request time to prevent reuse of an emptied
requester number; a preloaded landing allocates its number on arrival. Placement
uses the clone's per-unit Small/Large predicate, disc clearance and live collision
checks. The ring search is bounded by map size and retries with blocked cargo
aboard in phase 3 instead of hanging. A repeated activation after the stored
landing side is cleared falls back to the boat's side. Native behavior for that
invalid-side edge case is unresolved.

## Script activation and submarine

**V:** `boat.activate` picks the last matching boat scenario, forces ferry phase
3 and boat state 5 in any phase, preserves cargo/request state, and moves in the
current direction immediately, without a turn pause or direction flip. Ordinary
requests still take both departure and return turns.

**V:** a submarine is a stationary decoration controller with no combat unit,
route, mover or passengers. Placement creates looping resource 27 animation 0.
`submarine.go` picks the last matching scenario, removes that loop, emits
non-looping animation 1 at the same tile centre and removes the controller.
The replacement disappears at animation completion. Submarine resource durations
are used when present; **G:** missing-resource synthetic runs use 28 ticks.
The idle effect follows controller presence rather than damaged-object fire rules.

## Validation and remaining evidence

`tests/test_transport.cpp` retains crossing/save-replay, capacity, blocked-shore,
eligibility, deterministic loading order, class-specific placement, combat policy,
reach/direction, and UI restrictions. Added controls cover exact state transitions,
turn timing and direction changes, immediate activation in phases 0..5, the
small-only next-tile dock test, tile-centre snapping, square landing order and
fresh group/side/mode, requester death/cancellation, off-map cargo death and
follower cancellation, mission placement independent of editor metadata, uncounted
preloads, and submarine loop/replacement lifetimes. Invented animation resources
exercise finite resource timings as well as absent-resource fallbacks.

Final verification (2026-11-22): Release build passed; CTest completed all 79
entries with 73 passes and six expected skips (five private-data entries and
restricted UDP processes). Pre-commit passed for all tracked files and this new
document. Separate private-data format and engine checks passed, as did all 19
campaign transport dispatch sites. The clone-only mission-2 headless smoke reached
720 ticks. Private-data checks used a private installation copy with immutable
asset symlinks, dummy SDL video/audio and `unshare -rn`; outputs stayed ignored
under `work/ferry159/`. No original executable was run. Synthetic tests and the
clone smoke do not establish live game parity.
Native animation appearances, dynamic shore updates, original placement overrides,
full formation behavior and normal-game boat vulnerability still need independent
evidence. No missing prerequisite prevents these transport changes; the existing
movement/identity approximations remain explicit G boundaries.

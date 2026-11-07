# Ferries and transports

Evidence date: 2026-11-07. **V** means read from the original instruction
comments in private lifted sources; **G** means an independent engine policy or
an unresolved assumption. No reference process was run. Static inspection and
synthetic checks do not establish original-game parity. No executable-derived
implementation or original assets accompany this document.

## Original findings

| Label | Function / instruction location | Finding |
| --- | --- | --- |
| V | `f_0043ec60`, `0043ec92` | The object removal path distinguishes type 84 using the type field at offset `0x50`. This is a vessel-specific branch, not a capacity constant. |
| V | `f_00408eb0`, `00408f75`, `00409041`–`00409055` | Ferry controller passenger count is at `0xf8`; loading stops at **five passengers**. This is separate from production queue capacity. |
| V | `f_00408eb0`, `00408fa1`–`00409055` | Loading scans a 3×3 tile area centered on the ferry. Candidates must be live, pass a virtual eligibility predicate and belong to the requested controller (`0x30`). This does not establish that every land type is eligible. |
| V | `f_00408dd0`, `f_00408eb0` | Loading records the requesting controller and opening state. Accepted passengers transfer to the ferry controller's member collection. They are not independent walking occupants of water tiles. |
| V | `f_00408eb0`, `004090ee`–`004093d9` | Unloading searches expanding tile rings around the arrival point, checks placement through the passenger's virtual tile predicate, then transfers passengers back to their destination controller. The counter and destination controller clear after unloading. |
| V | `f_00408eb0`, `004093e6`–`0040942f` | After unloading, the controller initiates a turn/return phase. Returning is a separate phase from accepting another boarding request. |
| V | `f_00408eb0`, `00409430`–`00409499` | Missing/out-of-range ferry cleanup iterates its member collection and invokes member destruction. It does not freely disembark everyone onto nearby land. |
| V | `f_00408770`, `f_00408670` | Ferry visuals have opening, closing, turning and travel states. Travel changes both pixel coordinates by five with opposite signs and stops at a tile-aligned passability test. Five pixels per visual think is not a measured engine tick speed. |
| V | `f_00402000`, `f_00408d50` | `boat.activate` resolves the boat controller by scenario identity, starts its activation state and requests travel. It is more than a UI ownership change. |
| V | `f_004020c0`, `f_00402120` | `johnka.enable` / `disable` resolve the Johnka controller by scenario identity and set/clear its enabled byte at `0xf1`. |
| V | `f_0042dce0` | Johnka has its own diagonal motion, turning and weapon-target processing. No passenger capacity for Johnka was established. |
| V | `f_00402060`, `f_0042fbb0` | `submarine.go` resolves its controller, dispatches the configured effect at the scenario tile center (animation 27, sequence 1), and retires the controller. The visible submarine route is an animation; a fabricated world-unit path is unnecessary. |
| V | `f_004375e0`, `f_00437720`, `f_004378a0` | The parser maps the three vessel action families to separate dispatch entries. Dispatch lookup is by scenario ID, not object index or local viewing side. |
| V | `f_00433db0` | Marine think uses state at `0x64`, movement data at `0x74`/`0x78` and `0x98`/`0x9c`, and target at `0x90`. Those fields alone do not define a general transport protocol. |
| G | Eligibility and approach | Siege/vehicle eligibility, exact approach orders and priority between different requesting groups still require a live control. The inspected virtual predicate was not resolved into a complete type whitelist. |

## Engine behavior

Transport is enabled by default. Select owned land units and right-click an owned
ferry or Johnka. They walk to the nearest reachable land tile in the vessel's
3×3 neighborhood. Units already within 20 pixels of that tile enter immediately.
Arrival determines boarding order; simultaneous arrivals use ascending object ID,
independent of selection order. The single `kTransportCapacity` constant is five
(V for ferries, G for Johnka). Extra units stop on land once capacity is exhausted.
With no reachable adjacent shore, boarding remains pending until the boat moves
near reachable land. Stop, move or attack cancels the passenger's boarding request.

G: All owned mobile ground types, including siege vehicles, may board. Air units,
structures, enemy units, disabled vessels and other vessels cannot board. Cargo
retains stable object IDs and health, is hidden from drawing/picking and independent
combat/AI simulation, and has no movement collision or vision contribution.
Cargo follows its vessel's position. This uses a separate ordered ID list and a
carrier link; `removed` plus a nonzero carrier denotes an embarked living unit.

G: A selected vessel sails on right-clicked water through the shared deterministic
mover. Water uses a separate movement class; DAT zero is the fallback water mask.
DAT zero also represents some walls, so this mask is provisional. Mixed land/water
orders split before destination assignment. Ground passability remains unchanged.
Player sailing speed currently uses the existing eight-pixel rule fallback.

G: Right-click nearby land to unload, or press **Unload** in the command card's
former Garrison cell. The selection summary shows count/capacity, and the info
overlay lists cargo IDs. Unloading processes cargo in boarding order. Candidate
land tiles within two tiles of the vessel sort by squared distance to the requested
landing point, then row and column; structures, terrain and passenger collision
radii block placement. Requests more than three tiles away do nothing. Full shores
retain remaining cargo; later requests retry it. This bounded policy differs from
the original expanding rings. The ferry remains controllable after unloading.
Destroying a vessel destroys its remaining cargo (V cleanup intent; G exact timing).

BoatActivate enables its owner's commands and starts entry toward EditorX/EditorY,
then attempts unloading. G: entry interpolates directly, including from outside
the map, at five pixels per simulation tick; it does not use ground pathfinding.
The loader retains the section's passengers, creates loaded passengers for
EndPosition=1 and preserves Up orientation. Section passengers retain their own
landing positions before activation when EndPosition=0. Repeated activation during
a route or after arrival is idempotent. Player orders can stop/replace entry.
JohnkaEnable enables ownership commands and weapon processing; JohnkaDisable stops
it. G: autonomous Johnka patrol/turning remains unimplemented; it can receive
normal player sailing orders. SubmarineGo starts the original configured animation
at its scenario point and retires its invisible controller. Animation rendering
needs the user's resources. A missing live scenario identity produces an English
message rather than silently incrementing the unsupported-action counter.

Cargo, carrier links, pending boarding, vessel enablement, route destination and
completion participate in the state hash. Board/Unload commands use normal
command-log serialization. Saves restore cargo by deterministic replay. Replay
semantics version is now 6, so earlier engine saves fail explicitly rather than
silently continuing with changed simulation rules. Original-save transport import
remains partial; this task does not establish original-save interoperability.

## Verification and site disposition

Synthetic CTest `transport` covers loading, capacity overflow, sailing, ordered
unloading, far ferries, no nearby free land, eligibility, vessel destruction,
script arrival with section passengers, the unload command-card click, cargo hash
sensitivity, replay and serialized save restoration. `transport_playtest` generates
its own river map and empty invented archives, then runs the actual selection,
boarding, sailing and unloading clicks from
[transport.txt](../tests/playtest/transport.txt). Neither needs original assets.

The private-data `test_transport --campaign PRIVATE_GAME` control independently
invokes each of the following 19 sites against its matching mission vessel and
checks that it starts/enables. **All 19 handlers accepted**. This deliberately
bypasses trigger conditions; it is not evidence that a normal campaign reaches
every trigger. The normal mission 2 headless report at 600 ticks has zero unsupported actions.
Its sites at lines 5/6 did not dispatch (false condition line 3); line 35 did not
dispatch (trigger 101 disabled, false condition line 33); line 64 did not dispatch
(false condition line 62). A separate 14,400-tick attempt exceeded its 90-second
process timeout and produced no report; it is a failed attempt, not a completed
campaign check. All private launches used dummy SDL,
`unshare -rn`, a timeout, a private game directory and isolated profile. The
reference runtime was not launched.

| Mission | Trigger | Line | Action | Scenario | Independent handler control |
| --- | --- | --- | --- | --- | --- |
| 2 | 0 | 5 | BoatActivate | 17 | Starts route |
| 2 | 0 | 6 | BoatActivate | 18 | Starts route |
| 2 | 101 | 35 | SubmarineGo | 0 | Starts animation; retires controller |
| 2 | 3 | 64 | BoatActivate | 9 | Starts route |
| 7 | 2 | 30 | BoatActivate | 6 | Starts route |
| 7 | 2 | 31 | BoatActivate | 7 | Starts route |
| 7 | 2 | 32 | BoatActivate | 8 | Starts route |
| 7 | 2 | 33 | BoatActivate | 9 | Starts route |
| 7 | 2 | 34 | BoatActivate | 10 | Starts route |
| 9 | 1 | 18 | SubmarineGo | 30 | Starts animation; retires controller |
| 9 | 6 | 70 | BoatActivate | 9 | Starts route |
| 9 | 6 | 71 | BoatActivate | 10 | Starts route |
| 9 | 6 | 72 | BoatActivate | 11 | Starts route |
| 9 | 6 | 73 | BoatActivate | 12 | Starts route |
| 15 | 1 | 18 | SubmarineGo | 21 | Starts animation; retires controller |
| 15 | 3 | 43 | JohnkaEnable | 50 | Enables owner commands and weapons |
| 15 | 3 | 44 | JohnkaEnable | 51 | Enables owner commands and weapons |
| 15 | 4 | 56 | JohnkaEnable | 50 | Enables owner commands and weapons |
| 15 | 4 | 57 | JohnkaEnable | 51 | Enables owner commands and weapons |

Final verification: Release configure/build succeeded with `AZTEC_GEN_DIR=`.
CTest passed all 59 runnable tests; six optional game-data/network checks skipped.
Pre-commit passed for tracked files and the new files separately. The final
isolated mission 2 smoke reached 600 ticks with zero script errors, unsupported
executions or unsupported sites. No winning campaign route was verified.

## Original measurement run sheet: needs live run

Run these on the namespace-capable reference host with two independent repetitions,
private installations and the record/replay procedure in
[EXPERIMENTS.md](EXPERIMENTS.md). Keep captures and native observations private.

| Unresolved measurement | Control and recorded result |
| --- | --- |
| Boarding duration | Record order tick, first approach tick, shore arrival and disappearance for 1, 5 and 6 candidates. Reverse selection and placement order. |
| Eligibility | Repeat with infantry, mounted units, each siege/vehicle kind, aircraft and enemy/allied passengers. Resolve the original virtual eligibility predicate. |
| Range and approach | Issue the same order with a docked, moving and distant ferry; vary disconnected shores and obstruction. Record approach destination and cancellation. |
| Boat speed | Record pixel position every original tick through both directions, including opening, turning and docking. Align simulation/visual think phases before fitting speed. |
| Unload spacing/order | Block nearest landing cells and then every nearby cell. Record exact passenger identity, tile and tick; establish ring traversal and whether it waits or expands. |
| Return and destruction | Record empty/loaded activation, post-unload travel, direct destruction and off-map cleanup. Check every passenger outcome. |
| Johnka | Resolve enabled versus moving versus firing states, initial Enabled and repeated enable/disable behavior; test whether boarding is meaningful at all. |
| Campaign reachability | Run ordinary winning routes for all 19 sites and record trigger prerequisites, dispatch/arrival ticks and ownership. Forced handler acceptance is only an engine control. |

Missing live observer/movement parity measurements do not block the playable
engine fallback. They still block claims of timing, route or campaign parity.

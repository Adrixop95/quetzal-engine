# Mission script runtime gaps

Audit date: 2026-11-22. This is a loader-to-runtime audit of `core/script.*`,
`sim/script_runner.*` and the vessel adapters in this checkout. **V** means a
statically established reference rule; **G** means an engine approximation or
unconfirmed integration. Synthetic tests establish implementation behavior,
not reference parity. No original executable or reference runtime was run.

Sources: [gift and command codes](script_gift_command.md),
[conditions and flow](script_conditions_flow.md),
[tactic lookup and AI purchase](script_codes_and_ai_purchase.md),
[later corrections](leftovers_scripts_descriptors.md),
[group controller rules](group_formation_leash.md),
[ferry eligibility](ferry_eligibility.md), and the historical transport-site
inventory in [campaign scripts](campaign_scripts.md#transport-follow-up-2026-11-04-needs-live-run).
Later V findings supersede earlier guesses. ROADMAP.md is unchanged.

## Actions parsed by the loader

Codes below identify reference behavior, not the independent enum values.
`supports()` means a dispatch adapter exists; it does not certify full fidelity.

| Code | Parsed action | Runtime disposition and remaining gap |
| --- | --- | --- |
| 00, 01 | victory, defeat | V first result wins while playing; only the mission human side ends the local session |
| 02 | attackTown | V first town-controller scenario match, ignore own town, use its hall tile; dispatch through the shared group controller. G town-specific launch and nested controller hierarchy |
| 03 | moveTo | V shared base-group controller movement, preserving native stopped/moving state distinction. G town-specific launch and full movement/controller equivalence |
| 04 | joinTactic | V both lookups required; plain groups follow and merge strictly inside reach, retaining source identity until arrival and redirecting followers. G town-source recruitment, nested controller lifetime and movement scheduling |
| 05, 06, 07, 08 | timer set, stop, start, kill | V lazy running-zero creation, set preserves pause, kill does not create. Existing timers advance before triggers; newly created/recreated timers first advance next update. Actions retain the one-update latch delay |
| 09, 0a, 0b, 0c | path variants | V waypoint progression uses group controller idle states instead of member orders. G exact native path/controller scheduling and ferry-specific setter behavior remain incomplete |
| 0d | boat activate | **Closed script dispatch:** V last matching ferry scenario, force phase 3 and boat state 5 in any phase, preserve cargo/request and direction, no turn; missing controller is a silent no-op. Full trip and loss controls use the shared task-159 transport state machine; see the transport boundaries below |
| 0e | submarine go | **Closed script dispatch:** V last matching decoration scenario, replace idle resource-27 loop with finite animation 1 at its anchor and retire controller. No route, passengers or combat; missing controller is a silent no-op. See the transport boundaries below |
| 0f, 10 | Johnka enable, disable | V one last matching live controller, independent of side; action changes only its enable byte, missing target is silent. G visual-think scheduling and complete patrol fidelity |
| 11 | buildCommand | **Unsupported execution.** V native parser `0x437fb0` accepts `buildCommand(integer) {UGI_name,...}`; handler `0x402180` builds a side plan from available type requests and replaces the previous plan only if requests were admitted. Native grammar is now parsed, retaining order/duplicates with a safe 32-request bound. The earlier engine-only `(UGI_name,integer,...)` syntax remains accepted. Both forms report unsupported execution; neither is silently dropped |
| 12 | moveCommand | **Closed:** V handler `0x4022e0` only calls a read-only nearest-town lookup and discards its result. Explicit supported no-op; no movement, queue or money changes |
| 13, 21 | camera activate, deactivate | V stationary side vision at the defined tile center, mask clamp 64..71, positive lifetime decremented after actions including creation tick, nonpositive indefinite, latest-source deactivation. Neither view position nor input changes |
| 14 | centerView | V tile-origin request with signed 32-bit pixel conversion; native lower-then-upper view clamp, horizontal margin 4 and vertical margin 1. Instant jump resets scroll velocity |
| 15, 16, 17 | setFriendlyTo, setNeutralTo, setEnemyTo | V directional relation changes |
| 18 | giftToSide | V distinct receiver group or shared town ownership transfer. G native follow/child hierarchy and full town queue/counter resets |
| 19 | printString | V 16-slot ring retains 15 messages, 4000 ms wall-clock deadlines, one expired retirement per second; oldest first at (10,20), 18-pixel pitch, one clipped line per entry. G native debug-overlay suppression |
| 1a, 1b | lock, unlock | V only reserved plain tactics are script-addressable; towns are fallback. G map Locked loading, empty-group lifetime and AI reuse integration |
| 1c, 1d | addMoney, takeMoney | V signed 32-bit wrapping arithmetic floored at zero, without C++ signed overflow |
| 1e | kill | V death without a damage source; G recursive child-group flags cannot be represented by flat group IDs |
| 1f, 20 | trigger disable, enable | V first matching ID, preserving frozen latch |

`printStringR` has no separate native action code: V the inspected parser
consumes and discards it. This intentional no-op is supported.

## Conditions parsed by the loader

| Code | Parsed condition | Runtime disposition and remaining gap |
| --- | --- | --- |
| 00, 01 | always, money | V implemented |
| 02, 03 | tactics, towns | G distinct living group IDs and completed halls substitute for native lists; empty groups and duplicate scenario identities remain incomplete |
| 04 | enemies | V hostile active sides; G activity inferred from objects/halls instead of native list counters |
| 05, 06 | units, unitsOfId | G independent living object counts; embarked living passengers retain identity |
| 07 | localTime | V StartTime clamped to 0..7199; advance before first and subsequent trigger updates on both tilesets, wrap at 7200. Startup phase closed |
| 08 | globalTime | V single-player completed-tick counter: first trigger evaluation sees zero; increment after evaluation, with signed 32-bit comparisons. Multiplayer lockstep is outside this adapter |
| 09 | timer | V lazy creation during actual short-circuit evaluation; diagnostic previews do not mutate timers |
| 0a | tactic idle | V missing lookup false; query group states 0/1 (inactive/stopped), independently of member orders; waiting/moving are nonidle. G native empty-controller lifetime and town/controller identity |
| 0b | tactic attacked | V persistent notifying nonfriendly side, initially -1; G combat notification adapter and native controller lifetime |
| 0e, 0f, 10 | isFriendly, isNeutral, isEnemy | V directional comparisons |
| 11, 12 | tactic valid, locked | V reserved lookup/town fallback and missing-object rules; G flat object membership and map reservation metadata |
| 13, 14 | unitsInRect, unitsOfUGIInRect | **Closed geometry gap:** V sorted tile corners converted to pixels, strict object-box overlap, no extra maximum tile, degenerate rectangles empty. Living runtime categories 0/1/6/7 only. G engine render height substitutes for virtual height offset; absent descriptors use common default bounds |
| 15 | defence valid | G independent defence provenance/type/scenario lookup; native object/controller identity remains incomplete |

Native condition codes 0c/0d (side plan complete / absent) have no identified
script grammar producer and are **not parsed**, so they are not silently ignored
loader output. General side-plan state and execution are still missing. All six
signed comparison operators are implemented.

## Trigger timing

Already implemented and retained: V every enabled trigger first executes its
previous true latch, then short-circuits the AND of its conditions. Initial latch
and empty conditions are false. Actions fire one update late and repeat every
enabled tick until the latch clears or the trigger is disabled. Self-disable
finishes the action list and evaluation; disabling freezes the latch. Re-enabling
can therefore execute a stale true latch before retesting. File traversal and
timer pool phase remain G. Existing synthetic controls cover these rules; new
rectangle controls also check delayed execution after an object leaves the box.

## The 19 historical transport sites

ROADMAP.md's task-25 row records an earlier unsupported-dispatch count. These
sites now have adapters; the twelve boat and three submarine dispatch sites
are closed against the inspected action contract. **None is certified unreachable
or fully equal to the reference during campaign play**. This table reconciles that count without changing the roadmap
or reproducing script bodies. Numbers are source locations from the existing
public inventory, not generated mission fixtures.

| Mission | Trigger | Action lines | Action | Current disposition |
| --- | --- | --- | --- | --- |
| 2 | 0 | 5, 6 | BoatActivate | Closed script dispatch; shared task-159 phases, loading, landing and loss rules |
| 2 | 101 | 35 | SubmarineGo | Closed script dispatch; stationary loop replaced by finite effect |
| 2 | 3 | 64 | BoatActivate | Closed script dispatch; shared task-159 phases, loading, landing and loss rules |
| 7 | 2 | 30, 31, 32, 33, 34 | BoatActivate | Closed script dispatch; shared task-159 phases, loading, landing and loss rules |
| 9 | 1 | 18 | SubmarineGo | Closed script dispatch; stationary loop replaced by finite effect |
| 9 | 6 | 70, 71, 72, 73 | BoatActivate | Closed script dispatch; shared task-159 phases, loading, landing and loss rules |
| 15 | 1 | 18 | SubmarineGo | Closed script dispatch; stationary loop replaced by finite effect |
| 15 | 3 | 43, 44 | JohnkaEnable | Patrol adapter; live route validation pending |
| 15 | 4 | 56, 57 | JohnkaEnable | Patrol adapter; live route validation pending |

Total: 12 boat, 3 submarine and 4 Johnka sites = 19. No campaign sites were
reported for JohnkaDisable, buildCommand, moveCommand or cameraDeactivate.
Synthetic tests cover dispatch even for forms absent from campaign scripts.

The ferry eligibility audit also exposed an extra alliance gate in command
dispatch, follow processing and UI routing. **Closed:** following/boarding a
ferry no longer requires friendship, and the activation byte does not gate a
request. Synthetic friend/neutral/enemy crossings preserve passenger ownership.
The existing capacity, all-airborne, reach, requester and landing tests remain.
G anchor centroids, hit-selection precedence, formation, engagement and native
transport timing still require broader controller/observation work.

## Dependencies and validation boundary

The general side-plan model needed by buildCommand and native plan conditions
is absent in this checkout. The town AI planner is a different abstraction;
queueing a purchase there would invent behavior. Native nested tactic lifetime,
reservation loading and full ownership/production reset semantics also remain
dependencies. The independent parser, no-op, geometry and eligibility fixes do
not depend on them. Unsupported build execution stays visible in inventory and
execution diagnostics, including dormant sites.

All new tests use invented scripts/worlds. Diagnostics tests now use the genuinely
unsupported build action instead of the verified move no-op. Replay semantics
advance to version 15 because rectangle and ferry command behavior changed;
older replay saves are refused by the existing version gate. No assets, private
scripts, lifted source or generated executable-derived data are included.

Validation completed on 2026-11-23: Linux Release configure/build succeeded;
all 83 registered CTest entries finished with 77 passes and six private-data or
namespace-dependent skips. Pre-commit passed for all tracked files and this new
document. Baseline build/tests/pre-commit also passed before changes. This
follow-up used synthetic inputs only; the campaign counts above come from the
existing inventory. Native Windows/macOS CI and live reference parity were not
verified in this session.

## Approximation follow-up (2026-11-24)

The [aggregate report](script_approximation_report.md) ranks all 255 baseline
approximate sites and records 239 remaining after the clock fixes. Codes 02,
03, 04, 09..0c and condition 0a have corrected base-controller behavior but
retain approximate dispositions for the explicitly listed integration gaps.
Trigger timing already obeyed the verified delayed, repeating latch contract;
no additional fired-once flag or startup action pass was introduced.

Static reinspection of `0x4184a0` confirms that both tileset branches increment
the same clock. The older tileset-zero-only reading was incomplete. Global
single-player time advances at `0x45864c`, after the trigger pool. Clock tests
exercise the first evaluation, action delay, wrap and invalid StartTime bounds.
Shared controller states and follow/merge state participate in hashes; replay
semantics advance to 16. Synthetic evidence is not live reference parity.
General side plans, town-specific recruitment/launch, nested native controller
ownership and exact path/member readiness remain dependencies. These were not
replaced with guessed successful behavior. The independent base-group fixes
and single-player clocks do not depend on those missing models.

## Transport script closure (task 180, 2026-11-25)

Task 159 is present in this checkout. All twelve BoatActivate and three
SubmarineGo sites dispatch through its shared implementation; no mission-specific
route, condition bypass or passenger spawn is added. The four JohnkaEnable sites
remain approximate, so the historical nineteen-site count must not be read as
nineteen newly verified sequences. The private-data campaign check now executes
each original action through a freshly latched ScriptRunner before checking the
controller, rather than invoking the vessel adapter directly.

Static inspection of the two native action handlers confirms silent missing-target
behavior. Repeated submarine actions may consume earlier duplicate controllers;
after the last controller retires, subsequent calls leave effects and messages
unchanged. Boat and submarine lookup namespaces remain separate.

**Transport boundaries (G):** group centroid/formation and shore approach,
continuous occupancy representatives, per-unit landing clearance, bounded blocked
landing retries, repeated-activation side fallback, missing-resource animation
fallbacks and exact within-tick scheduling remain as documented in
[ferry_implementation.md](ferry_implementation.md). Normal-play boat vulnerability
still lacks live evidence. Closing action dispatch does not close these shared
simulation gaps or the general count/tactic/rectangle condition approximations.

Invented scripts now cover a passenger group's boarding, loss of its old tactic,
hidden-unit counts, arrival rectangle, fresh landing group, submarine departure,
victory, boat return and command-log replay. Another script activates a preloaded
boat onto a dockless line and reaches defeat only after its passengers die.
Activation in every phase, duplicate scenario selection and missing/retired
controllers are checked through ScriptRunner. Existing eligibility, load-block,
ring-order and requester/loss tests remain enabled. No original executable or
reference runtime was run; no private mission fixture is published.
## Camera and interface follow-up (2026-11-25)

Static reinspection resolves the creation phase: script-pool updates precede
visual-object updates in `0x4577b0`; camera virtual `+0x120` enters `0x43edc0`,
which calls its `+0x60` lifetime update `0x402a50`. A duration of one therefore
expires on the activation update, before that update's vision calculation.
Repeated sources, per-side vision and latest-source tombstones remain intact.
Off-map camera definitions retain their coordinates instead of moving to an edge.
Signed pixel/mask arithmetic wraps explicitly; sight stamping widens intermediate
coordinates to handle extreme script positions without signed overflow.
Original-save camera reconstruction and exact fog/render equivalence remain
separate integration dependencies, not missing activation or input semantics.

Timer allocation through `0x401930` uses the pool's prepend operation
(`0x473f50`, `0x473de0`, traversal `0x4740d0`/`0x474110`). Lazy timers therefore
precede already-loaded triggers; timers created during traversal are visited
starting next update. Set/start/stop/kill preserve the documented latch delay.
The adapter's existing file-order traversal of triggers remains an independent
pool-order approximation; this follow-up does not claim to resolve it.

Johnka handlers `0x4020c0` and `0x402120` scan for one last matching controller
and only change its enable byte. Patrol startup now belongs to the patrol update;
repeated enabling does not teleport an active vessel. Missing vessel actions are
silent. Native controller-list ordering and visual scheduling remain transport
dependencies. On-screen print layout follows the draw walk near `0x4681a0`;
installed text uses native font slot 4 (glyph base 1200), palette state 0,
through the existing font adapter. Debug-overlay suppression remains unintegrated.

Synthetic scripts and a software-rendered message check cover these rules,
including headless view edges, ungated drag input, one-tick camera expiry,
foreign-side activation, timer kill/recreation and duplicate Johnka IDs.
Replay semantics advance to 17 for the changed camera and vessel behavior.
No original executable or reference runtime was launched.

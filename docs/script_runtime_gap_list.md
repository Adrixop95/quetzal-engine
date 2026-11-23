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
[ferry eligibility](ferry_eligibility.md), and the historical transport-site
inventory in [campaign scripts](campaign_scripts.md#transport-follow-up-2026-11-04-needs-live-run).
Later V findings supersede earlier guesses. ROADMAP.md is unchanged.

## Actions parsed by the loader

Codes below identify reference behavior, not the independent enum values.
`supports()` means a dispatch adapter exists; it does not certify full fidelity.

| Code | Parsed action | Runtime disposition and remaining gap |
| --- | --- | --- |
| 00, 01 | victory, defeat | V first result wins while playing; only the mission human side ends the local session |
| 02, 03 | attackTown, moveTo | G flat group attack-move; native controller states, hierarchy and target selection remain incomplete |
| 04 | joinTactic | Both lookups required; G immediate member-ID reassignment substitutes for following/merging and nested town/group controllers |
| 05, 06, 07, 08 | timer set, stop, start, kill | V lazy running-zero creation, set preserves pause, kill does not create; G timer-versus-trigger pool phase |
| 09, 0a, 0b, 0c | path variants | G idle-gated waypoint follower; exact native path/controller scheduling and ferry-specific setter behavior remain incomplete |
| 0d | boat activate | Adapter exists: forces departure phase 3 in current direction. G movement, formation, ramp scheduling and unload placement; no longer an unsupported dispatch |
| 0e | submarine go | V stationary effect controller, not a mobile combat unit: replace idle effect with departure sequence and retire controller. G effect scheduling; no longer an unsupported dispatch |
| 0f, 10 | Johnka enable, disable | Existing patrol adapter; G visual-think scheduling and complete transport fidelity. Disable is not an ignored branch |
| 11 | buildCommand | **Unsupported execution.** V native parser `0x437fb0` accepts `buildCommand(integer) {UGI_name,...}`; handler `0x402180` builds a side plan from available type requests and replaces the previous plan only if requests were admitted. Native grammar is now parsed, retaining order/duplicates with a safe 32-request bound. The earlier engine-only `(UGI_name,integer,...)` syntax remains accepted. Both forms report unsupported execution; neither is silently dropped |
| 12 | moveCommand | **Closed:** V handler `0x4022e0` only calls a read-only nearest-town lookup and discards its result. Explicit supported no-op; no movement, queue or money changes |
| 13, 21 | camera activate, deactivate | V stationary source, mask clamp 64..71, positive tick lifetime, nonpositive indefinite, latest-source deactivation. G creation/update phase, original-save mapping and visual parity |
| 14 | centerView | Tile-origin request; G independent viewport clamps and scaling |
| 15, 16, 17 | setFriendlyTo, setNeutralTo, setEnemyTo | V directional relation changes |
| 18 | giftToSide | V distinct receiver group or shared town ownership transfer. G native follow/child hierarchy and full town queue/counter resets |
| 19 | printString | V 16-slot ring retains 15 messages, 4000 ms presentation deadlines. G layout/encoding and native draw scheduling |
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
| 07, 08 | localTime, globalTime | V one unit per simulation tick, day wraps at 7200 from StartTime. G startup phase and native pool/lockstep integration |
| 09 | timer | V lazy creation during actual short-circuit evaluation; diagnostic previews do not mutate timers |
| 0a | tactic idle | Missing lookup always false; G all members idle with no pending route substitutes for native states 0/1 |
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
sites now have adapters; **none is certified unreachable or behaviorally equal
to the reference**. This table reconciles that count without changing the roadmap
or reproducing script bodies. Numbers are source locations from the existing
public inventory, not generated mission fixtures.

| Mission | Trigger | Action lines | Action | Current disposition |
| --- | --- | --- | --- | --- |
| 2 | 0 | 5, 6 | BoatActivate | Implemented adapter; live loading/route validation pending |
| 2 | 101 | 35 | SubmarineGo | Stationary effect adapter; live timing pending |
| 2 | 3 | 64 | BoatActivate | Implemented adapter; live loading/route validation pending |
| 7 | 2 | 30, 31, 32, 33, 34 | BoatActivate | Implemented adapter; live loading/route validation pending |
| 9 | 1 | 18 | SubmarineGo | Stationary effect adapter; live timing pending |
| 9 | 6 | 70, 71, 72, 73 | BoatActivate | Implemented adapter; live loading/route validation pending |
| 15 | 1 | 18 | SubmarineGo | Stationary effect adapter; live timing pending |
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

# Campaign sweep

Evidence date: 2026-11-07. **V** = inspected original instructions or private
mission data; **G** = engine policy or assumption. Synthetic tests and forced
state experiments are engine evidence, not measurements of the original game.
No original scripts, assets, numeric captures or executable-derived code are
included here.

All 15 unchanged mission scripts can now declare victory in the full simulation
with a forced-state harness. Separate controls reach every defeat branch and
collectively dispatch all **363 action sites / 92 triggers**. Every case was
repeated in a fresh process. This establishes script execution coverage, not
15 honest winning routes, complete action effects, balance or game parity.

## Inventory and blockers

The engine parser audited both private `Campaign/Maps` and `Maps`: 92 triggers,
129 conditions, 363 actions, zero unsupported condition/action kinds. Both
inventories agree. The older unsupported transport counts in
[campaign_scripts.md](campaign_scripts.md) describe earlier checkouts; all 19
transport sites already had adapters before this sweep.

C/A means condition/action sites. The provisional column identifies affected
sites, not extra unsupported grammar. All remaining condition families have
engine evaluators; their exact reference counting, timing and identity contracts
are not thereby verified. Camera duration/locking remains incomplete; the
alternate-language print is deliberately suppressed in the English UI.

| Mission | C/A | Unsupported C/A | Provisional or presentation-only sites | Blocker found and disposition |
| --- | ---: | --- | --- | --- |
| 1 | 18/30 | 0/0 | General combat/counting assumptions | Both victory alternatives and loss dispatch in separate controls. |
| 2 | 10/50 | 0/0 | Timer 3 C; ferry activation 3 A; submarine 1 A; camera 1 A | Victory watcher incorrectly started disabled because another trigger could enable it. Fixed initial enablement; countdown now reaches victory. |
| 3 | 3/2 | 0/0 | General combat/counting assumptions | No additional script handler blocker found. |
| 4 | 8/28 | 0/0 | Timer 1 C; gift 1 A; camera 1 A | Unset referenced timer never advanced, leaving a presentation trigger dormant. Referenced timers now advance. Embarked hero identity is retained. |
| 5 | 5/18 | 0/0 | Timer 1 C; vessel exit timing | Both removal and relocation of the required vessel type satisfy the exit predicate. Destruction also satisfies it; this is not proof of a successful evacuation. |
| 6 | 6/19 | 0/0 | Timer 1 C; town validity 2 C; join ordering | Reinforcement condition needs towns to change owner. Town tactic identity and capture fallback now permit the branch. Two disables name an absent trigger; retain the harmless data warning. |
| 7 | 10/36 | 0/0 | Town validity 3 C; ferry activation 5 A | Capture-based protection losses could not fire without town identity/ownership changes. All three now have separate controls. |
| 8 | 9/22 | 0/0 | Town gifts 2 A; camera 2 A | Town gifts previously found no tactic members. Transfer now updates buildings, members and town ownership. |
| 9 | 13/30 | 0/0 | Town validity 4 C; gifts 4 A; ferry activation 4 A; submarine 1 A; camera 5 A; alternate-language print 1 A | Town rescue gifts fixed. Full-simulation control waits for vessel entry/unloading, then forces rescue positions. |
| 10 | 4/9 | 0/0 | General combat/counting assumptions | No additional script handler blocker found. |
| 11 | 9/37 | 0/0 | Protected town validity 3 C; gifts 20 A; camera 1 A | Immediate loss at tick 1: protected town lacked tactic identity. Fixed placement and complete town gifts; both protected-object losses and victory dispatch. |
| 12 | 5/18 | 0/0 | Timer 1 C; gifts 2 A | Timed allied reinforcement gifts execute before the forced victory control. |
| 13 | 7/18 | 0/0 | Idle 2 C; path completion timing | Newly assigned paths appeared idle in the same tick and could be replaced immediately. Pending paths now count as work. |
| 14 | 10/23 | 0/0 | Path/join timing | Timed waves and joins dispatch before forced victory in the isolated runner. |
| 15 | 12/23 | 0/0 | Captured-town validity 3 C; Johnka enable 4 A; submarine 1 A; camera 1 A | Capture-dependent diplomacy requires town identities and ownership transfer. Both vessel-enable branches now dispatch in a capture control. Autonomous Johnka routes remain provisional. |

**Objectives and terminal reachability:** mission 2's disabled victory watcher
and mission 11's immediate protected-town defeat were concrete blockers before
these fixes. The missing timer in mission 4 blocked presentation, not its victory
predicate. No terminal branch remains structurally unreachable in the forced
controls. False conditions at the end of an ordinary bounded run are **unmet**,
not proof that an objective can never trigger. Textual briefing objectives are
not a separate machine-readable objective system; these results concern script
milestones and terminal actions, not a new checklist UI.

**Required objects:** every mission's placed types, implicit halls, vessel
passengers and submarine controllers load with zero unknown required types.
There is no campaign spawn-action grammar requiring an additional unit factory.
Heroes and special vessels are supplied by mission placement, not ordinary
production menus. Town members, implicit halls and newly produced town objects
now retain the town's script tactic identity. Startup formation indexing also
accounts for passengers and submarine controllers. This proves creation of the
initial required objects, not every legal player construction site or special
unit's combat behavior. Water masks, shore clearance, weapons, AI production
and balance retain their existing limitations.

## Original facts and implementation choices

- **V:** lifted `f_0045efe0` initializes triggers enabled;
  `f_00436ad0` / `f_0045f0e0` construct script triggers through it. Removed the
  speculative rule that disabled any trigger mentioned by an enable action.
  Startup disable actions and normal source order now determine gating.
- **V:** `f_00430770`, `f_004594f0` and `f_0044f5c0` establish town controllers
  in the tactic class hierarchy and assign their scenario identities.
  `f_00402d50` branches between ordinary tactic gifts and town transfers through
  `f_0045da00`. A town gift cannot be just an idle order to mobile troops.
- **G:** independent town transfer cancels/refunds donor production, stops its
  members, changes their owner and movement team, updates town ownership/service
  state and resets its AI planner. Queues do not continue charging the donor.
  Exact original queue, refund, research and detached-troop handling need measurement.
- **V:** `f_0045b1e0` checks nearby ground attackers and friendly defenders before
  invoking capture through `f_0045a920`. **G:** the fallback uses the nearest
  hostile ground unit within two tiles and friendly ground defenders within four
  tiles, resolving ties by placement order. It excludes aircraft and transports.
  Expanded alert radius, eligibility, active construction and tie ordering remain
  measurements. This supplies a usable capture path for campaign script guards.
- **V:** timer lookup `f_00401930` creates a missing timer; `f_00401a10` advances
  an unstopped timer. **G:** the engine eagerly initializes referenced timers at
  mission start. The original first-evaluation phase can differ by a tick; no
  live timing measurement was obtained. Diagnostic evaluations remain read-only.
- **G:** living embarked passengers retain side/type/tactic identity for survival
  and elimination conditions, but do not independently occupy objective rectangles.
  Vessel destruction kills cargo. This prevents boarding a protected hero from
  becoming a false death. Exact original cargo counting needs measurement.
- **G:** a pending script path is busy until the path runner finishes it. The
  next evaluation can then dispatch a dependent attack. This prevents same-tick
  replacement without inventing a reference travel duration.

Script outcomes still use the existing first-outcome-wins rule. All actions in a
firing trigger run even after an outcome or self-disable. Replay semantics version
is **7**; older engine command-log saves are rejected rather than silently replayed
under changed script/ownership rules. Original-save script execution remains a
separate dependency.

## Per-mission verification

The ordinary baseline used the existing headless automatic player twice per
mission, 1,200 simulation ticks, 60 seconds per process. Mission 14 exhausted the
wall limit in both attempts; no outcome is inferred for it. The after sweep used
240 ticks, 90 seconds per process, again twice. These unequal horizons cannot
establish a survival improvement. Only mission 5 won in the ordinary after sweep;
its repeated save continuation also passed the existing harness.

The forced victory column uses the complete `GameSim`, unchanged scripts and
explicit world mutations, with no combat immunity or direct outcome assignment.
The isolated branch controls advance only the script runner and supplied world
mutations; path orders are dispatched but movement, combat, economy and transport
are not advanced there. Mission 9's rescue control instead uses full simulation
through tick 404. Coverage is the union of separate compatible controls, not a
claim that mutually exclusive outcomes happen in one playthrough.

| Mission | Ordinary baseline | Ordinary after | Forced simulation victory tick | Isolated defeat tick | Action sites dispatched / total | What remains unproven |
| --- | --- | --- | ---: | ---: | ---: | --- |
| 1 | Defeat 1195 | Timeout 240 | 2 | 1 | 30/30 | Honest survival/combat route; timed alternative uses isolated runner. |
| 2 | Timeout 1200 | Timeout 240 | 82 | 30000 | 50/50 | Honest town conquest; ferry combat/arrival and original countdown phase. |
| 3 | Timeout 1200 | Timeout 240 | 2 | 1 | 2/2 | Honest enemy elimination. |
| 4 | Defeat 289 | Timeout 240 | 2 | 1 | 28/28 | Hero traversal, combat and camera locking. |
| 5 | Victory 230 | Victory 230 | 2 | 1 | 18/18 | Ordinary victory may count destroyed vessels; rescue/cargo survival not established. |
| 6 | Timeout 1200 | Timeout 240 | 2 | 1 | 19/19 | Combat capture, reinforcement movement and asynchronous join semantics. |
| 7 | Timeout 1200 | Timeout 240 | 2 | 1 | 36/36 | Honest landings and capture ordering; protection losses also forced separately. |
| 8 | Timeout 1200 | Timeout 240 | 2 | 1 | 22/22 | Honest rescue travel and protection of allied towns. |
| 9 | Timeout 1200 | Timeout 240 | 2 | 1 | 30/30 | Four rescue gifts dispatch in the tick-404 simulation control; travel to their rectangles is forced. |
| 10 | Defeat 1126 | Timeout 240 | 2 | 1 | 9/9 | Honest enemy elimination. |
| 11 | Defeat 1 | Timeout 240 | 3 | 1 | 37/37 | Hero travel and combat; gift/capture queue behavior against the original. |
| 12 | Timeout 1200 | Timeout 240 | 251 | 1 | 18/18 | Reinforcement travel and battle; timer/gift branch dispatches at 250. |
| 13 | Timeout 1200 | Timeout 240 | 2 | 1 | 18/18 | Actual path completion; isolated runner forces idle states and reaches timed wave. |
| 14 | Wall timeout | Timeout 240 | 2 | 1 | 23/23 | Actual timed-wave travel; isolated runner reaches the final timed join before victory. |
| 15 | Timeout 1200 | Timeout 240 | 2 | 1 | 23/23 | Honest captures/diplomacy, Johnka routes, submarine presentation and final campaign UI. |

Repeated reports match within each final control. All victory outcomes name a
source script action. Every required terminal trigger has executed in some
control; every action site has a nonzero execution count across those controls.
This does **not** establish that each dispatched action found its target after
previous joins/gifts, followed a route, or matched the original's effects.
Synthetic tests separately check actual town ownership, queue cancellation,
protected cargo, timer enablement and path gating.

## Reproduction and pending measurements

Build and run the asset-independent controls:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DAZTEC_GEN_DIR=
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
pre-commit run --all-files
build/aztec_script_audit --maps /absolute/private/game/Campaign/Maps --require-supported
```

Use `tools/playthrough.py` for ordinary bounded runs. It stages separate private
installations and launches with `unshare -rn` and both dummy SDL drivers. For a
forced control, stage the same way with its `private_game` helper, retain the
unaltered Maps symlinks and create a private event file. Never run against the
shared installation. For example, using an independently invented mission:

```text
# One-based ticks, before the simulation update; these are world interventions.
2 kill-side 1
```

```sh
unshare -rn env SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
  build/aztec_campaign_probe --game "$PWD/work/sweep/game-1" \
  --rules-dir "$PWD/docs/data" --mission 1 \
  --events "$PWD/work/sweep/synthetic.events" --max-ticks 600 --expect victory
```

Events are `kill-side SIDE`, `kill-halls SIDE`, `kill-type SIDE TYPE`,
`kill-tactic SIDE ID`, `move-type SIDE TYPE X Y`, `move-units SIDE X Y`,
`transfer-town TOWN_INDEX SIDE`, `hit-tactic SIDE ID ATTACKER_OBJECT` and
`idle-tactic SIDE ID`. Coordinates are tiles. Prefix each with its one-based tick.
The probe rejects unknown operations, empty matches, invalid arguments, unexpected
outcomes, unused events, unknown placed types and even dormant unsupported actions.
`--mode scripts` explicitly selects the isolated runner; default `simulation`
advances the whole game. Neither mode edits scripts, forces a condition result,
sets an outcome, disables a trigger or writes campaign progress. Output contains
counts, hashes and source-site execution histories, not script text.

This worktree's ignored evidence is under `work/campaign-sweep-before/`,
`work/campaign-sweep-after/`, `work/campaign-sweep-audit/`,
`work/campaign-sweep-input/`, and `work/campaign-sweep-final/`. The final manifest
pins the probe, events, rule files and mission inputs: 100 successful processes,
50 identical report pairs, 363 covered action sites and 92 covered triggers. Retain event
files, executable/input hashes, both reports and process statuses together.
Final repeated controls are retained separately from exploratory attempts.

The isolated reference preflight failed at `ip link set lo up`:
`Cannot open netlink socket: Operation not permitted`. The original runtime
never launched. Its stderr and nonzero status remain private under
`work/campaign-sweep-input/reference-preflight/`. Isolation was not bypassed.
There are no live reference measurements from this task.

On a host that supports the documented reference namespace setup, measure each
item twice with unchanged controls and retain the raw recordings privately:

1. First trigger evaluation, timer creation/update phase, equal-time boundaries,
   source-order enablement and countdown reset after a town changes back.
2. Town tactic lifetime after hall loss, capture radius/defender eligibility,
   neutral/allied ownership, active construction, queues/refunds, research and
   detached garrisons during capture and gifts.
3. Hero/cargo side counts, type counts and rectangle membership while boarding,
   sailing, unloading and losing a carrier.
4. All 12 ferry, three submarine and four Johnka action sites: activation,
   landing, blocked shore, return, repeated enable and autonomous routes. Follow
   [transport_rules.md](transport_rules.md) for the existing transport assumptions.
5. Finite/cyclic paths, idle phase, joins and attack-town targeting after transfers.
   Camera duration/locking and submarine ending presentation need visible controls.
6. Honest winning player routes, defeat/retry, saves and UI campaign progression.
   Task 80/92 route/reference observers and original-save continuation remain
   dependencies; this checkout's forced-state results do not supply them.

Validation: the baseline Release build and all hooks passed; CTest passed 62
entries and skipped six. The final Release suite passes 63 entries and skips six
(69 total); all hooks also pass for tracked and new files. Separate private-copy
format and engine checks both pass with dummy SDL under `unshare -rn`. The strict
campaign inventory passes with zero unsupported sites and the two mission-6
warnings described above. `git diff --check` passes. No files in ROADMAP,
EXPERIMENTS or parity_matrix are changed.

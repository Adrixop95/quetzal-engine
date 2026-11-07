# Automated campaign playthrough

`tools/playthrough.py` exercises the independent engine on the user's campaign
maps. It discovers case-insensitive `mission_N.mp` names and requires coverage of
missions 1 through 15 by default. Each scenario runs twice in fresh processes,
then loads a mid-mission save in a third process when that boundary was reached. Original data is required;
no game files or reference-derived code are distributed with the harness.

Build from the worktree root:

```sh
sh tools/bootstrap.sh --no-hooks
CCACHE_DIR="$PWD/build/ccache" cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DAZTEC_GEN_DIR=
CCACHE_DIR="$PWD/build/ccache" cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
pre-commit run --all-files
```

Run all 15 campaign missions:

```sh
python3 tools/playthrough.py --game /absolute/path/to/private/game
```

For a short sweep or selected missions:

```sh
python3 tools/playthrough.py --game /absolute/path/to/private/game \
  --missions 1 2 3 --max-ticks 1200 --timeout 30 \
  --out work/playthrough-short
```

The default simulation budget is 43,200 ticks (one simulated hour at 12 Hz), with
180 wall seconds per process. A mission stops on its script's local victory or
defeat, or at the tick budget. The outer timeout kills the entire process group,
including the namespace launcher, if the simulation stalls or runs too slowly.
The next run and mission still execute. Duplicate numeric mission names are configuration errors. Missing missions remain
explicit failed matrix rows; partial selections retain all 15 matrix entries and
mark unselected entries. An installation with no missions is a configuration error.

Each invocation creates a new ignored directory under this worktree's `work/`.
`--out` must name a new directory there. It retains an executable snapshot with a
SHA-256 identifier, copied rule tables and optional orders, two private game
directories, per-run stdout/stderr, `engine.json`, `ticks.txt`, mid-mission AZS3
saves, a copied scenario plan, and incrementally updated `summary.json` (schema 2)
and `matrix.md`. The JSON matrix retains complete run diagnostics. Do not add any of these files to git. The executable is the
independent engine built here, not the reference runtime. Game resources and map
files are read-only symlinks to the installation; keep that installation stable
during both runs. Maps and Saves are ordinary private directories; profiles and
configuration files are copied, and original saves are excluded. The headless
engine only reads the symlinked inputs.

Every real game-data process uses `unshare -rn` and both
`SDL_VIDEODRIVER=dummy` and `SDL_AUDIODRIVER=dummy`. Namespace failures remain
failures; there is no fallback outside isolation. This mode opens no SDL window,
audio device or network socket and needs no loopback interface configuration.
It pins `AZTEC_GAME` to the private copy so sight rules cannot come from an
unrelated inherited installation. It does not launch the reference runtime. Reference record/replay and headless
probes still follow [EXPERIMENTS.md](EXPERIMENTS.md); internal engine agreement
cannot replace that evidence.

The simulation retains the existing computer-player AI and mission trigger/path
orders. Every 120 ticks, the default exercising policy purchases the first
affordable mobile item for the human side when its queue is empty, and sends
idle armed mobile units toward their nearest live hostile object for which the
weapon has known getters and positive known damage. Distance ties
use stable object order. This policy uses engine state, including unseen objects;
it is a coverage driver rather than a model of a human player. It does not grant
money, teleport units, force outcomes or implement mission-specific solutions.
Use `--order-interval N` to change its interval, or `--no-auto-orders` to disable
it. Existing computer AI and mission scripts continue in either case.

For specific objectives, place English order files in private `work/orders/` and
pass `--orders-dir work/orders`. Each optional `mission_N.orders` contains:

```text
# tick command side tile-x tile-y [engine object IDs...]
1 move 0 12 8
240 attack-move 0 30 24 1 2 3
```

Ticks are one-based. Commands are `move` and `attack-move`; omitted IDs select
all live ground or flying mobile objects belonging to that side at execution
time. Explicit IDs are the engine's stable one-based IDs. Coordinates must be inside the loaded
terrain; side indices are 0 through 7. Orders are stably sorted by tick, submitted
after automatic orders, and use the normal simulation command queue. Invalid
syntax, an order beyond the tick budget, an empty selection, or invalid/dead/
foreign/nonmobile explicit IDs fails the run. A terminal outcome can end a run
before later orders execute. `--no-auto-orders` gives the order file sole control
of the player without disabling mission scripts.

The engine mode can also be called directly on a prepared private copy:

```sh
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy timeout -s KILL 180 \
  unshare -rn build/aztec_engine --headless \
  --game "$PWD/work/private-game" --rules-dir "$PWD/docs/data" \
  --max-ticks 43200 --report "$PWD/work/mission-1.json" 1
```

`--headless` must be the first engine argument; `--headless --help` lists its
separate options. It requires valid resource archives and rule tables and uses
the same world loader, fixed 12 Hz simulation, scripts, combat and production as
the interactive engine. It never substitutes default rules when loading fails.

Reports distinguish victory, defeat, simulation timeout, wall timeout, engine
errors, crashes/signals, invalid reports and isolation/launch failures. They
include logged command counts, unsupported action execution counts, unsupported
action source lines (even in dormant triggers), missing trigger references and
script parse errors. Missing scripts remain explicit diagnostics.

`unreachable_objectives` contains narrow static findings: absence of a local
victory action, or a victory trigger requiring a positive unit count in a
rectangle disjoint from the terrain or with inverted bounds. Another victory
trigger might still provide a route to victory. `unmet_objectives` lists false
victory conditions at termination. `stalled_order_ids` lists live objects still
holding move/attack-move orders without positional progress for at least 600
ticks. Unmet conditions and stalled movement are investigation leads, not proof
that a mission is unreachable. The harness does not solve arbitrary trigger
chains, transport puzzles, dynamic obstructions or future production.

Every `ticks.txt` row contains an absolute tick and hexadecimal state hash, starting
at tick zero (or at the loaded save boundary). The runner compares every paired
tick and reports the first mismatch. Missing, malformed, non-contiguous or
truncated transcripts cannot pass. It also checks terminal status, tick count,
final hash and the uninterrupted rolling digest. Failed or killed processes
retain partial artifacts and yield unverified hash evidence.

Each scenario saves at its configured tick after simulation processing, continues
uninterrupted, then resumes that save in a fresh engine process. The ordinary
AZS3 reader checks mission, script/content identity and the replay hash before
continuation. The resumed process skips already executed scenario orders, retains
the automatic policy's absolute tick schedule, and records the saved boundary
plus every subsequent hash. The runner compares this entire suffix and the final
outcome to the uninterrupted run. Its rolling digest covers only the resumed
suffix and is deliberately not compared to the full-run digest. An early outcome,
failed save or lack of ticks after the save remains unverified; it cannot pass.
Stall diagnostics start afresh after loading and do not affect simulation hashes.
Headless saves include mission and rules identity but do not test camera, profile
or front-end progression. Keep installations unchanged during a run: asset/map
symlinks remain live read-only inputs rather than frozen copies.

With no scenario file, the route is explicitly labelled an unvalidated automatic
exercising policy, the expected outcome is victory, and the save tick is half the
simulation budget. This is a baseline probe, not a successful completion route.
For mission-specific routes, pass `--scenarios work/routes/scenarios.json`:

```json
[
  {
    "id": "mission-1-win",
    "mission": 1,
    "route": "Describe the player-controllable route and objective checkpoints in English",
    "expected_outcome": "victory",
    "save_tick": 600,
    "orders": "mission-1.orders"
  },
  {
    "id": "mission-1-defeat",
    "mission": 1,
    "route": "Describe a reproducible ordinary defeat route",
    "expected_outcome": "defeat",
    "save_tick": 120,
    "orders": "mission-1-defeat.orders"
  }
]
```

This is a schema example, not a campaign solution. Orders paths resolve relative
to the plan and are copied into private output. IDs must be unique filename-safe
names, missions must be 1 through 15, routes must be nonempty, expected outcomes
must be victory or defeat, and saves must precede the tick limit. A plan can supply
multiple routes for one mission. Every selected mission needs a scenario; omitted
missions fail coverage. `--orders-dir` remains a fallback for scenarios without
explicit orders. `--no-auto-orders` lets a route use only player orders and scripts.
No new cheats, outcome overrides or teleport commands are available.

A scenario passes only when both runs reach its expected outcome, have matching
per-tick evidence, continue identically after a fresh-process save, and have no
script errors, unsupported actions or static unreachable-objective diagnostics.
The runner exits zero only when every selected scenario passes and every selected
mission is covered. Expected defeat can pass a defeat scenario; it cannot count
as a campaign completion route. Timeouts and all failed evidence exit one;
configuration errors exit two. A zero exit for a subset or synthetic plan does
not establish full campaign acceptance.

The direct engine mode exits zero for any
completed bounded simulation (including defeat/timeout) and one for input or
simulation errors; report-write failures exit two. Read the JSON outcome when
using it without the Python runner. Direct options `--tick-hashes FILE`,
`--save-at N --save FILE` and `--resume FILE` expose the same evidence controls.

`ctest --test-dir build -R playthrough --output-on-failure` runs synthetic
fixtures for outcomes, hash agreement, malformed scripts/orders, impossible versus
unmet objectives, private copies, campaign summaries, crashes and timeouts. These
fixtures need no assets or namespace privileges. Only their synthetic launcher
is replaced in tests; production always invokes `unshare -rn`.

## V19 acceptance matrix and remaining work

Every generated matrix contains all 15 missions with scenario routes, expected
and observed outcomes, unsupported source sites and execution counts, static
unreachable findings, unmet objectives, stalled orders, crash/timeout reports,
paired tick comparison and save-continuation evidence. Missing or unselected
missions remain visible. Static findings are narrow checks, not a proof that all
objectives are reachable. An empty finding list does not establish reachability.

The public planning matrix below retains the existing [script audit](campaign_scripts.md).
No player-controllable completion route has been validated for any mission.
For every row, the required winning outcome is victory; actual run artifacts and
save/hash comparisons stay private under `work/`.

| Mission | Completion route | Unsupported action sites | Objective reachability | Interactive / progression |
| --- | --- | --- | --- | --- |
| 1 | Pending | 0 | Unverified | Pending |
| 2 | Pending | 4 transport sites | Unverified | Pending |
| 3 | Pending | 0 | Unverified | Pending |
| 4 | Pending | 0 | Unverified | Pending |
| 5 | Pending | 0 | Unverified | Pending |
| 6 | Pending | 0; two missing-trigger warnings | Unverified | Pending |
| 7 | Pending | 5 transport sites | Unverified | Pending |
| 8 | Pending | 0 | Unverified | Pending |
| 9 | Pending | 5 transport sites | Unverified | Pending |
| 10 | Pending | 0 | Unverified | Pending |
| 11 | Pending | 0 | Unverified | Pending |
| 12 | Pending | 0 | Unverified | Pending |
| 13 | Pending | 0 | Unverified | Pending |
| 14 | Pending | 0 | Unverified | Pending |
| 15 | Pending | 5 transport sites | Unverified | Pending |

V03-V16 are incomplete dependencies. The harness, inventory and synthetic
checks proceed independently; successful long campaign routes require the
remaining simulation, script, control and save behavior. The matrix deliberately
keeps interactive acceptance, defeat/retry and progression pending. Separate
headless mission processes do not exercise the front-end Retry button, mission
unlock/profile persistence or mission 15's ending.

For interactive acceptance on Linux and Windows, retain private records for each
mission: platform/build identity, route/order plan, normal controls and briefing,
objective checkpoints, victory, ordinary defeat followed by Retry with the same
inputs, a mid-mission save followed by process exit/load/completion, and progression
to the next mission (the final ending for mission 15). Record unresolved blockers
and manual observations separately from automated hashes. Do not mark V19 complete
until those records and successful automated routes cover the whole campaign.

Synthetic tests cover all 15 fixture missions with paired transcripts and
fresh-process saves, expected defeat and a separate fresh winning attempt,
first-difference detection, truncated/gapped transcripts, incompatible script
refusal, missing coverage and scenario validation. The synthetic retry fixture
changes its script between attempts; it tests harness bookkeeping, not the real
Retry UI or a playable same-input recovery route.

## Campaign fix iteration (2026-11-01, partial)

Two headless-driver defects have synthetic regressions. Scripted movement now
accepts flying units for both implicit selection and explicit IDs, matching the
ordinary command handler. Previously, an explicit flying ID failed validation,
and implicit selection omitted it. The automatic exercising policy now excludes
targets without known weapon getters and positive known damage. Previously, a
nearby flying scout could divert infantry from a ground target it could damage.
These are driver corrections; simulation combat, AI and script semantics are
unchanged. Existing flying types are unarmed; no flying combat pursuit behavior
has been added or verified.

Both regressions failed before their corresponding fixes. The synthetic flight
fixture covers move/attack-move orders, implicit/explicit selection, changed
simulation state and exact fresh-process save continuation. The targeting
fixture compares automatic orders with an explicit order toward an eligible
target. It checks both the terminal hash and rolling tick digest.

The final private smoke sweep used initial regroup orders alongside the automatic
policy and computer AI, 24 ticks, a tick-12 save and a 20-second process limit.
All 15 pairs matched every tick; fourteen save continuations matched. Mission 11
ended in defeat at tick 1 before saving. There were no crashes, wall timeouts,
static unreachable-objective findings or stalled movement reports in this sweep.
Every mission still had unmet victory conditions at termination. These narrow
diagnostics do not establish objective reachability or absence of later hangs.

| Mission | 24-tick outcome | Paired hashes | Save continuation | Unsupported sites / executions per run |
| --- | --- | --- | --- | --- |
| 1 | Tick limit | Equal | Equal | 0 / 0 |
| 2 | Tick limit | Equal | Equal | 4 / 0 |
| 3 | Tick limit | Equal | Equal | 0 / 0 |
| 4 | Tick limit | Equal | Equal | 0 / 0 |
| 5 | Tick limit | Equal | Equal | 0 / 0 |
| 6 | Tick limit | Equal | Equal | 0 / 0; two missing-trigger warnings |
| 7 | Tick limit | Equal | Equal | 5 / 5 |
| 8 | Tick limit | Equal | Equal | 0 / 0 |
| 9 | Tick limit | Equal | Equal | 5 / 4 |
| 10 | Tick limit | Equal | Equal | 0 / 0 |
| 11 | Defeat at tick 1 | Equal | Not reached | 0 / 0 |
| 12 | Tick limit | Equal | Equal | 0 / 0 |
| 13 | Tick limit | Equal | Equal | 0 / 0 |
| 14 | Tick limit | Equal | Equal | 0 / 0 |
| 15 | Tick limit | Equal | Equal | 5 / 0 |

The final longer sweep used the same orders, 1,200 ticks (100 simulated seconds),
a tick-600 save and a 20-second process limit. Both runs produced the listed
outcome in every row. There were no crashes or victories. All ten completed
pairs matched every tick; six matched save continuations. The five wall-limited
pairs remain unverified, and exceeding this budget does not prove a hang.
Completed reports had no static unreachable findings or stalled movement IDs;
unmet victory conditions remain investigation leads, not proof of impossibility.

| Mission | Longer-run outcome / final tick | Paired hashes | Save continuation |
| --- | --- | --- | --- |
| 1 | Defeat / 316 | Equal | Not reached |
| 2 | Tick limit / 1200 | Equal | Equal |
| 3 | Defeat / 968 | Equal | Equal |
| 4 | Defeat / 294 | Equal | Not reached |
| 5 | Tick limit / 1200 | Equal | Equal |
| 6 | Tick limit / 1200 | Equal | Equal |
| 7 | Tick limit / 1200 | Equal | Equal |
| 8 | Wall timeout | Unverified | Unverified |
| 9 | Tick limit / 1200 | Equal | Equal |
| 10 | Defeat / 586 | Equal | Not reached |
| 11 | Defeat / 1 | Equal | Not reached |
| 12 | Wall timeout | Unverified | Unverified |
| 13 | Wall timeout | Unverified | Unverified |
| 14 | Wall timeout | Unverified | Unverified |
| 15 | Wall timeout | Unverified | Unverified |

Mission 4's orders explicitly exercise a computer flying unit. A separate
24-tick observation probe confirms movement; the same order fails with the
baseline executable. Missions 7 and 9 have no placed player mobile units in the
current loader, so their initial regroup orders address computer units. The
player policy remains active, but this does not exercise boarding, cargo or a
player completion route. Other initial regroup orders address player units.
Order plans, copied executable/rules, transcripts, saves and matrices remain in
ignored `work/parity/campaign-final-smoke/` and `work/parity/campaign-final/`.
Both campaigns require victory and intentionally return failure without one.

Mission 11's initial data puts the protected scenario group in a town, while
`ScriptRunner::tactic_members` currently matches explicit object tactic IDs;
town members have no such IDs. This explains its initial false `valid` condition.
Correct town/tactic lookup, ownership transfer and capture behavior still need
the incomplete tactic model and reference evidence. Assigning every town member
a team tactic would also change autonomous AI eligibility; no such workaround
was applied. Transport actions/cargo in missions 2, 7, 9 and 15 remain dependent
on the incomplete movement/transport and script work. Winning routes, mission
progression and interactive acceptance remain pending for all 15 missions.

An isolated reference recording control failed at loopback setup with a netlink
permission error before launching the runtime. No recording or replay evidence
was produced; logs remain in `work/parity/campaign-reference/`. No reference
rule or parity status is promoted by these engine self-comparisons.

Validation: Release build and the full CTest suite passed (34 passed, six skipped:
five private-data entries and restricted UDP processes). Separate private-copy,
dummy-SDL checks under `unshare -rn` passed formats, engine integration and the
campaign script inventory. Synthetic regressions require no game installation.
All pre-commit hooks and `git diff --check` also passed.

## Outcome provenance and selected inputs (2026-11-04)

Headless JSON reports now include `outcome_origin`: the trigger ID, action source
line and simulation tick that first changes the human mission outcome. It is
`null` if no script action ended that mission. Foreign-side results and later
conflicting results do not replace it. The record is bounded to one event,
resets on initialization and is reconstructed by command replay; it is diagnostic
state, excluded from gameplay hashes. Condition/action lines now skip leading
whitespace before recording their source position, including CRLF and blank
lines. The record identifies the executed action, not a
proof that the underlying condition matches the reference.

Simulation sight and save fingerprints now use the same selected installation
as mission loading. A synthetic regression reproduced save refusal when an
unselected `AZTEC_GAME` directory changed; it passes after the fix. Further
controls reject changed selected resources, load a headless mid-mission save in
the actual mission UI and continue to synthetic victory, and reconstruct outcome
provenance after a fresh-process resume. All fixtures use invented data.

The isolated private campaign observations for this iteration and unresolved
acceptance steps are recorded in [the engine inventory](engine_rules_status.md#campaign-input-and-outcome-iteration-2026-11-04-partial).

## Action history and objective blockers (2026-11-04)

Every engine report now includes `action_observations`, one row per source action,
with `trigger_index`, `action_index`, trigger ID, source line, kind and support
classification. `executions`, `first_tick` and `last_tick` count dispatches at
one-based script ticks; an unobserved site has three zeros. Unsupported dispatch
counts mean the branch was reached, not that the action succeeded. Indices are
zero-based and distinguish repeated trigger IDs or actions on the same line.
`enabled` and `false_condition_lines` describe termination only. Histories cover
the whole run, including the replayed prefix after a save load.

The runner requires these additive schema-1 fields from its engine snapshot and
checks histories against `unsupported_executions`. It writes `actions.md` beside
`matrix.md`, retaining every unsupported site and every unobserved supported
site for both repetitions and the resumed run. Missing reports remain unavailable
evidence. An unobserved action is never evidence that the original never fires it.
A disabled local-victory trigger now appears in `unmet_objectives` even when all
its conditions are true; its current condition snapshot alone could previously
leave the unmet list empty.

For stalled-order investigations use `--observations FILE --order-observations`
with the headless engine. The default five-field parity stream stays unchanged.
Object rows include order, pixel destination, target ID, tactic and
scenario identity alongside fixed-point positions. They are sampled before
commands, so tick N+1 shows the effects of tick N. No original identity mapping,
transport route solver or general objective reachability analysis is implied.
The [site disposition table](campaign_scripts.md#transport-follow-up-2026-11-04-needs-live-run)
keeps all 19 required transport sites open pending measurements.

## Scripted route contract (2026-11-05): needs live run

Task 80 adds tooling independently of the incomplete task 71. There are **0/15
verified campaign routes**. In particular, transport in missions 2/7/9/15 and
town-derived objective identity in mission 11 remain blockers. No live campaign
or reference run was attempted in this sandbox. The earlier private sweeps above
are historical observations, not results of this change.

`--routes` aliases `--scenarios` and accepts the existing JSON list. A route may
replace `orders` with `steps` and add `checkpoints`. For example, this specification
uses invented coordinates and action sites; it is a synthetic example only:

```json
[
  {
    "id": "example-win",
    "mission": 1,
    "route": "Move, regroup after the save, then reach the two objective checkpoints",
    "expected_outcome": "victory",
    "save_tick": 5,
    "steps": [
      {"tick": 1, "action": "move", "side": 0, "x": 2, "y": 2, "ids": [1]},
      {"tick": 7, "action": "attack-move", "side": 0, "x": 3, "y": 3, "ids": [1]}
    ],
    "checkpoints": [
      {"id": "started", "trigger_index": 0, "action_index": 0, "kind": "TriggerDisable"},
      {"id": "won", "trigger_index": 1, "action_index": 0, "kind": "SideVictory"}
    ]
  }
]
```

Inline steps require `--no-auto-orders` and cannot be combined with `orders` or
`--orders-dir`. Ticks must be positive, ordered and within `--max-ticks`; equal
command ticks preserve list order. Coordinates are terrain tiles. IDs select
ordinary engine objects; an empty list selects all eligible mobile units of the
specified side. Only `move` and `attack-move` are supported. Boarding, unloading,
building and other commands require future driver support; validation rejects
them explicitly. No cheats or forced mission outcomes are introduced.

Checkpoint IDs are stable English names for equivalent semantic milestones in
both implementations. Source indices are zero-based engine action-observation
indices, with the kind checked to detect stale mappings. They are authored
specifications, not executable-derived code. Every checkpoint must execute a
supported action, with first dispatches strictly increasing in the specified
sequence. Multiple milestones in one tick are ambiguous with aggregated action
histories and fail this contract. Repeated dispatches after the first are outside
its scope. Resume checks reconstruct the full checkpoint history as well as
comparing every continuation hash. Missing checkpoints, reversed sequences and
unsupported sites cannot pass route acceptance.

The private summary records `route_sha256`, reached outcomes, checkpoint checks,
script diagnostics, paired ticks, save continuation and a separate `verified`
flag. The fingerprint binds the specification, order text, automatic-policy
settings and tick budget; it excludes absolute order-file paths. It identifies
inputs, not proof that a reference input translator executed them correctly.

`--reference-dir DIR` imports two **audited** private timelines per route,
`ID-1.json` and `ID-2.json`. Both must agree completely. Their format is:

```json
{
  "schema": 1,
  "contract": "campaign-checkpoints-v1",
  "mission": 1,
  "route_sha256": "COPY_THE_FINGERPRINT_FROM_THE_PRIVATE_CLONE_SUMMARY",
  "status": "victory",
  "ticks": 15,
  "events": [{"id": "started", "tick": 3}, {"id": "won", "tick": 15}]
}
```

The example is not an observation. An audited reference observer must derive
these events from actual runs, establish equivalent mission-start ticks, verify
normal player inputs against the route, and observe the terminal outcome.
Reference logs currently lack this general objective observer and a translator
for arbitrary player routes. Do not generate reference timelines from clone
reports, script expectations, raw hash equality or wall-clock sleeps. Record
missing observed milestones by omitting their events; never fill expected ticks
into measurements. Unsupported identity/timing mappings remain pending.

The comparator validates both complete timelines, then reports the earliest
checkpoint identity/tick or terminal outcome/tick difference against both clone
runs. The result describes **checkpoints and outcome only**, not full state parity.
Missing, malformed, mismatched-plan or inconsistent reference files retain
`equal: null`, `first_divergence: null` and status `needs live run`, with a reason.
Absence of a divergence measurement does not mean equality. Reference files are
copied into private output before comparison. Supply `--require-reference` to
fail when reference evidence is absent. Supplying `--reference-dir` also makes
reference equality mandatory. Legacy engine-only runs retain their exit behavior,
but `verified` is always false without reference evidence.

Complete comparisons write private `ID-reference/diff.json` files labelled L7
for the existing parity aggregation tools. Missing reference evidence emits no
paired diff and adds no pass, fail or skip to the public paired-scenario totals.
Even a measured equal outcome does not count as a verified route unless the
expected outcome, objective sequence, diagnostics and save continuation also pass.
Interactive retry, campaign progression and native platform acceptance remain
separate requirements.

The public route specification inventory below records required outcomes and
unmeasured fields instead of inventing mission solutions. For every row,
reached outcome, first reference divergence, current unsupported executions and
save-to-completion are **needs live run**. No current runtime counts are inferred
from the historical source-site inventory.

| Mission | Required route outcome | Objective sequence specification | Verified routes | Blocking evidence |
| --- | --- | --- | ---: | --- |
| 1 | Victory | Needs measured player route and named milestones | 0 | Task 71 / live observer |
| 2 | Victory | Needs transport and objective milestones | 0 | Task 71; 4 historical unsupported sites |
| 3 | Victory | Needs measured player route and named milestones | 0 | Task 71 / live observer |
| 4 | Victory | Needs measured player route and named milestones | 0 | Task 71 / live observer |
| 5 | Victory | Needs measured player route and named milestones | 0 | Task 71 / live observer |
| 6 | Victory | Needs measured player route and named milestones | 0 | Task 71; two historical missing-trigger warnings |
| 7 | Victory | Needs transport and objective milestones | 0 | Task 71; 5 historical unsupported sites |
| 8 | Victory | Needs measured player route and named milestones | 0 | Task 71 / live observer |
| 9 | Victory | Needs transport and objective milestones | 0 | Task 71; 5 historical unsupported sites |
| 10 | Victory | Needs measured player route and named milestones | 0 | Task 71 / live observer |
| 11 | Victory | Needs protected-group ownership milestones | 0 | Task 71; town-derived objective identity |
| 12 | Victory | Needs measured player route and named milestones | 0 | Task 71 / live observer |
| 13 | Victory | Needs measured player route and named milestones | 0 | Task 71 / live observer |
| 14 | Victory | Needs measured player route and named milestones | 0 | Task 71 / live observer |
| 15 | Victory | Needs transport, objective and ending milestones | 0 | Task 71; 5 historical unsupported sites |

Expected-defeat routes can use `expected_outcome: defeat`; they never substitute
for winning routes. The expanded synthetic regression runs all 15 invented
missions with steps before/after saving, two ordered objectives, missing-reference
failure and two agreeing invented reference timelines. Additional controls cover
first divergence, stale route identity, malformed/inconsistent timelines,
unsupported commands and checkpoint order. This is S tooling evidence only.
See the [live run sheet](EXPERIMENTS.md#v19-scripted-route-run-sheet-needs-live-run).

## All-mission campaign flow (task 92): needs live run

**S (2026-11-07):** `campaign_flow` runs the real front-end and mission loop with
fifteen independently invented 8x8 maps, empty resource archives and six-tick
outcome scripts. No private installation, executable or captured data is used.
The public fixture in `tests/test_campaign_flow.cpp` is the executable synthetic
route specification. For each mission number it covers:

1. Select the unlocked campaign button, review its briefing and press Start.
   Menu automation stops at selection; mission loading is exercised separately.
2. Open pause/objectives, wait, close review and resume. Tick zero must persist
   through review; the ordinary script runner then reaches victory at tick six.
   The outcome freezes time. Continue opens the actual successor briefing for
   missions 1-14; mission 15 shows the clone's `victory` screen and returns to Main.
3. Load an invented defeat script, reach defeat at tick six and return to Main.
4. Use the existing test-only Simulation interface for an initial defeat, press
   Retry, assert a fresh tick-zero/running world and reach normal scripted victory.
   A second control starts with injected victory, clicks Restart, then reaches
   normal scripted defeat and returns to Main. These injected attempts test UI
   transitions, not achievable gameplay outcomes.
5. Assert that playtest runs leave the engine completion profile unchanged.
   Separately record completion through `CampaignProgress`, save/reopen it,
   confirm the completed bit and immediate successor unlock, and assert that the
   following mission remains locked. Profile persistence and ordinary victory dispatch
   are separate contracts: this does not test an uninstrumented victory write.

The table reports **synthetic** outcomes only. `W/L/R` means scripted victory,
scripted defeat/return, and retry/restart controls passed. The mask is the separate
profile API test's expected completed-mission bitmask after recording that row.
Lose/retry leave the previous mask unchanged. No gameplay blocker was fixed:
the change closes missing all-mission regression coverage. Every real mission's
reached outcome, winning player route, retry outcome and profile progression
remain **needs live run**; verified campaign routes remain **0/15**.

| Mission | Synthetic reached outcome | Continue target | Profile mask | Remaining live blockers / cause |
| --- | --- | --- | ---: | --- |
| 1 | W/L/R | Briefing 2 | 1 | Normal-input route, objective and UI observers unmeasured |
| 2 | W/L/R | Briefing 3 | 3 | Above; four unsupported transport sites, task 86 measurements missing |
| 3 | W/L/R | Briefing 4 | 7 | Normal-input route, objective and UI observers unmeasured |
| 4 | W/L/R | Briefing 5 | 15 | Normal-input route, objective and UI observers unmeasured |
| 5 | W/L/R | Briefing 6 | 31 | Normal-input route, objective and UI observers unmeasured |
| 6 | W/L/R | Briefing 7 | 63 | Above; two historical absent-trigger warnings require live disposition |
| 7 | W/L/R | Briefing 8 | 127 | Above; five unsupported transport sites, task 86 measurements missing |
| 8 | W/L/R | Briefing 9 | 255 | Normal-input route, objective and UI observers unmeasured |
| 9 | W/L/R | Briefing 10 | 511 | Above; five unsupported transport sites, task 86 measurements missing |
| 10 | W/L/R | Briefing 11 | 1023 | Normal-input route, objective and UI observers unmeasured |
| 11 | W/L/R | Briefing 12 | 2047 | Above; town-derived protected-group ownership / immediate defeat unresolved |
| 12 | W/L/R | Briefing 13 | 4095 | Normal-input route, objective and UI observers unmeasured |
| 13 | W/L/R | Briefing 14 | 8191 | Normal-input route, objective and UI observers unmeasured |
| 14 | W/L/R | Briefing 15 | 16383 | Normal-input route, objective and UI observers unmeasured |
| 15 | W/L/R; clone ending screen | Main | 32767 | Above; five unsupported transport sites and original ending sequence unmeasured |

**P:** the successor/unlock policy and clone ending presentation remain provisional.
The `victory` screen is an engine heading, not evidence for an original final
cinematic, credits or equivalent return sequence. The player-profile selector
and complete original menu graph from task 77 remain open. A single continuous
normal-input campaign with persistent progress has not been demonstrated.

Task 86 is present as evidence tooling, but its transport behaviour dependency
has not landed: all 19 sites remain unsupported. Tasks 77/80 supply partial flow
and route scaffolding, not audited reference observers or successful routes.
No engine behaviour was changed without the required original measurement
repeated across two runs. No reference or private game-data process was launched
in this sandbox. Follow the [task-92 run sheet](EXPERIMENTS.md#all-mission-campaign-flow-run-sheet-task-92-needs-live-run)
to collect the missing evidence outside it. Synthetic results add no L7 passes.

Validation (2026-11-07): Release configure/build passed with `AZTEC_GEN_DIR=`
and no configured game directory. CTest passed 49 entries and skipped six
(five private-data checks and restricted UDP processes), including passing
`campaign_flow`, `campaign_ui` and `playthrough`. Pre-commit passed for all
tracked files and the new fixture; `git diff --check` passed. The five task-92
run-sheet shell blocks and embedded Python passed syntax checks only. No live
command was executed. Changes remain uncommitted because git metadata is read-only.

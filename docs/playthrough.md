# Automated campaign playthrough

`tools/playthrough.py` exercises the independent engine on the user's campaign
maps. It discovers every case-insensitive `mission_N.mp`, sorts by numeric mission
number and runs each mission twice in fresh processes. Original data is required;
no game files or reference-derived code are distributed with the harness.

Build from the worktree root:

```sh
sh tools/bootstrap.sh --no-hooks
CCACHE_DIR="$PWD/build/ccache" cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DAZTEC_GEN_DIR=
CCACHE_DIR="$PWD/build/ccache" cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
pre-commit run --all-files
```

Run all discovered campaign missions:

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
The next run and mission still execute. Missing requested missions and duplicate
numeric mission names are configuration errors. Without `--missions`, missing
numbers in the installation are not invented or silently substituted.

Each invocation creates a new ignored directory under this worktree's `work/`.
`--out` must name a new directory there. It retains an executable snapshot with a
SHA-256 identifier, copied rule tables and optional orders, two private game
directories, per-run stdout/stderr and `engine.json`, and an incrementally updated
`summary.json`. Do not add any of these files to git. The executable is the
independent engine built here, not the reference runtime. Game resources and map
files are read-only symlinks to the installation; keep that installation stable
during both runs. Maps and Saves are ordinary private directories; profiles and
configuration files are copied, and original saves are excluded. The headless
engine only reads the symlinked inputs.

Every real game-data process uses `unshare -rn` and both
`SDL_VIDEODRIVER=dummy` and `SDL_AUDIODRIVER=dummy`. Namespace failures remain
failures; there is no fallback outside isolation. This mode opens no SDL window,
audio device or network socket and needs no loopback interface configuration.
It does not launch the reference runtime. Reference record/replay and headless
probes still follow [EXPERIMENTS.md](EXPERIMENTS.md); internal engine agreement
cannot replace that evidence.

The simulation retains the existing computer-player AI and mission trigger/path
orders. Every 120 ticks, the default exercising policy purchases the first
affordable mobile item for the human side when its queue is empty, and sends
idle armed mobile units toward their nearest live hostile object. Distance ties
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
all live mobile objects belonging to that side at execution time. Explicit IDs
are the engine's stable one-based IDs. Coordinates must be inside the loaded
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

Determinism is true only when both processes succeed and agree on terminal
status, tick count, final state hash and a rolling digest of every tick's state
hash. Failed or killed runs yield null (unverified), not true. Hash agreement is
an internal reproducibility check, not reference parity or proof of campaign
completeness. The script state hash includes path cycling/inversion flags;
hashes recorded before that correction may differ.

The harness exits zero only when every selected mission wins in both runs,
agrees deterministically, and has no script-error, unsupported-action or
unreachable-objective diagnostics. Defeat, either timeout and all failures exit
one. Configuration errors exit two. The direct engine mode exits zero for any
completed bounded simulation (including defeat/timeout) and one for input or
simulation errors; report-write failures exit two. Read the JSON outcome when
using it without the Python runner.

`ctest --test-dir build -R playthrough --output-on-failure` runs synthetic
fixtures for outcomes, hash agreement, malformed scripts/orders, impossible versus
unmet objectives, private copies, campaign summaries, crashes and timeouts. These
fixtures need no assets or namespace privileges. Only their synthetic launcher
is replaced in tests; production always invokes `unshare -rn`.

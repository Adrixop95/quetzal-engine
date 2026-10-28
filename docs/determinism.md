# Simulation determinism audit

V03 progress, 2026-10-28. These are independent-engine construction properties
and synthetic checks, not reference parity or campaign completion.

## Ordering and tick phase

Player commands are stably sorted by side, retaining submission order within a
side. All are applied and logged before pause handling. Thus a later-side
PauseOff can override an earlier-side PauseOn in the same tick. A paused tick
still advances world/simulation tick counters and the driver's animation clock;
it skips scripts, economy, AI, effects and movement. After player commands, an
active tick executes scripts/path orders, economy, AI commands (town order),
effects, object orders, repairer membership, movement, visual clocks and fog.
AI and script commands are recomputed during replay, rather than logged twice.
These are engine policies pending V02 reference-order evidence.

The replay observer runs before submitting the current tick's commands and before
`begin_tick`. Its tick 1 state is the initialized world; its tick t state follows
t-1 updates. Unordered, zero-tick and beyond-horizon logs are rejected before
initializing the destination world; an unreadable requested script is rejected.
Reinitialization clears pending commands, the command log and tick counters.

## Random streams and arithmetic

The simulation owns one explicitly named `visual_rng_`, initialized to 12345.
Its unsigned 32-bit recurrence is `state = state * 1664525 + 1013904223`; the
returned value is `state >> 8`. Spawning consumes one value for `anim_variant`;
vehicle explosions/debris and projectile impact slots consume additional values
in object/effect order. No gameplay damage, AI decision or path search currently
consumes this stream. Visual state can drive lighting, so both its cursor and
results remain hashed. Do not move these draws into rendering or invent a
reference-compatible seed/stream split without observations.

`WanderSimulation` is a separate demo with its own seed and stream. Audio uses
its own member RNG; music advances in track order. Neither is the game simulation
RNG. Camera interpolation and audio updates never advance `visual_rng_`.
Reference seeds, streams, call counts and original random functions remain open.

Production multipliers now use exact integer representations of the existing
binary32 coefficients over 2^24. A split product computes rounding without host
`long double`, FMA, or nonportable 128-bit types; values saturate at INT_MAX.
Nonpositive base costs/times return zero. Warehouse income uses the exact ratio
11184811/8388608, with a bounded result. These preserve the specified coefficients,
including their binary32 approximation, rather than replacing them with ideal
2/3, 4/3 or 4/5 ratios. Synthetic rounding and overflow controls pass. This change
does not investigate the historical lifted 161-versus-160 price artifact.
Script timer/comparison storage uses int64_t instead of platform-width `long`.

## State hash contract 2

Hashes mix integer values and ordered collection lengths, never addresses,
container capacity, C++ struct bytes or host padding. They are diagnostic digests,
not collision-proof proofs of equality. Equal immutable rules and loaded sight/
animation resources are a precondition; this is not a rules/content digest.
The engine does not enforce that content precondition at session join/save load.

Coverage now includes:

- World/simulation ticks, pause flag, visual RNG, spawn counter and pending
  command payloads/ID ordering.
- Object source, identity, ownership, HP, orders, animation clocks/variant,
  construction/harvest/repair state, movement links and previous/current positions.
- Effect identity, payload, endpoints, interpolation positions and clocks.
- Full town identity/owner/position/level, side AI/disabled-type/start inputs,
  script paths, resource cells/dimensions, queues/groups/repairers and AI state.
- Relations, mission and per-side outcomes, script-side identity, import mode,
  script program plus enabled triggers/timers/path state, and fog bytes/dimensions.
- Navigation terrain and dynamic blocker **counts**, grid version, movement
  configuration, agents, steering caches, order references and flow-field costs.
  Grid/field digests are cached at mutation/build boundaries to avoid rehashing
  immutable cost arrays every tick.

Local selection, viewing side, camera requests/serials, fog invalidation counters,
messages, audio state, replay history, diagnostics and spatial scratch arrays are
excluded. Messages/camera requests affect presentation only; replay history is
not read by simulation. Spatial arrays are rebuilt before use. Map artwork,
profiles and unresolved imported-save records are not simulation inputs once
initialization has populated the world. New state producers must extend this audit.

The changed digest is incompatible with earlier recorded engine save hashes.
The UI now refuses a replay mismatch or invalid replay before replacing the active
world. Save requests wait for a completed tick with no pending commands, so the
command-log snapshot cannot silently lose future inputs. A locally paused game
with pending commands must resume before that deferred save can complete. File format compatibility, bounded replay budgets, content digests and
atomic save writes still belong to V16.

## Reproducible checks and remaining evidence

`determinism_state` checks omitted-field sensitivity, presentation exclusions,
cross-side and same-side ordering, exact before-command alignment, replay-log
validation, script-program differences and navigation/configuration differences. It also checks four known spawn-variant draws from
the initial seed.
`determinism_processes` runs a synthetic 14,400-tick world (20 simulation minutes):
training/spawns, conflicting pause commands, movement, combat/death, a save at
tick 7,201, fresh-process restoration and continued inputs. It compares every tick
against another process using a different viewing side, camera update count,
interpolation fraction and audio volume settings, and against full command-log
replay. It checks both differing and truncated trace controls. No assets or
reference binary are needed. Presentation checks exercise camera/interpolation
and silent audio updates; they do not verify a renderer or live audio device.

Run the portable tests and optionally export a synthetic transcript:

```sh
AZTEC_DETERMINISM_TRACE="$PWD/build/determinism.trace" \
  ctest --test-dir build -R 'determinism|oracle_packets|parity_controls' --output-on-failure
python3 tests/test_determinism_processes.py --compare \
  build/gcc.trace build_clang/clang.trace
```

The build workflow exports transcripts on GCC/Linux, MSVC/Windows,
MinGW/Windows and AppleClang/macOS and requires exact comparison in a dependent
job. Adding that job does not establish that remote CI has passed. Campaign
coverage, native Windows execution, renderer/audio-device variations and the V02
reference-order/RNG observations remain required before V03 can be closed.

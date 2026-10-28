# Project handoff

Snapshot: 2026-10-26. Read this with root `CLAUDE.md` / `AGENTS.md`,
[SETUP.md](SETUP.md), [EXPERIMENTS.md](EXPERIMENTS.md) and [tasks/](../tasks/README.md).
No chat history, private executable, generated source or old assistant session is
required to build the independent engine and run its synthetic checks.
The project is not yet a behaviorally complete replacement for the game.

## Current state

Two implementations serve different purposes:

- `runtime/` hosts locally lifted x86 code with Win32/DirectX, SDL and network
  support. `tools/build_recomp.sh` plus Ghidra produce the private reference
  executable. `runtime/oracle.c` records/replays lockstep commands
  (`RT_CMD_LOG`, `RT_CMD_REPLAY`) and hashes reference state (`RT_STATE_LOG`).
  Unit/path logs, scripted input, object scans and screenshots support experiments.
  `tools/corpus.sh` provides a historical multi-mission corpus workflow. Its fixed
  build/game paths and writable shared installation need review before use;
  follow the private-copy isolation rules in EXPERIMENTS.md.
- `src/engine/` is independently written C++20: SDL rendering with an atlas,
  camera zoom/scroll, interpolation, terrain/overlays, sprites, fog and minimap;
  selection, movement/attack commands, groups, production menus, construction,
  combat, training, script execution and a deterministic computer town planner. Front-end,
  briefings, ffmpeg intro playback, audio mixer, command-log saves/replay and
  scripted playtests exist. Features working internally do not establish parity.
  The `MoveSystem` interface isolates deterministic pathfinding and collision work.

`src/core/` covers XBE archives, palettes, ordinary and alpha sprites, terrain,
passability/resource/visibility maps, INI missions, script parsing, briefings,
UGI ids, strings, player/settings files, audio and save-container readers.
Animation resources and kind-specific slot/hold-frame readers are implemented;
90 object types and 11 factory kinds have metadata. The older
[rebuild checklist](REBUILD_CHECKLIST.md) predates much of this coverage: its
unchecked animation, string, audio and mission-format entries are not proof that
readers are missing. Consult implementation/tests and [formats.md](formats.md).
Original save-container parsing does not mean the engine can resume an original
simulation save: engine saves currently restore by replaying its own command log.

There are six CTest entries: `formats` and `engine` use private game data and skip
with code 77 when absent; `path`, `engine_unit`, `fuzz` and `savegame` use synthetic
inputs. A fresh Release build in `build_handoff/` passed all four independent
checks on this snapshot; both game-data entries skipped. No reference runtime,
Ghidra lift or live game-data check was verified in this worktree. No game files
are supplied here. CI builds with `AZTEC_GEN_DIR=` and runs CTest plus pre-commit.

## Decisions and rationale

- Use static recompilation as a local oracle: it exposes repeatable commands,
  state and rendering for experiments while the independent engine develops.
  Lifted/decompiled code stays private. The oracle itself can differ from the
  original x86 behavior: price conversion yielding 161 rather than 160 is a
  documented example. Inspect original instructions when recovered types or
  floating-point behavior are ambiguous.
- Use a deterministic 12 Hz simulation with fixed-point positions, stable command
  processing and state hashes. Rendering interpolates independently. This makes
  replay and future lockstep networking feasible; it does not establish packet
  compatibility or completed multiplayer support.
- GPL-3.0 is the repository license (`LICENSE`); original game rights and assets
  remain separate. This PUBLIC repository contains independent code and factual
  format/behavior documentation, never the executable, assets or derived code.
- Gitflow protects integration/releases: feature branches and PRs into develop,
  required build/pre-commit gates, squash merges, no direct main/develop pushes.
  English text and focused diffs make review and assistant handoffs repeatable.

## Evidence boundaries and known gaps

[engine_rules_status.md](engine_rules_status.md) is the implementation inventory:
V = verified reference fact or construction property (read the notes),
M = measurement, P = placeholder. [rules_summary.md](rules_summary.md) describes
reference knowledge: V = inspected/observed, G = inference, O = unresolved.
These alphabets have different meanings. A V reference rule may still be P or
missing in the engine. Internal replay tests compare engine runs to themselves;
reference and engine hashes use different representations and are not directly
comparable. Historical reference no-input runs even had three unexplained
transient hash differences out of 214 ticks.

Recent reference findings supersede older hypotheses: Marine primary HP is d1,
not d4 (secondary HP); economy depends on resource maps and day-clock events;
prices/times depend on town level and chosen speed; visibility uses interpolated
light masks; normal tactic assignment can omit ordinary buildings even when a
weapon can damage them. The engine still has placeholder HP loading, damage,
range, income, training/refunds, AI composition, placement and tactic assignment. Garrison, repair,
town upgrades, several commands/scripts and RNG parity remain incomplete.
Original WaveSearch equivalence, projectile timing and complex weapons remain
open; networking is not implemented in the independent engine.

[sprites_mapping.md](sprites_mapping.md) separates R resource facts, C inspected
selection behavior, T historical animation traces and P screenshot comparisons.
Older tolerant matches are not exact pixel matches. Metadata now covers complex
kinds, construction, effects, height, draw order and light masks; renderer
integration and synchronized reference comparisons remain incomplete. Unknown
effect owners 104/153, alpha compositing, wind, lighting interpolation and several
attack/death/script phases still need evidence. The engine guide contains older
sprite/lighting approximations; prefer the newer evidence labels when they conflict.

The 2026-10-25 live probes failed during namespace interface/socket setup in a
restricted environment. This verifies neither immunity nor animation behavior.
Do not bypass required isolation to obtain results. A new unrestricted machine
must rerun the documented probes and retain failures as failures.

## Prioritized roadmap

The [v1 roadmap](ROADMAP.md) defines release acceptance, remaining tasks and rough
sizes. It reconciles the inventory with current tests and records the pending
open-PR review; use it for release scope and the list below for investigation order.

This order reconciles [REWRITE.md](REWRITE.md),
[REBUILD_CHECKLIST.md](REBUILD_CHECKLIST.md), the engine status, rules summary and
sprite mapping. The first six items have complete prompts in `tasks/queue/`.

1. Establish a reproducible reference record/replay baseline and normalized
   engine/reference comparison. Diagnose transient differences and lifted
   arithmetic artifacts before treating hashes as an oracle verdict.
2. Correct descriptor-driven HP and weapon mechanics: primary versus secondary
   HP, regeneration, category multipliers, range, reload and projectile timing.
   Keep unresolved weapon behavior explicit and preserve synthetic replay checks.
3. Replace placeholder economy and production with resource-map yields, day-clock
   schedules, town-local prerequisites, speed/price branches and tested rounding.
4. Integrate the established animation/render contracts: kind-specific sequences,
   secondary sprites, construction, height, common sorting and light/vision masks;
   validate synchronized exact and tolerant screenshot metrics separately.
5. Implement verified garrison/repair/destroy and town upgrades with stable command
   semantics and complete deterministic state hashing. Do not invent boarding
   capacities or a universal population cap from unrelated queue limits.
6. Validate the deterministic AI town planner against the reference. Town states,
   economic requests, autonomous waves and defence reactions now have synthetic
   coverage; composition, placement, threat entry and launch thresholds remain
   explicit fallback policies. Complete town queues/commands and tactic assignment.
7. Differentially validate movement classes, collisions and WaveSearch behavior;
   then close remaining command ordering and RNG gaps before lockstep transport.
8. Add network protocol/lobby support, original-save interoperability, remaining
   UI/audio fidelity and campaign/editor/skirmish coverage. Keep portable builds,
   malformed-input tests and release checks passing at each stage.

Before each task: read its evidence docs, build/test the baseline, inspect current
code rather than assuming this snapshot is current, and record what was actually
verified. Afterward: update status labels and leave an English summary of changed
files and checks. Archive the task after its PR merges as described in tasks/README.md.

## AI implementation update

The planner in `src/engine/ai/` is a pure world/state-to-command function integrated
through `sim/ai.cpp`. AI state and per-town harvest/spending accounting participate
in simulation hashes and command-log replay. `ai_unit` covers decisions and
synthetic `.scn` orders; `ai_skirmish` compares two 14,400-tick runs and replay.
See the computer-player section in `engine_rules_status.md` for evidence boundaries
and missing task 01/05 dependencies. A private isolated reference probe failed at
namespace network setup; no reference parity was verified.

Validation: Release build, ten synthetic CTest entries (including the AI skirmish),
and both private game-data entries passed. The game-data checks used dummy SDL
inside `unshare -rn` with a private installation copy. Pre-commit hooks were run
from a locally cached environment; remote fetching was unavailable.

## V11 sustained-opposition update (2026-10-28, partial)

The planner now sends idle autonomous reinforcements to an ongoing town alert,
including while production is busy, without resetting the current plan or active
combat orders. This remains an explicit fallback policy. Shared-cash town tests
check independent speed selection and pricing; the synthetic mission cash-pressure
scenario and repeated 14,400-tick soak require autonomous waves, firing, losses
and production in every five-minute interval. Deliberately durable synthetic halls
keep both sides alive; this does not measure campaign balance. Every tick matches
across viewing sides and command-log replay. See the computer-player inventory in
[engine_rules_status.md](engine_rules_status.md) for counts and evidence limits.

Release build, synthetic CTest and pre-commit gates pass; four private-data checks
and restricted UDP process creation skip in the default suite. Separate format
and engine checks pass with the private game copy under dummy SDL and
`unshare -rn`. The isolated reference mission probe fails at loopback setup
before launch. Separate paired engine mission 1/4 smoke reports match but end
in defeat at ticks 1196/1474; neither completes the twenty-minute horizon or a
winning route. V05-V10/reference observer dependencies and full V11 campaign
acceptance remain open; only the evidenced V11 subitems are ticked.

# Roadmap to v1

Planning snapshot: 2026-10-28. **v1 means the whole campaign is playable from
the user's original data without crashes, with deterministic simulation, the
engine's own save/load, and working Linux and Windows builds.** The independent
engine must run without the reference runtime or locally generated source.
Original assets remain external and are never included in releases.

## Release acceptance

- [ ] Complete every campaign mission (currently mission_1 through mission_15),
  including briefings, objectives, scripted events, victory, defeat, restart and
  progression to the final ending. Record a successful player-controllable route
  for each mission; loading a map or reaching a timeout does not count.
- [ ] Finish full campaign runs on Linux and Windows without crashes, hangs,
  unsupported required script actions or objectives blocked by engine behavior.
  Verify ordinary controls and defeat/retry paths as well as winning runs.
- [ ] Replay identical initial data, rules, seeds and commands with equal state
  hashes at every tick, across repeated processes, supported Linux/Windows builds,
  viewing sides, rendering rates and audio settings. Cover all future-affecting
  state; matching a partial hash is insufficient.
- [ ] Save during each mission, exit the process, load in a fresh process and
  finish it. Continuation must match an uninterrupted run. Reject corrupt,
  incompatible or mismatching saves safely and preserve the active game.
- [ ] Publish tested Linux and Windows packages that find the user's installation
  and include needed engine rules, runtime dependencies and licenses. Require
  successful build, synthetic CTest and pre-commit gates before release.

Determinism and campaign playability are distinct from exact reference parity.
Use normalized reference comparisons to resolve behavioral blockers; neither
engine self-replay nor tolerant screenshots establishes whole-game equivalence.

## Evidence and integration status

The checklist below reconciles [engine_rules_status.md](engine_rules_status.md),
[HANDOFF.md](HANDOFF.md), [render_integration.md](render_integration.md),
[parity.md](parity.md), the [task queue](../tasks/README.md), current sources and
[CTest registrations](../CMakeLists.txt). V/M/P labels retain their source
meanings; a verified formula does not establish integrated mission behavior.
Queued prompts can describe work that has already partially landed.

Open PR review is **pending**. On this snapshot `gh pr list --state open` could
not reach the GitHub API, and the web fallback could not retrieve the
[open PR list](https://github.com/Adrixop95/quetzal-engine/pulls). No PR is assumed
open, merged or complete on that basis. Before scheduling implementation, refresh
that list, record each relevant PR's scope and dependencies here, and reconcile
its diff and checks with this checklist. Unmerged dependencies must not be
treated as available. Documentation, synthetic scenarios and independent work
can proceed while those dependencies remain unavailable.

Local history already includes partial town commands (#25), the map viewer
(#26), mission parity tooling (#27), original-save import (#28), two-peer
lockstep (#29) and animation states (#31). These are historical integration
references, not a current open-PR inventory. Their remaining limitations are
listed below rather than treating the features as absent.

Baseline on this snapshot: Release configure/build and all pre-commit hooks
passed. Of 16 CTest entries, 12 passed, three game-data entries skipped and
`lockstep_processes` failed because UDP socket creation was denied. Hook caches
were copied into ignored build output to run without network access. No live
campaign or reference observation was obtained for this documentation task.

## Remaining v1 work

All rows are unchecked release work, including validation of existing code.
Descriptions are owner-neutral. Sizes are rough focused engineering effort:
**S** = 1-3 days, **M** = 4-10 days, **L** = 2-4 weeks, **XL** = several weeks
requiring smaller tasks. They include implementation and regression checks,
exclude waiting for access/review, and are not calendar commitments or additive
estimates. Reference-dependent work may grow when evidence changes.

### Establish measurements and determinism

| ID | Task and completion evidence | Size | Existing basis / dependencies |
| --- | --- | --- | --- |
| V01 | Refresh open PR status and split overlapping or partially landed queue tasks into concrete remaining changes; retain links and required merge order. | S | PR access pending; task queue and local history above. |
| V02 | Complete oracle packet replay, audited object identity, normalized field observers and before-command tick alignment. Demonstrate reference repeatability, diagnose transient pool differences and lifted arithmetic artifacts, and report the first semantic mismatch. | XL | Tasks [01](../tasks/queue/01-oracle-parity.md) and [11](../tasks/queue/11-oracle-parity-missions.md); strict comparator/mission runner exist, adapters do not. Requires a host permitting isolated reference runs. |
| V03 | Audit command ordering, RNG streams and full future-affecting state hashing. Add long replay and save-continuation scenarios across Linux/Windows, compilers, viewing sides and presentation settings; resolve every divergence. | L | `engine_unit` and simulated `lockstep` cover selected scenarios, not campaign or cross-platform determinism. Build on V02 observations where reference ordering is uncertain. |

### Close campaign simulation gaps

| ID | Task and completion evidence | Size | Existing basis / dependencies |
| --- | --- | --- | --- |
| V04 | Validate initialization for all campaign object kinds: ownership, town/tactic identity, descriptor HP, secondary HP and regeneration transitions/overshoot. Replace mission-maximum HP fallbacks and establish missing weapon/armor mappings, including Hypno behavior. | L | Descriptor combat tests cover selected arithmetic and Marine fields; task [02](../tasks/queue/02-descriptor-combat.md), V02. |
| V05 | Implement and compare required movement classes, boat/air movement, speeds, radii, footprints, collisions and path decisions. Exercise crowded routes, narrow passages and unreachable destinations without hangs. | XL | Partial: descriptor ground classes/speeds and fixed-point air orders have synthetic integration/replay checks. `path`, `path_bench_smoke`, `zpath_tools` cover engine routes. Reference probe denied namespace setup; boat routes, Bird speeds, radii, footprints, collisions and WaveSearch comparison remain open. V02/V04 are incomplete; see [movement progress](rules_movement.md#8-v05-partial-integration-2026-10-28). |
| V06 | Replace simulation sight/fog and nearest-enemy acquisition placeholders with per-type masks, daylight, exploration, visibility thresholds and tactic/weapon eligibility. Verify target and town selection under fog. | L | Task [15](../tasks/queue/15-sight-fog-rules.md); renderer light masks already exist but do not establish AI sight. V02/V04. |
| V07 | Complete required damage classes, regeneration/combat state interactions, range geometry, firing/reload scheduling, composite weapons and projectile impact timing. Resolve trajectory, interception, burn, splash/friendly damage, death and corpse semantics with focused scenarios. | XL | Tasks [02](../tasks/queue/02-descriptor-combat.md), [18](../tasks/queue/18-projectile-collision.md); projectile replay exists, unresolved descriptor weapons are disabled. V02/V04-V06. |
| V08 | Validate economy clock phase, activation/suspension, construction placement/completion and training/spawn timing. Replace side-wide queues and provisional refunds with measured town-local production behavior. | L | Task [03](../tasks/queue/03-economy-production.md); resource-map income, price formulas and prerequisites already have synthetic coverage. V02/V04. |
| V09 | Complete repair priority/allocation and destruction service using harvested credits; finish hall upgrades and non-map farms/mines, costs, timers, build-tree changes and command/UI integration. Verify save/replay through completion. | L | Tasks [05](../tasks/queue/05-town-commands.md), [17](../tasks/queue/17-town-upgrades-structures.md); garrison extraction/list toggles exist, service and upgrades remain incomplete. V08. |
| V10 | Implement campaign-required attack modes, group/follow/selection command semantics and evidenced population/difficulty rules. Remove invented limits such as AI cap 80; do not infer a universal cap from queue limits. | L | Task [16](../tasks/queue/16-population-difficulty.md); several original command types are only logged. V02/V04/V08. |
| V11 | Implement deterministic town planning, strategist speed selection, defence reactions and autonomous attack waves. Exercise economy/build orders and sustained opposition in mission scenarios and a repeated 20-minute run. | XL | Tasks [06](../tasks/queue/06-ai-mission-behavior.md), [13](../tasks/queue/13-ai-from-rules.md); current AI is a purchase skeleton. V05-V10. |
| V12 | Audit every campaign script condition/action and implement required boat/Johnka/submarine, build/move and camera commands. Verify trigger enablement/order, timers, paths, alliances, gifts, defence identity and victory/defeat against mission objectives. Require zero unsupported required actions. | XL | `engine_unit` has script replay checks; semantics remain P and the script runner counts unsupported actions. V02/V05/V10/V11; inventory and synthetic fixtures can precede adapters. |

### Make the campaign usable and resumable

| ID | Task and completion evidence | Size | Existing basis / dependencies |
| --- | --- | --- | --- |
| V13 | Finish simulation producers for required transport/animation phases, turret aim, steering, aircraft altitude and construction/destruction effects. Validate attack/death/overlap/night scenes and readable visibility/minimap behavior. Track exact and tolerant pixel metrics separately. | L | Tasks [04](../tasks/queue/04-render-contracts.md), [12](../tasks/queue/12-animation-states.md); `render_contracts`, `animation_headless` and `zspr_tools` cover existing integration. V05-V07/V12. |
| V14 | Verify all campaign controls and feedback: command panels, build/repair/upgrade controls, groups, hotkeys, tooltips, messages, objectives, briefings, outcome screens and next-mission/profile progression. Fix unavailable actions and unreadable UI across supported window sizes. | L | Task [14](../tasks/queue/14-ui-parity.md); front end, panels and next-mission flow exist but have no full campaign acceptance run. V09/V10/V12/V13. |
| V15 | Complete required speech/subtitles, unit/order/combat/building/UI events, state loops, volume controls, visibility attenuation and mission music. Test event logs and confirm audio settings never change simulation results. | M | Task [20](../tasks/queue/20-audio-parity.md); mixer/banks/frame sounds exist, event coverage and selection policies remain partial. V03/V12/V14. |
| V16 | Harden engine save/load: version/content compatibility, bounded parsing, safe writes, explicit encoding and refusal of replay hash mismatch before replacing the active world. Load from a fresh session, retain mission/camera/progression and verify continued play after long saves on both platforms. | L | `engine_unit` checks AZS2/log roundtrips; current UI uses one mission-local slot and accepts a replay hash mismatch with a warning. `savegame` tests the original container, not end-to-end engine saving. V03 and every new simulation state producer. |

### Prove and ship v1

| ID | Task and completion evidence | Size | Existing basis / dependencies |
| --- | --- | --- | --- |
| V17 | Make native Linux and Windows builds/tests required CI gates. Resolve GCC/POSIX assumptions, SDL discovery, paths/case handling and serialization portability; isolate optional POSIX networking or supply a Windows adapter so default builds succeed. | L | Task [09](../tasks/queue/09-portability.md); current build CI is Linux-only, Windows release job is experimental and `aztec_net` uses POSIX UDP. Can proceed alongside simulation work. |
| V18 | Produce self-contained engine packages with external-data setup instructions, rules metadata, dependencies and licenses; smoke-test on clean Linux/Windows machines without the oracle/generated source. Check missing or malformed data produces useful errors. | M | Release workflow exists but currently packages the executable, README and license only. V17; audit runtime data lookup and optional ffmpeg requirements. |
| V19 | Add the full campaign harness and mission coverage matrix: completion route, expected outcome, unsupported actions, unreachable objectives, timeout/crash reports, paired tick hashes and mid-mission saves. Cover all 15 missions, defeat/retry and final progression with automated scenarios plus interactive acceptance. | XL | Task [19](../tasks/queue/19-campaign-playthrough.md); no playthrough harness exists. Historical corpus has only nine recordings over bounded intervals. Harness/inventory can start now; successful completion depends on V03-V16. |
| V20 | Fix campaign/soak failures, run malformed-input and sanitizer checks, and complete required tests on hosts that permit sockets and private game-data runs. Publish an English release checklist with platform, mission and save/load results and outstanding limitations. | L | `fuzz` and [ROBUSTNESS.md](../tests/ROBUSTNESS.md) cover loaders, not all runtime failures. V17-V19; no skipped or denied test may stand in for required release evidence. |

Start V01-V03, V17 and the V19 mission inventory early. Use V02 to prioritize
first behavioral divergences rather than assigning causality from engine hashes.
Economy, scripts, AI and their UI depend on one another; split the XL rows into
reviewable scenarios and update status after each merged change. Close the
campaign matrix and fresh-process save continuations before declaring v1.

## Deferred work and fidelity follow-ups

These gaps remain visible but are not required by the v1 definition. Promote any
item that actually blocks campaign objectives, readability, determinism, safe
loading or a supported build into the required checklist with a reproducer.

| Task | Size | Current boundary |
| --- | --- | --- |
| Complete original `.sav` resume, including orders, research and script state, and define interoperability/export policy. | XL | Task [07](../tasks/queue/07-original-saves.md); parser and partial importer with `save_import` checks exist. v1 requires the engine's own saves. |
| Integrate multiplayer lobby/session lifecycle, content identity, disconnect handling and Windows transport; separately investigate original network compatibility if desired. | XL | Task [08](../tasks/queue/08-multiplayer-lockstep.md); two-peer transport and simulated checks exist, front-end integration is absent and real UDP validation is restricted here. |
| Complete exact alpha/shadow blending, per-pixel lighting, wind/overlay producers, effect owners 104/153 and remaining cosmetic animation/audio parity. | XL | Renderer and rules inventories retain unresolved evidence; tolerant matches do not close exact-pixel work. Campaign-visible blockers belong in V13-V15. |
| Add native macOS support and required platform checks. | L | Experimental release job exists; Linux and Windows are the v1 platforms. |
| Extend editor/custom-map/skirmish coverage and tools beyond campaign needs. | L | Map inspection/dump tooling exists; it does not establish a playable editor or complete skirmish behavior. |

Reference experiments must follow [EXPERIMENTS.md](EXPERIMENTS.md): private
writable game copies, read-only resource symlinks, both SDL dummy drivers,
`unshare -rn` and hard timeouts. Namespace/socket setup failures remain failures;
rerun on a suitable host. Keep assets, executable-derived code, recordings,
screenshots and state dumps in ignored private output. Public progress should
contain independent descriptions, coverage and test outcomes only.

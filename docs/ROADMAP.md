# Roadmap to v1

Planning snapshot: 2026-10-28, reconciled through `develop` / `origin/develop`
`79fa121` (#57). This checkout is `a28c60c`; its only additions over that develop
snapshot are queue prompts 30-35. **v1 means the whole campaign is playable from
the user's original data without crashes, with deterministic simulation, the
engine's own save/load, and working Linux and Windows builds.** The independent
engine must run without the reference runtime or locally generated source.
Original assets remain external and are never included in releases.

## Release acceptance

The five release gates remain open. Checked foundations below have code and
synthetic tests; they do not substitute for mission or platform acceptance.

- [ ] Complete every campaign mission (mission_1 through mission_15), including
  briefings, objectives, scripted events, victory, defeat, restart and progression
  to the final ending. Record a successful player-controllable route for each
  mission; loading a map or reaching a timeout does not count. V12/V14/V19.
- [ ] Finish full campaign runs on Linux and Windows without crashes, hangs,
  unsupported required script actions or objectives blocked by engine behavior.
  Verify ordinary controls and defeat/retry paths as well as winning runs.
  V04-V15/V17/V19/V20.
- [ ] Replay identical initial data, rules, seeds and commands with equal state
  hashes at every tick across repeated processes, supported Linux/Windows builds,
  viewing sides, rendering rates and audio settings. Cover all future-affecting
  state and new producers; complete native campaign evidence. V03.
- [ ] Save during each mission, exit the process, load in a fresh process and
  finish it. Continuation must match an uninterrupted run, retaining mission,
  camera and progression. Verify corrupt/incompatible/hash-mismatching save
  refusal preserves the active game on both platforms. V16/V19.
- [ ] Publish tested Linux and Windows packages that find the user's installation
  and include needed engine rules, runtime dependencies and licenses. Obtain
  green native builds, synthetic CTest, pre-commit and extracted-package checks;
  verify required CI gates before release. V17/V18/V20.

Evidenced foundations for those gates:

- [x] Campaign discovery, paired headless runs, outcome/error/timeout reporting
  and per-tick hash digests: `tools/playthrough.py`, engine `app/headless.cpp`
  and `playthrough` synthetic checks (#38). This is not a completion-route matrix.
- [x] Hash contract 2, ordering/RNG controls and 14,400-tick fresh-process
  replay/save-continuation scenarios: `sim/game_sim.cpp`, `test_determinism.cpp`,
  `determinism_state` and `determinism_processes` (#49).
- [x] AZS3 bounded parsing, compatibility checks, atomic writes and transactional
  replay refusal: `sim/savegame.cpp`, `engine_save` and `engine_save_processes`
  (#53). These cover synthetic long saves and 300 continuation ticks.
- [x] Portable resource lookup and explicit little-endian/unaligned access:
  core/engine portability changes and `portability` (#30). Native CI jobs exist;
  a workflow definition and local Linux pass do not prove a Windows run.

Determinism and campaign playability are distinct from exact reference parity.
Use normalized reference comparisons to resolve behavioral blockers; neither
engine self-replay nor tolerant screenshots establishes whole-game equivalence.

## Evidence and integration status

This refresh reconciles [engine_rules_status.md](engine_rules_status.md),
[HANDOFF.md](HANDOFF.md), [render_integration.md](render_integration.md),
[parity.md](parity.md), queue prompts 01-29, current sources/tests and
[CTest registrations](../CMakeLists.txt). Later dated status appendices and code
supersede older rows: sight masks, attack modes, town queues, audio loops and
saves are no longer wholly absent. V/M/P labels retain their source meanings;
a verified formula does not establish integrated mission behavior.

### Landed PR inventory

The following PR numbers and commits are present in develop's squash history.
Links identify the actual integrations, not completion claims based on titles.
`gh pr list --state merged --base develop --limit 100` (also attempted without
the base filter) failed to connect to `api.github.com` during this refresh; the
web API fallback was unavailable too. This inventory is therefore verified from
local history and integrated files, not a successful remote API response.
Newer remote merges, remote check results and branch protection remain unverified.
There is no assumed unmerged implementation dependency.

| Queue prompt(s) | Landed PR / commit | Integrated scope and remaining boundary |
| --- | --- | --- |
| 01 oracle; 11 mission parity; 22 determinism | [#27](https://github.com/Adrixop95/quetzal-engine/pull/27) `7c9c57a`; [#49](https://github.com/Adrixop95/quetzal-engine/pull/49) `092b736` | Mission runner/comparator, packet controls, observer boundary and synthetic state audit. No separate task 01 implementation merge is identified; audited reference identity/field adapters and successful repeatability remain open. |
| 02 descriptor combat | [#19](https://github.com/Adrixop95/quetzal-engine/pull/19) `3685755` | Selected HP fields and damage/reload arithmetic; not all campaign kinds, regeneration states or weapons. |
| 03 economy; 24 economy/repair | [#20](https://github.com/Adrixop95/quetzal-engine/pull/20) `82cb3e0`; [#57](https://github.com/Adrixop95/quetzal-engine/pull/57) `79fa121` | Resource yields, pricing, prerequisites, independent town queues and repair/destruction allocation. Timing, pause/capacity/refund and HP parity remain open. |
| 04 render contracts; 12 animation; 26 producers | [#21](https://github.com/Adrixop95/quetzal-engine/pull/21) `5c1f451`; [#31](https://github.com/Adrixop95/quetzal-engine/pull/31) `b08e411`; [#52](https://github.com/Adrixop95/quetzal-engine/pull/52) `7347c50` | Render/animation contracts, angles, Boat phase completion, pause clocks and construction overlay. Transport initiation, altitude and reference scenes remain open. |
| 05 town commands; 17 upgrades | [#25](https://github.com/Adrixop95/quetzal-engine/pull/25) `9ca5fa3`; [#36](https://github.com/Adrixop95/quetzal-engine/pull/36) `a2ef81d` | Garrison, service modes/lists, hall upgrades and flag-0 structure production. Service allocation follows in #57; placement/transition parity remains open. |
| 06 mission AI; 13 AI from rules | [#32](https://github.com/Adrixop95/quetzal-engine/pull/32) `e81fabf` | Pure deterministic town planner, adapter, defence/waves and repeated 20-minute skirmish. No separate task 06 merge is identified; composition, placement and mission behavior remain partial. |
| 07 original saves | [#28](https://github.com/Adrixop95/quetzal-engine/pull/28) `eb93f87` | Partial original-save importer with synthetic checks; complete simulation resume is deferred. |
| 08 lockstep | [#29](https://github.com/Adrixop95/quetzal-engine/pull/29) `c5ce49e` | Two-peer layer, simulated tests and POSIX UDP process runner; lobby and Windows transport deferred. |
| 09 portability | [#30](https://github.com/Adrixop95/quetzal-engine/pull/30) `3f670c0` | MSVC/MinGW/AppleClang matrix, paths, encoding/alignment fixes and optional UNIX UDP. Native results/gate enforcement need verification. |
| 10 map viewer | [#26](https://github.com/Adrixop95/quetzal-engine/pull/26) `5847163` | Standalone browse/dump inspector and synthetic checks; not an editor or campaign solver. |
| 14 UI | [#48](https://github.com/Adrixop95/quetzal-engine/pull/48) `f4dfd8d` | Anchored panels, scaling/wrapping, hit regions/hotkeys and synthetic screenshots. Full campaign controls/progression and reference comparison remain open. |
| 15 sight/fog | [#43](https://github.com/Adrixop95/quetzal-engine/pull/43) `9844225` | Per-type masks, brightness/exploration and visibility gating. Tactic selection and renderer consumers remain partial. |
| 16 population/difficulty/modes | [#44](https://github.com/Adrixop95/quetzal-engine/pull/44) `0e52ee5` | Four attack modes partially handled; production above 110 units and map Difficulty exclusion tested. Follow/group semantics and reference limits remain open. |
| 18 projectile collision | [#35](https://github.com/Adrixop95/quetzal-engine/pull/35) `48ef0d3` | Measurement protocol and failed-probe record only; this PR changed no simulation code. Trajectory/collision/burn/splash work remains open. |
| 19 campaign harness | [#38](https://github.com/Adrixop95/quetzal-engine/pull/38) `6323687` | Headless engine, paired mission runner and synthetic diagnostics. No proven full campaign completion matrix. |
| 20 audio | [#39](https://github.com/Adrixop95/quetzal-engine/pull/39) `e50065b` | Frame events, loops, visibility cleanup, music order and event-log tests. Speech/subtitles/announcements and settings remain incomplete. |
| 21 roadmap | [#37](https://github.com/Adrixop95/quetzal-engine/pull/37) `8b0e955` | Initial release plan; this refresh reconciles subsequent landings. |
| 23 movement | [#50](https://github.com/Adrixop95/quetzal-engine/pull/50) `7b353ed` | Descriptor ground classes/speeds and fixed-point air orders; boats, radii and WaveSearch parity remain open. |
| 25 campaign scripts | [#51](https://github.com/Adrixop95/quetzal-engine/pull/51) `7d4298f` | Full script inventory and synthetic trigger/action contracts; 19 required transport sites still unsupported. |
| 27 save hardening | [#53](https://github.com/Adrixop95/quetzal-engine/pull/53) `3208dd1` | AZS3 compatibility, bounded parsing, atomic writes and fresh-process continuation; campaign/platform acceptance remains open. |
| 28 packaging | [#54](https://github.com/Adrixop95/quetzal-engine/pull/54) `e64a439` | Rules/dependency/source/license bundling and clean-runner workflow; native artifacts and smoke results remain unverified here. |
| 29 sanitizers | [#55](https://github.com/Adrixop95/quetzal-engine/pull/55) `dd863da` | Malformed-input expansion, overflow fixes and ASan/UBSan option/job; hosted leak checks and campaign soak remain open. |

Related foundations include path work #15 and effects #18. Queue/orchestration
PRs #24, #34, #46 and #56 do not prove completion of the prompts they schedule.
Prompts remain in `tasks/queue/` after landing; filename presence is not evidence
that work is absent, and a merged prompt implementation may still be partial.

Baseline for this documentation refresh: `bootstrap.sh --no-hooks`, Release
configure/build with `AZTEC_GEN_DIR=` / `GAME_DIR=` and a worktree-local ccache,
then CTest and all-file pre-commit passed. Of 31 CTest entries, 26 passed; four
private-data entries and `lockstep_processes` skipped (restricted UDP). These are
not live game-data, campaign, reference, native Windows or hosted CI results.
No reference/game run was needed for this documentation-only change.

## Remaining v1 work

No full implementation/release row is closed by a partial merge. Each row names
its tested foundation and splits the remaining scope into concrete pieces.
**S** = 1-3 days, **M** = 4-10 days, **L** = 2-4 weeks, **XL** = several weeks
requiring smaller tasks. Sizes apply to the individual remaining pieces, include
regression checks, exclude waiting for access/review and are not additive calendar
commitments. Reference-dependent work may grow when evidence changes.

### Establish measurements and determinism

| ID | Tested foundation | Concrete remaining work and size | Dependencies |
| --- | --- | --- | --- |
| V01 | This develop/PR/prompt reconciliation; see the inventory above. | Refresh remote merged PRs/checks and verify required gate settings (S); reconcile any newer changes before scheduling (S). | GitHub access; do not infer green CI from a squash title. |
| V02 | Packet parser/readback controls, strict comparator, mission runner and before-command replay observer; `oracle_packets` / `parity_controls` (#27/#49). | Build audited object/ownership/HP observers and packet translation (L); align normalized fields/ticks and prove repeated reference replay (L); diagnose transient pools/lifted arithmetic and report first semantic mismatch (M). | Tasks 01/11/22 remain partial; suitable isolated reference host. [Replay audit](parity.md#v02-replay-audit-2026-10-28). |
| V03 | Hash contract 2, fixed-width timers/integer arithmetic and long fresh-process replay/save controls; `determinism_state` / `determinism_processes` (#49). | Verify native cross-compiler transcripts and presentation/audio variants (M); extend state audit and long campaign continuation to every new producer (L); resolve original ordering/RNG streams (L). | V02 for reference semantics; V04-V16 for final campaign coverage. [Audit](determinism.md). |

This refresh reconciles all V01-V20 rows with local develop, queue 01-29 and
integrated sources/tests, and enumerates landed PRs and remaining work.

- [ ] Verify remote inventory, CI results and required checks before closing V01.
- [x] Add synthetic replay boundaries, hash sensitivity and long continuation
  controls (#49); V02 reference adapters and V03 campaign/platform acceptance stay open.

### Close campaign simulation gaps

| ID | Tested foundation | Concrete remaining work and size | Dependencies |
| --- | --- | --- | --- |
| V04 | Validate initialization for all campaign object kinds: ownership, town/tactic identity, descriptor HP, secondary HP and regeneration transitions/overshoot. Replace mission-maximum HP fallbacks and establish missing weapon/armor mappings, including Hypno behavior. | L | Descriptor combat tests cover selected arithmetic and Marine fields; task [02](../tasks/queue/02-descriptor-combat.md), V02. |
| V05 | Implement and compare required movement classes, boat/air movement, speeds, radii, footprints, collisions and path decisions. Exercise crowded routes, narrow passages and unreachable destinations without hangs. | XL | Partial: descriptor ground classes/speeds and fixed-point air orders have synthetic integration/replay checks. `path`, `path_bench_smoke`, `zpath_tools` cover engine routes. Reference probe denied namespace setup; boat routes, Bird speeds, radii, footprints, collisions and WaveSearch comparison remain open. V02/V04 are incomplete; see [movement progress](rules_movement.md#8-v05-partial-integration-2026-10-28). |
| V06 | Replace simulation sight/fog and nearest-enemy acquisition placeholders with per-type masks, daylight, exploration, visibility thresholds and tactic/weapon eligibility. Verify target and town selection under fog. | L | Task [15](../tasks/queue/15-sight-fog-rules.md); renderer light masks already exist but do not establish AI sight. V02/V04. |
| V07 | Complete required damage classes, regeneration/combat state interactions, range geometry, firing/reload scheduling, composite weapons and projectile impact timing. Resolve trajectory, interception, burn, splash/friendly damage, death and corpse semantics with focused scenarios. | XL | Tasks [02](../tasks/queue/02-descriptor-combat.md), [18](../tasks/queue/18-projectile-collision.md); projectile replay exists, unresolved descriptor weapons are disabled. V02/V04-V06. |
| V08 | Validate economy clock phase, activation/suspension, construction placement/completion and training/spawn timing. Replace side-wide queues and provisional refunds with measured town-local production behavior. | L | Task [03](../tasks/queue/03-economy-production.md); resource-map income, price formulas and prerequisites already have synthetic coverage. V02/V04. |
| V09 | Complete repair priority/allocation and destruction service using harvested credits; finish hall upgrades and non-map farms/mines, costs, timers, build-tree changes and command/UI integration. Verify save/replay through completion. | L | Tasks [05](../tasks/queue/05-town-commands.md), [17](../tasks/queue/17-town-upgrades-structures.md); garrison extraction/list toggles exist, service and upgrades remain incomplete. V08. |
| V10 | Implement campaign-required attack modes, group/follow/selection command semantics and evidenced population/difficulty rules. Remove invented limits such as AI cap 80; do not infer a universal cap from queue limits. | L | Task [16](../tasks/queue/16-population-difficulty.md); partial selection/follow integration and queue-limit correction below. V02/V04/V08 remain incomplete. |
| V11 | Implement deterministic town planning, strategist speed selection, defence reactions and autonomous attack waves. Exercise economy/build orders and sustained opposition in mission scenarios and a repeated 20-minute run. | XL | Tasks [06](../tasks/queue/06-ai-mission-behavior.md), [13](../tasks/queue/13-ai-from-rules.md); current AI is a purchase skeleton. V05-V10. |
| V12 | Audit every campaign script condition/action and implement required boat/Johnka/submarine, build/move and camera commands. Verify trigger enablement/order, timers, paths, alliances, gifts, defence identity and victory/defeat against mission objectives. Require zero unsupported required actions. | XL | `script_unit` now covers trigger order/timers, all condition kinds, paths, gifts, identities and replay. [Campaign audit](campaign_scripts.md): all 15 scripts parse; 19 transport action sites remain unsupported. Camera semantics and objective/reference validation remain open; V02/V05/V10/V11 adapters are incomplete. |

V08/V09 implementation update (2026-10-28):

- [x] Independent town production progression and selected-town production UI,
  with synthetic simultaneous training and cancellation checks.
- [x] Harvested-credit repair/destruction allocation, priority boundaries and
  save/replay through service completion, verified by synthetic checks against
  inspected allocation facts. Existing hall upgrades and build-tree checks pass.
- [ ] V08 acceptance: measured clock phase, activation/suspension, placement,
  training/spawn ticks, reference queue semantics and refunds. V02 adapters and
  V04 HP parity remain dependencies; namespace loopback setup denied the private
  reference attempt before launch.
- [ ] V09 acceptance: reference-aligned service timing, complete protection/HP
  transitions, hall upgrade/non-map placement and completion comparisons through
  saves. V08 remains incomplete. See the [evidence inventory](engine_rules_status.md#v08v09-economy-and-town-service-update-2026-10-28-partial).
| V04 | Marine d1/d4 separation, selected other descriptor HP, arithmetic/regeneration checks and town/defence provenance; `engine_unit` / `script_unit` (#19/#25/#51). | Audit all campaign object ownership/town/tactic initialization and remove mission-maximum HP fallbacks (L); establish regeneration state/overshoot, missing weapon/armor fields and Hypno behavior (L). | Task 02 partial; queued [31](../tasks/queue/31-initial-state-parity.md); V02 identity observers for comparison. |
| V05 | Descriptor ground classes/speeds, fixed-point air orders and replay; `engine_unit`, `path`, `path_bench_smoke`, `zpath_tools` (#50). | Establish boat/Johnka routes and Bird speed (L); implement measured radii/footprints/reservations/vehicle turns (L); compare WaveSearch, crowded/narrow/unreachable paths (L). | Tasks 23 and 01/02 partial; V02/V04. [Movement progress](rules_movement.md#8-v05-partial-integration-2026-10-28). |
| V06 | Resource-loaded per-type masks, daylight/glow, strict brightness >32, exploration and eligibility gate; `engine_unit` sight checks (#43). | Integrate raw-brightness/border conventions with renderer/fog/selection consumers and resource loading paths (M); replace nearest/lower-ID/four-tick acquisition with measured tactic bounds, scans and assignment (L); compare day/night target/town choices (M). | Task 15 partial; V02/V04/V10. Missing-resource six-tile fallback remains P. [Sight notes](sight_fog.md). |
| V07 | Selected descriptor damage/reload, projectile state/impact/replay and effect metadata; `engine_unit` (#19/#18). #35 added measurement docs only. | Establish missing weapon/category mappings and range/regeneration interactions (L); measure/implement composite scheduling and launch/impact timing (L); measure trajectory/interception/burn/per-victim splash and death/corpse behavior (XL). | Tasks 02/18 partial; V02/V04-V06. Known unresolved descriptor weapons remain disabled. |
| V08 | Resource-clock formulas/prices/prerequisites and per-town queue progression/cancellation UI; `engine_unit` production suites (#20/#57). | Measure clock/activation/suspension and construction placement/completion phases (M); align training/spawn ticks and town-local pause/capacity/refunds (L). | Tasks 03/24 partial; V02 observers and V04 HP parity incomplete. |
| V09 | Garrison extraction, repair lists/modes, harvested-credit priority allocation/destruction, hall upgrades and flag-0 structures; `engine_unit` town/service/save-replay cases (#25/#36/#57). | Compare service timing/protection/HP transitions (M); align hall upgrade and non-map placement/cost/timer/build-tree completion through saves (L); finish command/UI acceptance (M). | Tasks 05/17/24 partial; V04/V08, then V14. |
| V10 | Four numeric modes, owned-tactic propagation/direct engagement, no AI cap 80, production above 110 and map Difficulty neutrality; `engine_unit` / `ai_unit` (#44/#32). | Finish group/follow/selection packet semantics and attached-tactic graph/distance limits (L); establish population/global difficulty effects without inventing bonuses or caps (M). | Task 16 partial; queued [32](../tasks/queue/32-command-semantics.md); V02/V04/V08. |
| V11 | Pure town planner, speed/economy decisions, defence/waves; `ai_unit` and paired 14,400-tick `ai_skirmish` with replay/activity/casualty assertions (#32). | Establish composition/placement/launch scoring, instantaneous income estimates and shared side-plan arbitration; add autonomous hall-upgrade selection (L); validate sustained mission opposition and defence with normalized observations (L). | Tasks 06/13 partial; queued [33](../tasks/queue/33-ai-campaign-behaviour.md); V05-V10; retain the existing planner and skirmish assertions. |
| V12 | All 15 scripts parse; every condition kind, trigger order/timers, paths, gifts, defence identity and replay have `script_unit` / `script_audit` controls (#51). | Implement measured transport routes/boarding/activation for 19 unsupported Boat/Submarine/Johnka sites in missions 2/7/9/15 (XL); establish camera activation argument/locking and reference trigger/timer semantics (M); validate objective chains, alliances/gifts and outcomes per mission (L). | Task 25 partial; V02/V05/V10/V11. Build/move/deactivate/Johnka-disable grammar branches have no sites in this campaign; promote if required. [Campaign audit](campaign_scripts.md). |

- [x] Implement independent town production, selected-town menus/cancellation,
  harvested-credit service allocation and save/replay through service/upgrade
  completion (#57); tests cover simultaneous towns and priority boundaries.
- [x] Implement deterministic town planning and a repeated 20-minute synthetic
  skirmish (#32); purchase composition, placement and threat/launch policies remain P.
- [x] Inventory all campaign script sites and test implemented script contracts
  (#51); parsing and zero unsupported sites are different acceptance conditions.
- [ ] Close V08/V09 reference timing, activation, queue/refund, placement and HP
  transitions. Normalized V02 adapters and V04 initialization remain incomplete;
  earlier isolated attempts failed before reference launch. See the
  [service evidence](engine_rules_status.md#v08v09-economy-and-town-service-update-2026-10-28-partial).

V10 implementation update (2026-10-28):

- [x] Implement deterministic control-group recall, side-local tactic selection,
  persistent ground/air follow orders, stance interruption/resumption, ownership
  and cycle checks, and synthetic replay/save continuation across viewing sides.
  Engine payload and centroid conventions remain distinct from reference facts.
- [x] Confirm AI cap 80 is absent and remove the invented eight-entry production
  cutoff in favor of the inspected town queue capacity 50. Retain synthetic
  population-above-110 and parsed Difficulty 0/100 engine checks.
- [ ] Complete V10 acceptance: campaign controls, reference formation/attachment
  and selection modifiers, mode-specific group distances and inheritance, live
  population/difficulty observations. V02/V04/V08 remain prerequisites; the private
  isolated reference attempt failed before launch at loopback setup. See the
  [V10 evidence inventory](engine_rules_status.md#v10-command-integration-2026-10-28-partial).

### Make the campaign usable and resumable

| ID | Tested foundation | Concrete remaining work and size | Dependencies |
| --- | --- | --- | --- |
| V13 | Kind-specific render/animation selection, independent aim/body angles, finite Boat completion, pause clocks and Defence construction overlay; `engine_unit`, `render_contracts`, `animation_headless`, `zspr_tools` (#21/#31/#52). | Supply transport initiation, aircraft altitude, turn/steering/hop and destruction inputs from established simulation behavior (L); validate synchronized attack/death/overlap/night scenes and readable fog/minimap (M). | Tasks 04/12/26 partial; V05-V07/V12 and V06 consumers. Keep exact and tolerant pixel metrics separate. [Producer status](engine_rules_status.md#v13-simulation-animation-producers-2026-10-28-partial). |
| V14 | Anchored panels, scaling/wrapping, minimap/portrait geometry, Menu hit regions, paging/hotkeys and eight synthetic screenshot cases; `ui_screenshots` / `engine_unit` (#48). | Finish available repair/upgrade/mode/group controls, icons/tooltips/messages and sizes against reference (M); exercise briefings/objectives/outcomes/restart/profile/next-mission/final-ending flow for every mission (L). | Task 14 partial; queued [34](../tasks/queue/34-campaign-ui-audio.md); V09/V10/V12/V13. Synthetic layout checks are not live campaign or reference acceptance. |
| V15 | Tick-driven body/turret/effect frame cues, positional loops/visibility cleanup, music order and numeric event logs; `audio_events` (#39). | Map/dispatch selection/order speech, subtitles, warnings and building/UI announcements; add volume/acknowledgement policies (M); compare mission playback/attenuation and prove audio settings preserve simulation hashes (M). | Task 20 partial; queued 34; V03/V12/V14. [Audio inventory](audio.md) supersedes older missing-loop/music-order status rows. |
| V16 | AZS3 bounds/version/content checks, atomic replacement, replay-mismatch refusal and fresh-process 14,400-tick saves plus 300 ticks; `engine_save` / `engine_save_processes` (#53). | Extend compatibility/hash/continuation checks as producers land (M); verify fresh-session campaign saves on Linux/Windows with camera, frontend and progression through mission finish (L). | Task 27 partial; V03 and V04-V15 state. UI remains one mission-local slot. [Save boundaries](engine_saves.md). |

- [x] Implement animation producer corrections and synthetic angle/phase/pause/
  construction checks (#52); altitude and campaign transport initiation remain open.
- [x] Implement panel geometry/readability controls and synthetic screenshot
  regressions (#48); no original screenshot fixtures or new reference score.
- [x] Implement scene audio events, loops and mission playlist tests (#39);
  selection/order speech and subtitles are still missing.
- [x] Implement save hardening and synthetic fresh-process continuation (#53);
  every-mission save/resume and native campaign continuation remain unverified.

### Prove and ship v1

| ID | Tested foundation | Concrete remaining work and size | Dependencies |
| --- | --- | --- | --- |
| V17 | MSVC/MinGW/AppleClang build/test matrix, explicit byte order/alignment and case-insensitive resource lookup; `portability`. CMake isolates UDP to UNIX (#30). | Verify green native Linux/Windows jobs and cross-platform traces, fix actual failures (M); verify branch protection requires relevant build/test/lint jobs (S). | Task 09 implementation landed; V01 remote access. Windows UDP is deferred and no longer blocks the default independent build. |
| V18 | Package/source/license/dependency staging, bundled rules lookup and clean-runner smoke workflow; `tools/package_release.py` / `tests/test_package.py`, recorded local relocated synthetic staging (#54). | Obtain Linux Debian-bundled and static Windows artifacts plus extracted-package smoke results on fresh runners (M); verify external-data setup/errors on clean Linux/Windows hosts before publication (M). | Task 28 partial; V17. Local host-library staging does not verify redistributable bundling. [Package evidence](#v18-package-evidence). |
| V19 | Paired campaign runner/headless mode with outcomes, unsupported/dormant action diagnostics, narrow objective/stalled-order findings and per-tick digests; `playthrough` (#38). | Extend the existing harness with an explicit 15-mission matrix and mid-mission save comparison (M); supply successful player-controllable routes and expected win/lose/retry/progression results (XL); complete interactive Linux/Windows acceptance (L). | Task 19 partial; queued [35](../tasks/queue/35-campaign-matrix.md). Matrix scaffolding can start now; successful routes depend on V03-V16. [Harness method](playthrough.md). |
| V20 | Malformed-input corpus and loader fixes, fail-fast ASan/UBSan option/job, recorded local Clang sanitizer subset; `fuzz` and composed loader tests (#55). | Obtain hosted sanitizer/leak results and fix findings (M); fix campaign/soak failures with private-data/socket-capable checks (L); publish platform/mission/save-load release evidence and limitations (S). | Task 29 covered sanitizer/malformed-input subset only; V17-V19 for release closure. [Robustness limits](../tests/ROBUSTNESS.md). |

- [x] Add portable code/tests and native CI definitions (#30); a local Linux
  pass does not establish native Windows results or enforced required checks.
- [x] Expand synthetic malformed-input coverage and fix covered descriptor
  overflow/validation gaps; recorded GCC Release and local Clang ASan/UBSan pass (#55).
- [x] Add fail-fast sanitizer configuration and dedicated Linux synthetic CI job (#55).
- [ ] Obtain hosted sanitizer results with leak detection. Earlier local leak
  inspection was denied; this refresh ran Release checks only.
- [ ] Complete campaign/soak fixes, required private-data/socket checks and the
  platform/mission/save-load release checklist. Skips never satisfy release gates.

### V18 package evidence

- [x] Implement Linux dependency/source/notice bundling, static Windows packaging,
  packaged rules lookup, external-data instructions and gated publication with
  engine source and checksums (#54). ffmpeg stays optional and external.
- [x] Record local relocated staging checks from an unrelated directory with
  synthetic victory/repeatability, dummy SDL, missing/corrupt archives/terrain
  and missing rules; these use `tests/test_package.py`, no original data or oracle.
- [ ] Obtain green Linux and Windows release builds and extracted-package smoke
  runs on fresh CI runners. Recorded Fedora local staging uses host libraries
  and does not verify Debian bundling or native Windows.
- [ ] Verify V17 native checks and branch protection against actual CI results.

## Next concrete tasks in merge order

This is a dependency order, not permission to rerun already landed prompts in
full. Missing prerequisites below are behavioral/evidence gaps, not assumed
unmerged code. Preserve the existing tested foundations when splitting work.

1. **V01 remote evidence (S), V17 native gates (M/S):** refresh merged PRs and
   run/check native CI, transcript comparison and branch protection. In parallel,
   extend V19's existing harness with matrix/save scaffolding (M, prompt 35);
   successful mission completion must remain unclaimed.
2. **V02 observer/replay adapters (L/L):** finish the remaining task 01/11/22
   work, establish isolated repeatability and the first normalized field report.
   Synthetic adapter controls can precede a host permitting live reference runs.
3. **V04 initialization (L/L, prompt 31):** establish object identity, ownership,
   HP and regeneration before interpreting movement/combat differences. Keep
   unmeasured weapon/HP states explicit rather than adding fallback certainty.
4. **V10 commands (L/M, prompt 32), V05 movement and V06 acquisition (L/M):**
   merge command/identity contracts before dependent tactic/transport adapters;
   establish water routes, geometry and visibility consumers before their users.
5. **V07 weapons and V08/V09 economy/service (separate M/L changes):** implement
   measured mechanics in isolated scenarios, then their save/hash regressions.
   Economy work independent of unresolved weapons can proceed after V04.
6. **V12 transport/scripts (XL split by Boat, Johnka and submarine), then V11
   mission AI (L/L, prompt 33):** build on movement/commands/production. Close
   the 19 required transport sites, replace AI fallback policies, then validate
   script objective/tactic ownership with the integrated AI. Retain the existing
   skirmish assertions.
7. **V13 producers/visibility (L/M), then V14/V15 campaign UI/audio (prompt 34,
   M/L pieces):** expose completed commands/transport/state, verify readable
   scenes and complete briefings/outcomes/progression/speech feedback.
8. **V03/V16 final state/save audit (M/L), then V19 routes/platform matrix
   (prompt 35, XL/L):** replay and resume each mission in fresh processes, cover
   defeat/retry and final progression. Extend hash/save tests after each producer
   merge as well; do not defer regression coverage to this final pass.
9. **V18 clean packages (M/M), then V20 release closure (L/S):** use green V17
   jobs, complete sanitizer/leak and campaign soak checks, validate final artifacts
   and publish the release evidence only after all five acceptance gates pass.

Packaging smoke runs and hosted sanitizer checks can proceed alongside earlier
simulation work; the final release still depends on campaign/platform evidence.
Do not launch prompts 22-29 as untouched tasks: #49-#55/#57 have already landed
partial implementations. Narrow their follow-ups to the remaining pieces above.

## Deferred work and fidelity follow-ups

These gaps are outside the v1 definition. Promote any item that blocks campaign
objectives, readability, determinism, safe loading or supported builds into the
required checklist with a reproducer.

| Task | Size | Current boundary |
| --- | --- | --- |
| Complete original `.sav` resume, including orders, research and script state, and define interoperability/export policy. | XL | Task 07 / #28; parser and partial importer with `save_import` checks exist. v1 requires engine saves. |
| Integrate multiplayer lobby/session/content/disconnect handling and Windows transport; investigate original network compatibility separately. | XL | Task 08 / #29; simulated two-peer tests and POSIX UDP exist, frontend integration absent. Restricted UDP skips are not transport validation. |
| Complete exact alpha/shadow blending, per-pixel lighting, wind/overlay producers, effect owners 104/153 and cosmetic animation/audio parity. | XL | Unknowns remain explicit; tolerant comparisons do not close exact-pixel work. Campaign-visible blockers belong in V13-V15. |
| Promote native macOS from configured CI coverage to release acceptance and packaging. | M | #30 already adds AppleClang builds/tests; Linux and Windows are the v1 platforms. |
| Extend editor/custom-map/skirmish coverage and tools beyond campaign needs. | L | Task 10 / #26 provides map inspection/dumps, not a playable editor or complete skirmish behavior. |

Reference experiments must follow [EXPERIMENTS.md](EXPERIMENTS.md): private
writable game copies, read-only resource symlinks, both SDL dummy drivers,
`unshare -rn` and hard timeouts. Earlier namespace/socket setup failures remain
failures; rerun on a suitable host. Keep assets, executable-derived code,
recordings, screenshots and state dumps in ignored private output. Public
progress contains independent descriptions, coverage and test outcomes only.

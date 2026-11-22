# Documentation index

For build instructions, tests, tools, and contribution rules, see
[CONTRIBUTING.md](../CONTRIBUTING.md). The [project overview](../README.md) and
[engine guide](../src/engine/README.md) describe the runnable engine.

Some existing research notes are in Polish. New contributions must be in English.
Research notes describe work in progress; consult their verification markers
before relying on a finding.

Original game files and executable-derived code must stay outside contributions.
Tool-generated images and other game assets must also remain private.

## Rules and mechanics

- [rules_summary.md](rules_summary.md) - Gameplay rules for the rewrite, with simulation constants and rule references.
- [rules_types.md](rules_types.md) - Gameplay rule data and evidence, with type parameters and partial rule reconstruction.
- [engine_rules_status.md](engine_rules_status.md) - Status of the implemented simulation rules and their evidence levels.
- [determinism.md](determinism.md) - Simulation determinism audit of the independent engine.
- [game_modifications.md](game_modifications.md) - Play profiles and game modifications, including the helper presets.
- [modifications.md](modifications.md) - Clone-only simulation options; without an enabled option the engine uses original-style rules.
- [104-profiles-faithful-and-enhanced.md](104-profiles-faithful-and-enhanced.md) - Task 104: faithful and enhanced play profiles.
- [combat-parity-second-pass.md](combat-parity-second-pass.md) - Combat parity second pass (task 117): isolated formula and Marine sequencing differences.
- [fire_air_siege.md](fire_air_siege.md) - Fire, air and siege investigation.
- [ground_weapons.md](ground_weapons.md) - Ground weapon investigation.
- [projectiles_effects.md](projectiles_effects.md) - Projectiles and weapon hit effects (gap closure).
- [weapon_class_map.md](weapon_class_map.md) - Weapon class map of the original, from a decompilation survey.
- [towers_air_specials.md](towers_air_specials.md) - Towers, air units and open special abilities, from a decompilation read.
- [special_abilities.md](special_abilities.md) - Static inventory of special abilities and unit mechanics (partial implementation).
- [special_mechanics_residual.md](special_mechanics_residual.md) - Residual special mechanics: capture, Johnka, healing, latches, effects and Hypno.
- [specials_commands.md](specials_commands.md) - Class 13 hit effect, Mage/Chief/Abdula actives and invisibility.
- [hypno_and_ownership.md](hypno_and_ownership.md) - Hypno family, special objects and ownership.
- [hypno_recon.md](hypno_recon.md) - Hypno reconnaissance from headless reference runs in mission 7.
- [ownership_mechanism.md](ownership_mechanism.md) - Ownership changes in the original: mechanism and town capture.
- [ownership_observations.md](ownership_observations.md) - Ownership changes of buildings in the original (outpost capture, mission 7).
- [tick_order_and_rand.md](tick_order_and_rand.md) - Tick order, pool order and rand() in Aztec.exe.
- [sight_fog.md](sight_fog.md) - Sight and fog validation of the reimplementation.
- [unit_state_machines.md](unit_state_machines.md) - Per-class unit think state machines.
- [leftovers_scripts_descriptors.md](leftovers_scripts_descriptors.md) - Leftovers: script actions, condition timing, descriptor words and ferry rate.

## AI

- [rules_ai.md](rules_ai.md) - Computer-player rules for a clean implementation.
- [ai_army_control.md](ai_army_control.md) - AI army control and tactics (task 119).
- [ai_assign_structure.md](ai_assign_structure.md) - AI combat assignment functions, from a structure survey (not a full formula read).
- [ai_phases.md](ai_phases.md) - Strategist phase handlers (table 0x4ae9e8).
- [ai_purchase_gate.md](ai_purchase_gate.md) - AI purchase gate investigation (2026-11-07, incomplete).
- [ai_purchase_plan.md](ai_purchase_plan.md) - AI purchase plans: source investigation and replacement.
- [ai_scoring_threat.md](ai_scoring_threat.md) - Computer player target score, plan builder, threat trigger and side plan.
- [ai_tables_and_light.md](ai_tables_and_light.md) - AI tables, kill time and the light map (numeric leftovers).
- [idle_autoengage.md](idle_autoengage.md) - Idle unit auto-engage in the original, from decompilation.
- [unit_automatic_behaviour.md](unit_automatic_behaviour.md) - Unit automatic behaviour (task 127).

## Economy

- [economy_rules.md](economy_rules.md) - Economy, construction and production rules of the original.
- [economy_residual.md](economy_residual.md) - Economy residual unknowns from a static pass.
- [118-economy-construction-harvest-audit.md](118-economy-construction-harvest-audit.md) - Economy, construction and harvest audit (task 118).
- [103-mine-audit.md](103-mine-audit.md) - Mine audit (task 103).
- [early_payment.md](early_payment.md) - Early computer spending (task 96), partial.
- [parallel_training.md](parallel_training.md) - Task 100: parallel training.
- [placement_rules.md](placement_rules.md) - Building placement and town build radius.
- [101-faithful-placement-and-relaxed-helper.md](101-faithful-placement-and-relaxed-helper.md) - Placement policy and optional relaxed placement (task 101).

## Movement and pathfinding

- [rules_movement.md](rules_movement.md) - Movement observations, experiments, and replacement design.
- [movement_residual.md](movement_residual.md) - Movement and unit behaviour: residual unknowns.
- [pathfinding_wave.md](pathfinding_wave.md) - Path finding ("wave") and unit movement of the original.
- [pathing-regression-from-playthroughs.md](pathing-regression-from-playthroughs.md) - Pathing regression from playthroughs (task 128).
- [group_formation_at_start.md](group_formation_at_start.md) - Group formation at mission start (task 98).
- [group_formation_rule.md](group_formation_rule.md) - Mission startup group formation (implemented guess).
- [group_formation_leash.md](group_formation_leash.md) - Group movement, formation and leash, from the base tactic.
- [group_reach.md](group_reach.md) - Group reach, idle acquisition and pursuit.
- [idle_tactic_walk.md](idle_tactic_walk.md) - Idle tactic walk investigation (task 95).
- [unit_unstick.md](unit_unstick.md) - Unit unstick helper (task 102).

## Transport and ferry

- [transport_rules.md](transport_rules.md) - Ferries and transports.
- [transport_boats_full.md](transport_boats_full.md) - Transport: boats, ferry tactic and submarine (full reading).
- [transport_live_observations.md](transport_live_observations.md) - Live observations of ferry and squad boarding in mission 2.
- [ferry_control_flow.md](ferry_control_flow.md) - Ferry control flow in the original, from decompilation.
- [ferry_eligibility.md](ferry_eligibility.md) - What makes a ferry accept a group (decompilation trace).
- [ferry_shuttle.md](ferry_shuttle.md) - Ferry shuttle controller (task 129).
- [ferry_timeline.md](ferry_timeline.md) - Ferry timeline measured in the original (headless, mission 2).

## Rendering, audio and interface

- [render_integration.md](render_integration.md) - Sprite and lighting integration inventory.
- [rendering_animation_second_pass.md](rendering_animation_second_pass.md) - Rendering and animation second pass.
- [sprites_mapping.md](sprites_mapping.md) - Mapping between types, animation slots, and sprites.
- [animation_timing.md](animation_timing.md) - Attack and walk animation timing from a static dump.
- [audio.md](audio.md) - Engine audio and parity evidence.
- [audio-parity.md](audio-parity.md) - Audio events, voices and controls (task 122).
- [audio_events.md](audio_events.md) - Audio events of the original (gap analysis).
- [audio_saves_residual.md](audio_saves_residual.md) - Audio and save residuals of the original Aztec.exe.
- [fog_sight_minimap.md](fog_sight_minimap.md) - Fog of war, sight and minimap of the original (decompilation only).
- [fog_visibility_minimap.md](fog_visibility_minimap.md) - Fog, visibility and minimap audit.
- [frontend_screens.md](frontend_screens.md) - Front-end screens (partial reconstruction).
- [frontend_parity_results.md](frontend_parity_results.md) - Front-end capture evidence.
- [ingame_ux_results.md](ingame_ux_results.md) - In-game UX experiment status.
- [interface_geometry.md](interface_geometry.md) - Interface geometry: side panel, minimap, tooltips, messages and menus.
- [interface_ingame.md](interface_ingame.md) - Interface: menu states, briefing flow, in-game controls and screen layout.
- [ui_screens.md](ui_screens.md) - UI capture inventory and measurement status.
- [worldview_mouse_cursors.md](worldview_mouse_cursors.md) - World view: mouse commands, selection, cursors and placement preview.
- [121-input-hotkeys-commands-audit.md](121-input-hotkeys-commands-audit.md) - Player commands and input audit.
- [124-options-difficulty-speed-wiring.md](124-options-difficulty-speed-wiring.md) - Options, speed and difficulty wiring (task 124).

## Campaign, saves and multiplayer

- [campaign_loader_and_flow.md](campaign_loader_and_flow.md) - Campaign missions: launcher, loader chain, per-mission structure and end flow.
- [campaign_menus_saves.md](campaign_menus_saves.md) - Campaign flow, player profiles, saves and front-end states.
- [campaign_scripts.md](campaign_scripts.md) - Campaign script audit (V12).
- [campaign_sweep.md](campaign_sweep.md) - Campaign sweep: inspected original instructions and private mission data.
- [custom_mission_skirmish.md](custom_mission_skirmish.md) - Custom mission (skirmish) mode (task 125).
- [mission2_blockers.md](mission2_blockers.md) - Mission 2 blockers.
- [mission_script_presentation.md](mission_script_presentation.md) - Mission script presentation (task 126).
- [script_conditions_flow.md](script_conditions_flow.md) - Script conditions and flow (trigger system).
- [script_codes_and_ai_purchase.md](script_codes_and_ai_purchase.md) - Script queue codes 0x0e, 0x1a, 0x1e and the AI purchase gate.
- [script_gift_command.md](script_gift_command.md) - Script command queue and the gift to side command.
- [savegame.md](savegame.md) - Original save-game stream.
- [savegame_roundtrip_all_state.md](savegame_roundtrip_all_state.md) - Save/load full-state audit (task 116).
- [engine_saves.md](engine_saves.md) - Engine save format and safety.
- [multiplayer.md](multiplayer.md) - Engine lockstep protocol for two-peer play (aztec_net library).

## Formats, objects and data

- [formats.md](formats.md) - Resource archives, maps, scripts, and other data files.
- [classes.md](classes.md) - Class relationships and observed object layouts.
- [object_types.md](object_types.md) - UGI type identifiers and categories.
- [object_fields.md](object_fields.md) - Serialized object members of the original save format.
- [type_stats.md](type_stats.md) - Observed unit and structure statistics.
- [descriptor_fields.md](descriptor_fields.md) - Object descriptor fields.
- [descriptor_marine.md](descriptor_marine.md) - Marine descriptor words.
- [descriptor_building_defence.md](descriptor_building_defence.md) - Building and Defence descriptor words.
- [descriptor_other_kinds.md](descriptor_other_kinds.md) - Descriptor words of Bird, Zeppelin, Truck, Cannon, Headtruck and Spider.
- [descriptor_config_cells.md](descriptor_config_cells.md) - Descriptor config cells: resolution.
- [descriptor_provenance.md](descriptor_provenance.md) - Descriptor CSV provenance (desc_*.csv).
- [data_tables_audit.md](data_tables_audit.md) - Data table completeness audit.
- [data/](data/) - CSV and JSON reference tables consumed by the engine and research tools.

## Tooling, process and status

- [README.md](README.md) - Documentation index (this file).
- [PLAN.md](PLAN.md) - Research approach and original project plan.
- [REBUILD_CHECKLIST.md](REBUILD_CHECKLIST.md) - Progress across engine subsystems.
- [REWRITE.md](REWRITE.md) - Engine architecture and rewrite status.
- [ROADMAP.md](ROADMAP.md) - Roadmap to v1 (historical planning snapshot).
- [AUDIT_2026-11.md](AUDIT_2026-11.md) - Conflict-resolution and landed-feature audit.
- [HANDOFF.md](HANDOFF.md) - Project handoff snapshot.
- [NEXT_STEPS.md](NEXT_STEPS.md) - Next steps when a session starts with only "continue".
- [SETUP.md](SETUP.md) - Setup on a new machine.
- [INIT_PROMPT_WINDOWS.md](INIT_PROMPT_WINDOWS.md) - Windows initialization prompt for Claude Code.
- [PACKAGES.md](PACKAGES.md) - Quetzal Engine binary packages.
- [EXPERIMENTS.md](EXPERIMENTS.md) - Running experiments with the reference runtime.
- [PARITY_TEST_PLAN.md](PARITY_TEST_PLAN.md) - Parity test plan: original game versus the clone.
- [parity.md](parity.md) - Whole-mission parity reports.
- [parity_matrix.md](parity_matrix.md) - Parity matrix of endpoint counts.
- [parity_visual_ui_results.md](parity_visual_ui_results.md) - L4/L5 aggregate evidence.
- [gap_recon.md](gap_recon.md) - Gap reconnaissance (2026-11-08).
- [playthrough.md](playthrough.md) - Automated campaign playthrough tool.
- [playthrough_framework.md](playthrough_framework.md) - Player-command playthrough framework.
- [playthrough_full_campaign.md](playthrough_full_campaign.md) - Full campaign player-command playthrough (task 115, incomplete).
- [playthrough_missions_1_5.md](playthrough_missions_1_5.md) - Player-command playthroughs of missions 1-5.
- [playthrough_missions_6_10.md](playthrough_missions_6_10.md) - Player-command playthroughs of missions 6-10.
- [playthrough_missions_11_15.md](playthrough_missions_11_15.md) - Player-command investigations of missions 11-15.

## Newer documents

- [economy_implementation.md](economy_implementation.md) - Economy implementation
- [lockstep_packets.md](lockstep_packets.md) - Command packets, lockstep and network layer
- [pathfinding_implementation.md](pathfinding_implementation.md) - Original wave search: implementation and integration limits
- [renderer_rules.md](renderer_rules.md) - World renderer rules (0x465a50 and callees)
- [unit_behaviour_implementation.md](unit_behaviour_implementation.md) - Unit scans, chase and group walking
- [measured_infantry_speed.md](measured_infantry_speed.md) - Measured walking speed of Rifleman, Footman, Archer and Bear (headless oracle).

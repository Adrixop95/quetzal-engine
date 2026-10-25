# Documentation index

For build instructions, tests, tools, and contribution rules, see
[CONTRIBUTING.md](../CONTRIBUTING.md). The [project overview](../README.md) and
[engine guide](../src/engine/README.md) describe the runnable engine.

Some existing research notes are in Polish. New contributions must be in English.
Research notes describe work in progress; consult their verification markers
before relying on a finding.

## Plans and implementation status

- [Reconstruction plan](PLAN.md): research approach and original project plan.
- [Rebuild checklist](REBUILD_CHECKLIST.md): progress across engine subsystems.
- [C++20 rewrite plan](REWRITE.md): engine architecture and rewrite status.
- [Engine rules status](engine_rules_status.md): implemented simulation rules and evidence levels.

## Formats, objects, and presentation

- [File formats](formats.md): resource archives, maps, scripts, and other data files.
- [Object classes](classes.md): class relationships and observed object layouts.
- [Object types](object_types.md): UGI type identifiers and categories.
- [Type statistics](type_stats.md): observed unit and structure statistics.
- [Sprites and animations](sprites_mapping.md): mapping between types, animation slots, and sprites.

## Simulation and reference data

- [Rules summary](rules_summary.md): simulation constants and rule references.
- [Type rules](rules_types.md): type parameters and partial rule reconstruction.
- [Movement and pathfinding](rules_movement.md): movement observations, experiments, and replacement design.
- [Data tables](data/): CSV and JSON reference tables consumed by the engine and research tools.

Original game files and executable-derived code must stay outside contributions.
Tool-generated images and other game assets must also remain private.

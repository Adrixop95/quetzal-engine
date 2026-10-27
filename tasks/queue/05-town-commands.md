Depends: 01-oracle-parity,03-economy-production
# Implement verified town commands and upgrades

Read CLAUDE.md, docs/HANDOFF.md and docs/engine_rules_status.md first. This is a
PUBLIC repository: write only independent code and English text; never add game
assets, the executable, generated C or decompiled source. Keep private evidence
under ignored work/. Use a build directory inside this worktree. Verify the
baseline build and CTest before editing; repeat both and run pre-commit afterward.
Respect read-only git metadata when applicable and leave changes uncommitted.
Finish with changed files, checks, evidence labels and unresolved questions.

Purpose: replace logged-only or placeholder town behavior with documented semantics.
Sources: docs/rules_summary.md, docs/rules_types.md, docs/object_types.md,
docs/engine_rules_status.md, engine commands/GameSim/savegame.
Scope: garrison, repair/destroy modes, town upgrades and their deterministic state.

Requirements:
- Implement garrison as extraction of live mobile town members into a selected
  tactic; do not invent boarding slots or building capacity from the command name.
- Implement marked repair/destroy lists, mode transitions, per-tick repair credits
  and damage-priority grouping; destroy toggles membership instead of instant deletion.
- Apply established hall upgrade/build-tree changes and prerequisite locality;
  coordinate shared production changes with task 03.
- Include all new command/state data in hashing and replay. Add synthetic invalid
  target, repeated toggle, multiple-town, upgrade and save/replay scenarios.

Acceptance: command cases and full build/CTest/pre-commit pass; update implementation
status separately from reference facts. Compare isolated reference commands with
task 01 tooling if available, documenting any blocked observation.
Dependency: complete task 03 prerequisite/production integration first; review task 01 comparison schema.

# Implement resource and town production rules

Read CLAUDE.md, docs/HANDOFF.md and docs/engine_rules_status.md first. This is a
PUBLIC repository: write only independent code and English text; never add game
assets, the executable, generated C or decompiled source. Keep private evidence
under ignored work/. Use a build directory inside this worktree. Verify the
baseline build and CTest before editing; repeat both and run pre-commit afterward.
Respect read-only git metadata when applicable and leave changes uncommitted.
Finish with changed files, checks, evidence labels and unresolved questions.

Purpose: remove fixed mine/farm yields and invented production timing.
Sources: docs/rules_types.md, docs/rules_summary.md, docs/engine_rules_status.md,
src/core/mapfiles.*, engine rules/GameSim and docs/data tables.
Scope: resource-map income, day clock, pricing/time and build prerequisites.

Requirements:
- Use occupied-cell resource weights and descriptor factors; implement periodic
  and scheduled events against StartTime and the 7200-tick day, including wraparound.
- Apply the completed same-town warehouse multiplier to farms once; keep repair
  credits separate from gold. Do not infer resource-channel names without evidence.
- Implement established hall/speed/computer price/time branches and original
  rounding (160 versus lifted 161). Enforce town-local versus side-wide prerequisites.
- Cover schedules at start/wrap boundaries, warehouse duplication, multiple towns,
  insufficient funds and command-log replay with synthetic inputs; revise status docs.

Acceptance: full build/CTest/pre-commit pass; focused scenarios match documented
formulas. If task 01 tooling/private data is available, compare one mine/farm and
training scenario and report evidence separately from unit tests.
Dependency: task 01 for differential checks; avoid overlapping GameSim edits with task 02.

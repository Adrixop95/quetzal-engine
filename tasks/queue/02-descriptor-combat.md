# Replace placeholder health and weapon rules

Read CLAUDE.md, docs/HANDOFF.md and docs/engine_rules_status.md first. This is a
PUBLIC repository: write only independent code and English text; never add game
assets, the executable, generated C or decompiled source. Keep private evidence
under ignored work/. Use a build directory inside this worktree. Verify the
baseline build and CTest before editing; repeat both and run pre-commit afterward.
Respect read-only git metadata when applicable and leave changes uncommitted.
Finish with changed files, checks, evidence labels and unresolved questions.

Purpose: implement reference facts already documented, preserving deterministic replay.
Sources: docs/rules_summary.md, docs/rules_types.md, docs/engine_rules_status.md,
docs/data descriptor/weapon CSVs and src/engine/sim/rules.* / game_sim.*.
Scope: descriptor loading, health/regeneration and weapon calculations; no UI redesign.

Requirements:
- Read Marine primary/max HP from d1 and keep d4 secondary HP distinct. Implement
  the documented regeneration threshold behavior without extrapolating to other kinds.
- Replace generic damage/range/reload defaults where descriptors are established;
  preserve sequential fixed-point category multipliers and reload+1 semantics.
  Distinguish projectile delay from reload and explicitly retain unresolved class 7/burn cases.
- Add synthetic cases for structure/nonorganic and airborne/organic combinations,
  boundary rounding, primary/secondary HP and replay hashes. Update rule status labels.

Acceptance: focused arithmetic and replay checks plus full build/CTest/pre-commit
pass. Use task 01 comparison tooling if available for a small isolated duel; record
unavailable evidence honestly. Do not promote historical duel intervals to universal rules.
Dependency: coordinate with task 01 for reference checks; synthetic corrections can proceed independently.

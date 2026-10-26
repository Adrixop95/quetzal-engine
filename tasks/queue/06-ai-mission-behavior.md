Depends: 01-oracle-parity,03-economy-production
# Replace the AI purchase skeleton and close script gaps

Read CLAUDE.md, docs/HANDOFF.md and docs/engine_rules_status.md first. This is a
PUBLIC repository: write only independent code and English text; never add game
assets, the executable, generated C or decompiled source. Keep private evidence
under ignored work/. Use a build directory inside this worktree. Verify the
baseline build and CTest before editing; repeat both and run pre-commit afterward.
Respect read-only git metadata when applicable and leave changes uncommitted.
Finish with changed files, checks, evidence labels and unresolved questions.

Purpose: implement verified town/tactic behavior while keeping open planner questions explicit.
Sources: docs/rules_ai.md, docs/rules_summary.md, docs/engine_rules_status.md,
engine sim/ai.* and script_runner.* plus docs/formats.md script grammar.
Scope: deterministic computer town states, tactic dispatch and supported mission commands.

Requirements:
- Replace the universal 56-tick random-buy interpretation with the documented
  town state machine, speed selection and autonomous attack preparation/activation.
  Do not invent difficulty bonuses or a universal population cap.
- Audit unsupported boat/johnka/submarine and camera/build/move script actions;
  implement only behavior established by evidence. Track unresolved launch scoring,
  composition and camera presentation explicitly rather than guessing.
- Keep simulation side/tactic ownership independent of the viewing peer. Hash and
  replay new state/RNG; avoid wall-clock and unordered-container iteration effects.
- Add synthetic multi-side/script scenarios and repeated-run hashes; document
  remaining unsupported commands and evidence needed for them.

Acceptance: full build/CTest/pre-commit pass; same commands produce the same state
across view-side choices. Run private mission probes using task 01 tooling where
available and label missing runtime evidence. Update rules status and handoff.
Dependency: task 03 production rules and task 05 town commands should merge first.

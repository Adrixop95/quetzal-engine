# Establish a reproducible differential oracle

Read CLAUDE.md, docs/HANDOFF.md and docs/engine_rules_status.md first. This is a
PUBLIC repository: write only independent code and English text; never add game
assets, the executable, generated C or decompiled source. Keep private evidence
under ignored work/. Use a build directory inside this worktree. Verify the
baseline build and CTest before editing; repeat both and run pre-commit afterward.
Respect read-only git metadata when applicable and leave changes uncommitted.
Finish with changed files, checks, evidence labels and unresolved questions.

Purpose: make future parity claims reproducible rather than comparing incompatible hashes.
Sources: docs/REWRITE.md (oracle stage), docs/EXPERIMENTS.md, runtime/oracle.c,
tools/corpus.sh and engine GameSim replay/state hashing.
Scope: oracle/corpus tooling, focused synthetic checks and experiment documentation.

Requirements:
- Audit command tick/order and state fields in both implementations. Define a
  normalized observation schema for one small movement/combat scenario, avoiding
  raw pointers and unrelated host state; document exclusions.
- Add a runner/comparator with explicit private game/runtime/output paths,
  independent writable game copies, required namespace isolation and timeouts.
  Fail on setup/runtime errors; never continue after denied network setup.
- Repeat record/replay twice and locate the first differing tick/field. Investigate
  historical transient hashes and the known lifted price rounding artifact;
  distinguish original instruction evidence from runtime measurements.

Acceptance: synthetic equal/different/truncated-log controls pass; replay checks
are deterministic; run a private reference probe when permitted. If private data
or networking is unavailable, report that gap without claiming live parity. Record
exact commands and update EXPERIMENTS.md with the comparison procedure. No dependency
on another queued task; do this before broad differential engine changes.

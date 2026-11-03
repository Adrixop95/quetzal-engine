# Integrate established sprite and lighting contracts

Read CLAUDE.md, docs/HANDOFF.md and docs/engine_rules_status.md first. This is a
PUBLIC repository: write only independent code and English text; never add game
assets, the executable, generated C or decompiled source. Keep private evidence
under ignored work/. Use a build directory inside this worktree. Verify the
baseline build and CTest before editing; repeat both and run pre-commit afterward.
Respect read-only git metadata when applicable and leave changes uncommitted.
Finish with changed files, checks, evidence labels and unresolved questions.

Purpose: bring the renderer into line with established metadata and expose remaining visual uncertainty.
Sources: docs/sprites_mapping.md (R/C/T/P evidence), engine assets/data/render
modules and tools/zspr_run.sh / zspr_compare.py. docs/REBUILD_CHECKLIST.md is historical.
Scope: renderer integration, animation selection and focused screenshot tooling/tests.

Requirements:
- Audit existing integration first. Implement missing kind-specific body/secondary
  sequences, timed-prefix/hold behavior, height sources and construction/damage phases.
- Sort overlays/objects by the documented common ordering and draw each item's
  shadow/body/secondary layers in order; do not invent an air-last pass.
- Read light masks from user resources and integrate brightness/vision grid
  positioning/interpolation without embedding mask bytes. Keep unknown owners 104/153 explicit.
- Add synthetic boundary/ordering/light controls and synchronized private probes
  for non-Marine attack/death, overlapping objects and night lighting. Separate exact
  pixels from tolerant scores and account for alpha background compositing.

Acceptance: build/CTest/pre-commit pass. Update the integration/evidence inventory;
record blocked probes as pending, never P. Keep screenshots and pixel data ignored.
Dependency: none for integration; use task 01 infrastructure where it helps reference synchronization.

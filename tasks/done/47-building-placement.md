Task: building placement, build grid and construction flow.
Observed by the user playing mission 1 with the current develop build:
1. Placement does not work correctly: buildings end up where they should not, or cannot be put where they should be.
2. Mines cannot be placed at all.
3. There is no building grid drawn on the ground while placing (the original shows which tiles of the footprint are valid or blocked under the cursor).
4. The construction flow is wrong (start, progress, stages, completion, who builds, cancel and refund).
Reproduce each symptom first with a deterministic synthetic scenario or headless test, then fix the causes in the new engine (src/engine, not runtime/): placement rules (footprint, terrain, resource deposits for mines, distance to the town hall, collision with objects), the placement mode UI with the footprint grid overlay (valid and blocked tiles, following the cursor, click to confirm, right click or Escape to cancel), and the construction progress and stage animation. Use the reference runtime (docs/EXPERIMENTS.md, UI capture controls) to measure the original footprint colours, grid and rules where possible; label rows V/M/P in docs/engine_rules_status.md. Follow CLAUDE.md: public repo, English only, no game assets or executable-derived code, deterministic simulation.

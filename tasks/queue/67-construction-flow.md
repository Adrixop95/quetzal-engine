Depends: 66-building-and-mine-placement
Task: construction flow parity, a known playtest gap. In the original, placing a building first puts a placeholder on the ground, then a worker builds it through visible stages; the clone differs.
Scope: placeholder sprite and footprint at placement, worker assignment and walk-to-site, construction stages and their timing, cost deduction time, cancel and refund, interruption and resume, completion event (sound id, selection, rally), and build queue behaviour. Parallel construction (several workers on one site) stays an optional game modification, default off (see task 48); do not change its default.
Method: scripted scenarios in tests/parity/ on ztest_all that build one of each class in both programs; compare the per-tick object state (stage, hp, worker state, money) and frames of the construction overlay. Measure the original; derive timings from measurements, not guesses.
Requirements: fix src/engine construction code until stages, timings and money flow match, add synthetic regression tests without game data, update docs/ROADMAP.md (V08 construction phases) with evidence labels.
Sandbox note: live reference runs are denied inside the Codex sandbox; finish with status "needs live run" and a run sheet in docs/EXPERIMENTS.md when reference data is missing.
Public repo, English only, no game data or captures in git.

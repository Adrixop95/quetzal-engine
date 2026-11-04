# Next steps (state on 2026-11-03)

Read this when a session starts with only "continue". Verify against live state
(`gh pr list`, `git log origin/develop`, `python3 tools/codex_orch.py status`) first.

## Done

All prompts in `tasks/done/` are merged into `develop`. Parity layers L0-L8 have a
plan (`docs/PARITY_TEST_PLAN.md`), a harness and iterations for rules, visuals,
audio, movement and AI behaviour; menu flow, in-game UX and the UI capture pipeline
are merged; parallel construction is a game modification (default off).

## Open work

Prompts left in `tasks/queue/` (Codex usage limit was exhausted when they stopped, so
their earlier local sessions are not portable; run them again from the prompts):

- `56-conflict-resolution-audit` review of automatic merge-conflict resolutions.
- `57-roadmap-open-rows` close open rows of `docs/ROADMAP.md`.
- `59-ui-pixel-parity` (needs 58, merged) pixel-exact static UI.
- `62-gameplay-recording-compare` (needs 58, merged) gameplay recording comparison.
- `63-test-maps` generator for the private `ztest_all` and `ztest_battle` maps.

## Known playtest gaps

Build placement grid and mine placement, construction flow, path tracing quality,
UI click animation and highlighting. Check `docs/ROADMAP.md` for current status.

## Continue procedure

1. Run `gh pr list`; finish or merge open PRs (squash after green CI, never `--admin`).
2. Start the supervisor only if no other machine runs one:
   `CODEX_MAX_PARALLEL=3 setsid nohup python3 tools/codex_supervisor.py > work/codex/supervisor.log 2>&1 < /dev/null & disown`
3. Without a Codex limit, tasks start from `tasks/queue/` in dependency order. Merge
   conflicts and CI failures on `feature/codex-*` PRs are handled by the supervisor.
4. Report to the user in Polish, briefly.

## Plan for 2026-11-05

Priority: finish the game (every mission completable, campaign and saves complete,
combat, economy, construction, movement and AI matching the original). Quality-of-life,
infrastructure and release work lives in `tasks/backlog/` and is not started.

Done on 2026-11-04: tasks 56, 57, 62, 63 merged (#99, #100, #102, #103); task 59 (#101)
was waiting for CI. The reference runtime builds and runs headless in `unshare -rn`
outside the Codex sandbox; inside the Codex sandbox namespace creation is denied, so
live reference runs are executed by the orchestrator and prompts end with "needs live run".

Critical path to a finished game, in start order (merge the queue PR first):

1. No dependencies, start immediately: `64-shared-runner-observer` (enabler for
   measurement), `71-campaign-transport-and-objectives` (blocks missions),
   `72-original-save-resume`, `73-sight-and-fog-consumers`,
   `74-projectile-collision-and-splash`.
2. After 64: `65-l1-rule-measurements`, `66-building-and-mine-placement`,
   `68-pathing-quality`, `75-ai-composition-and-launch-scoring`,
   `76-audio-live-controls`, `77-campaign-ui-and-progression`.
3. After 66: `67-construction-flow`. After 71: `80-playthrough-matrix`
   (the acceptance test: all 15 missions reach their outcome).

Orchestrator duties: run the live reference commands from the run sheets in
`docs/EXPERIMENTS.md` outside the sandbox, feed the data back, review diffs before merge,
rerun cancelled hosted jobs, keep `CODEX_MAX_PARALLEL=5`.

Deferred to `tasks/backlog/`: 69 UI click feedback, 70 supervisor CI resilience,
78 release packaging acceptance, 79 sanitizer and soak workflows.

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

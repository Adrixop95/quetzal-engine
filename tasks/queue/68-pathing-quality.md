Depends: 64-shared-runner-observer
Task: path tracing and group movement quality (L3), a known playtest gap: the clone's routes look different from the original.
Scope: zpath-style suites (tools/make_zpath*, docs/rules_movement.md): obstacles, chokepoints, crowds, unreachable goals, disconnected group destinations, boats and flight (V05 open items: boat/Johnka routes, Bird speed, radii and footprint, WaveSearch parity).
Method: run each scenario in both programs through the shared runner (task 64), log per-unit positions per tick, and compare arrival tick, stalls, path length ratio and the route shape against the original. Report first divergence with unit, tick and cause class.
Requirements: fix src/engine path code until arrival ticks stay within a documented tolerance, no stalls appear that the original lacks, and path length ratio is bounded; keep existing 48 movement configurations hashes unchanged unless a reference measurement justifies a change (then document it). Add synthetic regression tests and update docs/parity_matrix.md L3 counts.
Sandbox note: live reference runs are denied inside the Codex sandbox; finish with status "needs live run" and a run sheet in docs/EXPERIMENTS.md when reference data is missing.
Public repo, English only, no game data or captures in git.

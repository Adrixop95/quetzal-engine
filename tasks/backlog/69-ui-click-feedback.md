Depends: 64-shared-runner-observer
Task: UI click animation and highlighting parity (L5), a known playtest gap: buttons in the clone do not show the original press/hover frames and highlights.
Scope: hover, press, release and disabled frames of menu and in-game buttons, command card buttons, selection highlight and selection circles, minimap click and drag, tooltips timing, cursor shape per context, hotkeys, and the audio event fired per click (log only).
Method: scripted input at pixel coordinates (tests/parity/ui scenarios) in both programs; record the UI state sequence (button frame ids, highlight state, emitted command) per tick and compare. Measure the original; do not guess frame timing from code.
Requirements: fix src/engine/ui and rendering until the state sequences and emitted commands are equal for every screen in docs/ui_screens.md, add synthetic regression tests without game data, update docs/parity_matrix.md L5 counts per screen.
Sandbox note: live reference runs are denied inside the Codex sandbox; finish with status "needs live run" and a run sheet in docs/EXPERIMENTS.md when reference data is missing.
Public repo, English only; captures stay in ignored work/parity/ui/.

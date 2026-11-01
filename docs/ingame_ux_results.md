# In-game UX experiment status

Local attempt: 2026-11-01. Task 61 remains incomplete. No original UI behaviour
was measured and no gameplay behaviour was changed on the basis of a guess.
All private captures, inputs, traces and process logs are under ignored
`work/parity/ui/`. The supplied external installation and runtime were read-only;
runs used private game copies and dummy SDL drivers inside `unshare -rn`.

The reference capture probe failed at loopback setup both before and after the
automation change: `Cannot open netlink socket: Operation not permitted`. The
reference executable was never reached. Isolation was not bypassed. The clone
capture probe succeeded both times. Its static capture is unpaired and cannot
establish original-game parity.

| Screen / comparison | Before paired captures | After paired captures | Before measured differences | After measured differences |
| --- | ---: | ---: | --- | --- |
| Mission 1 gameplay, reference versus clone | 0 | 0 | unavailable | unavailable |
| Synthetic gameplay, clone repeat versus clone repeat | 1 | 2 | 0 trace differences; frame equality not checked | 0 trace differences; 0 differing pixels |

The independent script runner now supports `keydown` and `keyup`, including
left/right modifiers and held scrolling keys. A one-frame chord preserves a
modifier that was already held. Repeated keydown does not create another press
edge. Modifier state is included in the raw engine trace. The regression checks
modifier persistence through a drag, a chord and an unrelated hotkey, explicit
release, repeated keydown and keyboard camera movement. These are automation
contracts, not measured rules of the original game.

| Independent control | Before | After |
| --- | ---: | ---: |
| Held-input script unsupported commands, private mission run | 9 | 0 |
| Held-input private mission repeat trace differences | unavailable | 0 |
| Held-input private mission repeat differing pixels | unavailable | 0 |
| Held-input synthetic regression failures | not implemented | 0 |

The two synthetic scripts are `tests/parity/ui-input.txt` and
`tests/parity/ux-held-input.txt`. `engine_unit` checks input snapshots without
game data; `parity_visual_ui` drives the actual clone and compares complete
traces and decoded frames across repeats. Synthetic imagery is invented.
The private mission repeat separately exercised the held-input script with the
user's assets. Neither control compares the original with the clone.

Pending dependencies include a working isolated reference launch and the audited
shared L0/L4/L5 alignment and observation contract described in
[PARITY_TEST_PLAN.md](PARITY_TEST_PLAN.md) and [parity.md](parity.md). Existing
runner/comparator files do not supply fixed-tick reference camera, selection,
UI-state and command normalization. The external prebuilt runtime cannot gain
new instrumentation merely by changing this checkout.

All requested original-versus-clone controls remain pending: clicks and box
selection, double clicks, modifiers and groups, command hotkeys, contextual
cursors, edge and keyboard scrolling, minimap and camera bounds,
acknowledgements, selection overlays and health bars, command card and tooltips,
messages, pause and speed, placement, repair/sell, rally and resource interaction.
No numerical expectations or evidence labels were promoted.

Validation before and after: Release build succeeded; 34 CTest entries passed
and six skipped. Pre-commit passed using the existing local hook cache; fetching
a fresh hook environment was unavailable because DNS resolution failed. The
post-change suite includes the held-input and frame-repeat assertions above.

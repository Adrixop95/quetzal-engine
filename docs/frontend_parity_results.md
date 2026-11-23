# Front-end capture evidence

Evidence labels (audit 2026-11-23): V = inspected source fact, with reference
or clone scope stated; M = measurement rechecked from retained captures or an
offline clone probe; G = inference, proposed procedure or uncorroborated
historical report. A clone V does not establish native parity. A marker covers
one paragraph/list item or the entire immediately following table. Unresolved
meanings remain G even inside a table whose codec structure is V.
See [label_audit.md](label_audit.md) for counts and evidence limits.

**G** (historical report). Historical scope: Capture counts and incomplete-screen descriptions are the 2026-11-01 snapshot; newer frontend/campaign/layout work must be assessed separately.

**G** (historical report). Local verification: 2026-11-01. Task 60 remains incomplete. Task 58's shared,
fixed-frame reference UI pipeline is absent from this checkout. The work here
supplies independent front-end input/capture automation, a provisional route and
engine-only regression controls. It does not establish original-game parity.

**G** (historical report).

| Capture name | Clone captures after | Changed pixels between repeats | Clone pixels changed from baseline | Paired reference captures before | Paired reference captures after |
| --- | ---: | ---: | ---: | ---: | ---: |
| main-normal | 2 | 0 | 0 | 0 | 0 |
| main-hover | 2 | 0 | unmeasured | 0 | 0 |
| main-held | 2 | 0 | unmeasured | 0 | 0 |
| main-returned | 2 | 0 | unmeasured | 0 | 0 |
| campaign | 2 | 0 | 0 | 0 | 0 |
| custom | 2 | 0 | 0 | 0 | 0 |
| briefing | 2 | 0 | 0 | 0 | 0 |
| options | 2 | 0 | 0 | 0 | 0 |
| audio | 2 | 0 | unmeasured | 0 | 0 |
| modifications | 2 | 0 | unmeasured | 0 | 0 |

**G** (historical report). Every image contains 480000 pixels. Baseline comparisons use the same legacy
screen-walk hook before and after the edits; repeat comparisons use actual
scripted input and include the cursor. These are separate controls. Matching
clone images are not measurements against the reference. The two normalized
clone traces each contain 38 frames and match exactly. Audio is unobserved.

**G** (historical report).

| Requested flow | Paired measurements before | Paired measurements after | Status |
| --- | ---: | ---: | --- |
| Main menu | 0 | 0 | reference blocked |
| New game / difficulty | 0 | 0 | reference blocked; clone entry point incomplete |
| Campaign selection | 0 | 0 | clone route captured; reference blocked |
| Briefing | 0 | 0 | clone route captured; reference blocked |
| Load dialog | 0 | 0 | reference blocked; front-end entry point absent |
| Save dialog | 0 | 0 | reference blocked; mission flow unmeasured |
| Video options | 0 | 0 | reference blocked; clone options screen captured |
| Audio options | 0 | 0 | clone route captured; reference blocked |
| Control options | 0 | 0 | reference blocked; clone entry point incomplete |
| Game options / persistence | 0 | 0 | reference blocked; unmeasured |
| Credits | 0 | 0 | reference blocked; clone entry point absent |
| Exit confirmation | 0 | 0 | reference blocked; clone quits directly |
| Multiplayer entry points | 0 | 0 | reference blocked; clone entry point absent |
| Tab order / hotkeys | 0 | 0 | reference blocked; unmeasured |
| Escape / window close | 0 | 0 | engine-only regression controls |
| Hover / press / release | 0 | 0 | engine-only captures; activation occurs on press |
| Sounds / fades / transitions / cursor | 0 | 0 | reference blocked; unmeasured |

Two isolated reference attempts failed at loopback setup with a netlink
permission error, before the runtime launched. Isolation was preserved. No
reference screenshot, sound sequence or state sequence was obtained. No
original-versus-clone before/after error rate can be calculated for any screen.
No behavioral parity fix is claimed.

Reproduction uses `tools/ui_capture.py frontend` with `--game`, `--runtime`, `--clone`
and a fresh `--output work/parity/ui/NAME`. The default scenario is
`tests/parity/ui/frontend.json`. Each process has its own private game copy;
immutable resources are symlinks and writable installation state is copied.
Both runners use dummy SDL inside `unshare -rn` with a kill timeout. The
reference additionally requires the documented namespace network setup.
Reports, scripts, traces and all images stay in ignored `work/parity/ui/`.

**V** (clone). `--menu-script FILE` consumes frame-numbered events starting at zero: `move X Y`,
`down`, `up`, `keydown NAME`, `keyup NAME`, `shot PATH`, `close`, and `quit`.
Frames with no events preserve held input. Key names are SDL scancode names;
coordinates are renderer pixels. Comments start with `#`; screenshot paths may
contain spaces. A fixed 60 Hz clock drives cursor animation. EOF terminates on
the frame following the final event; choosing a mission ends automation before
the simulation starts. Invalid scripts, failed captures and trace writes return
a process failure. `--menu-trace FILE` records the rendered and next screen,
button geometry/hover/held state, input edges, changed options, chosen mission
and termination. Button records cover the front-end's ordinary buttons; audio
and modification controls use separate drawing paths. The trace explicitly marks
audio as unobserved. It records engine behavior without reference normalization.

**G** (historical report). The reference script uses the existing wall-clock `RT_SCRIPT` interface. Its
navigation follows the documented player/campaign/briefing route and is
provisional until observed. The runner may report exploratory image differences
if both captures exist, but always returns failure for unmeasured parity. A
rebuilt reference with fixed-frame input/capture and semantic UI/sound observers
is still required before L4/L5 acceptance or menu behavior changes based on
measurements. Reference timing, sound controls, missing screens, persistence,
focus/tab behavior and actual pressed animations remain open.

**G** (historical report).

| Validation | Pass | Fail | Skip |
| --- | ---: | ---: | ---: |
| Baseline CTest | 34 | 0 | 6 |
| Final CTest | 35 | 0 | 6 |
| Separate isolated private format/engine checks | 2 | 0 | 0 |
| Baseline and final pre-commit gates | 2 | 0 | 0 |

The Release build passed before and after the edits. Pre-commit used a writable
copy of locally cached hook environments because remote DNS was unavailable;
the final run also included every new file explicitly. No git metadata was
changed and no commit was created.

## Label audit corrections (2026-11-23)

Evidence checked: Reference: no retained paired reference frames support this report; static menu research cannot verify screenshot counts. Clone: `tools/ui_capture.py`, `app/menu_script.hpp` and frontend capture tests. Frame-numbered scripting is V clone behavior; historical image/trace counts remain G, and no reference image equality is inferred.

1. Capture counts and incomplete-screen descriptions are the 2026-11-01 snapshot; newer frontend/campaign/layout work must be assessed separately.

No original executable or reference runtime was run for this audit. Historical
validation totals retain their original dates; they are not this
audit's test results. Private captures, derived source and probe output remain
outside the tracked documentation.

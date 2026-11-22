# In-game interface implementation

V = a documented original fact (inspection, not a new live observation).
G = an engine approximation, inference, unresolved value, or missing integration.
Passing synthetic tests verifies the implementation contract, not original pixels
or behavior. No original executable or reference runtime was launched for this task.

Evidence: [interface_geometry.md](interface_geometry.md),
[interface_ingame.md](interface_ingame.md),
[worldview_mouse_cursors.md](worldview_mouse_cursors.md), and
[frontend_screens.md](frontend_screens.md). The corrections in interface_geometry
supersede the older panel, tooltip, tactic and repair interpretations. No private
code, artwork, screenshot or extracted data is included in this implementation.

## Choosing a layout

`aztec_engine --layout original --size 800x600 --game DIR 1` enables the original
in-game layout. `--layout modern` is the default. This is a presentation/input
option, independent of `--original-look`, simulation modifications, save formats
and the front-end layout. It is not persisted in the options file. Installed
resources are read at runtime; no assets are supplied.

G: original mode uses the verified 800x600 coordinates, translated horizontally
with the window edge, with an integer scale constrained by 800x600. The lower
block remains bottom-anchored. Other window sizes are supported as engine
adaptations, not verified original resolution modes. In particular, the native
1024x768 list row count, button/grid offsets and alternate art remain unresolved.
Modern mode retains its existing scaling and five-column card.

## Geometry and controls

Rectangles below use `(x,y,width,height)` at 800x600, with half-open hit edges.
Drawing and input share the same geometry functions in `ui/panel_layout.hpp`.

| Element | V original value | Implementation / comparison |
| --- | --- | --- |
| World / panel boundary | Full-height 204-wide strip at x=596 plus lower block (546,392,254,208) | V geometry in original mode; both layouts reserve the L-shaped corner for input. The camera renders a rectangular 596-wide view behind the corner. |
| Menu/root region | (596,0,60,22) | Original mode corrects modern x=598. G: the menu artwork/action is retained from the clone; the inspected root itself has no tooltip. |
| Minimap box | (608,25,180,180) | Original mode corrects modern (608,28,180,174). |
| Map content | Integer scale `180 / max(map_w,map_h)`; centered within box | V arithmetic; a synthetic 120x90 map produces (638,70,120,90). Modern uses at most one pixel per tile and otherwise fits the opening. G: maps larger than 180 tiles produce zero scale and are safely hidden; original behavior for such inputs is unverified. |
| Town list | (618,218), cells 59x44 | First four rows use V positions. G: list content order uses world object order; additional rows, paging, arrow geometry and native lifetimes remain missing. |
| Team list | (720,218), cells 59x44 | First four rows use V positions. G: one representative per nonnegative engine tactic; implicit tactics, joined subteams and ordering need integration. |
| Squad members | (572,463), four columns by three rows, 47x42 cells | V geometry; original mode shows member icons here instead of modern command icons. G: more than twelve members, scrolling and individual member click behavior remain incomplete. |
| Town build grid | (570,463), three columns by three rows, 42x42 cells | V geometry; existing simulation menu supplies entries. G: wheel paging is an engine substitute for original scroll arrows and native build-tree order. |
| Build queue | (738,463), one column by three rows, 42x42 cells | V geometry; three visible slots, versus eight modern slots. G: engine queue cancellation/refunds and wheel paging retain existing semantics. |
| Money | Right edge W-24, y=5 | V anchor in original mode; G: uses existing font and actual money immediately, without coin animation or per-frame displayed-money smoothing. |
| Script messages | x=10, y=20, pitch 18 | V anchors/pitch in original mode; G: existing font, wrapping and message retirement remain engine policies. |

Squad controls (V):

| Control | Rectangle | Value / effect |
| --- | --- | --- |
| Center | (560,406,59,44) | Center on selection |
| Aggressive | (624,405,48,44) | AttackMode 2 |
| Defensive | (672,405,38,44) | AttackMode 1 |
| Balanced | (710,405,38,44) | AttackMode 3 |
| Direct Control | (748,405,38,44) | AttackMode 0 |

Town controls (V):

| Control | Rectangle | Value / effect |
| --- | --- | --- |
| Center | (558,406,59,44) | Center on selection |
| Manual Repair | (626,406,40,44) | RepairMode 1; repeated activation switches off |
| Manual Destroy | (666,406,36,44) | RepairMode 2; repeated activation switches off |
| Build speed | (702,406,34,44) | BuildMode |
| Current building | (746,407,42,42) | PauseBuild |

V: original mode switches the lower card and controls by town/squad selection,
using common:293 for the upper strip and 295/297 for squad/town lower art.
G: engine structure selection identifies the town; it does not reproduce native
town/tactic hierarchy. Buttons use text inside their verified rectangles while
pressed/disabled imagery remains unverified. Production/member icons reuse the
existing runtime resource lookup. Build-speed toggling uses normal/fast as an
explicit engine policy; the original fast/economy transition needs confirmation.
Center uses selected-object centroid rather than a native tactic origin.

V: original tooltips split at the first newline, use one or two lines, appear
above the hovered control's top-left and clamp to the screen. Original mode
implements these placement rules. G: font slot 2, exact colors/frame, delay,
prerequisite/error text, grey-out and extreme text clipping remain incomplete.
Modern mode retains its wrapped tooltip beside the lower card. There is no new
invented hover status line. Original mode hides the debug HUD initially; F10
still enables it as an engine extension.

## Cursor and mouse rules

V: original-mode cursor precedence is unavailable world -> sprite 1;
modal, placement or outside world -> 4; active scroll -> 14..21; box drag or no
selection -> 4; selected town service mode 2 -> 2, other nonzero mode -> 5;
otherwise hostile visible hover -> 0, default/own/allied/empty -> 4.
There is no separate world move cursor. Legacy enum names `Menu`, `Move` and
`Pan` identify sprites 4, 2 and 5 respectively; they do not establish visual
meaning. Scroll indices in compass order N, NE, E, SE, S, SW, W, NW are
14, 21, 20, 19, 18, 17, 16, 15. Animation retains 80-ms steps (V period,
G absolute phase/reset). Modern targeting/panning cursors remain unchanged.

V: original mode uses right release for orders, including a right drag; it does
not pan with a right drag. Right press cancels the service-target cursor.
A service-target left release submits the existing marked-building toggle;
simulation validation owns eligibility. Minimap left hold centers the camera,
right release sends an order, and Inv+left selects the nearest own engine tactic
within 240 world pixels. Hiding the original interface also hides its minimap.
G: nearest-object tie order, original packet modifier flags, waypoint queues,
joined squad routing, town minimap selection and exact camera clamping still
need the native tactic/input adapters. Middle-button panning and M/X/H remain
engine extensions. Edge-scroll acceleration and exact four-pixel edge detection
are unchanged engine policies, not original parity claims.

## Selection box and group hotkeys

V: original drag starts only when either screen axis exceeds 16 pixels, strictly.
Modern retains four pixels. Original boxes exclude structures and use strict
rectangle overlap, not center containment. Empty boxes/clicks preserve selection.
Plain click expands an owned unit's existing engine tactic; Ctrl-click selects
one unit; Shift-click adds only that unit, excludes buildings, and Shift+Ctrl-click
does nothing. There is no added world double-click selection rule.

G: the native object's half-open rectangle getter and first-match bucket scan
are not available to this UI. Clicks still use the clone's nearest-object picker.
Boxes accept a rectangle provider; without one they use a documented 48x48 proxy
around each interpolated position. No engine production caller supplies verified
native bounds yet. Ctrl-click changes selected IDs without establishing native
persistent detachment. Shift-box still flattens IDs; the documented silent joined
sub-team hierarchy and FilterType/FilterCondition callers remain missing.
These adapters are dependencies, not substituted reference facts.

V: all digits 0..9 assign with the remappable Inv action and otherwise recall in
original mode. Ctrl+Shift+digit replaces the group when Inv is held, rather than
invoking modern's extra assignment-add behavior. Modern retains Ctrl assignment,
Ctrl+Shift append and digit recall. G: original raw-left-Shift digit routing,
native town/squad number storage and assignment lifetime are not reproduced by
the engine's ten ID lists. No on-screen group-number art is invented.

## Remaining G values and dependencies

Beyond the gaps attached to individual rows above, these need further evidence:

- Native 1024x768 region relocation, extra list row and art selection by display mode.
- Button draw-state flags, pressed/disabled art and exact availability per selection.
- Money interpolation, coin frames, emblem/day-clock placement and redraw cadence.
- Minimap dot palette `17 + side*4`, dot footprints, camera frame and refresh cadence.
- Selection ellipse and health-bar geometry integration, exact colors/thresholds,
  rubber-band colors and any on-screen group labels.
- Tooltip timing, original font slots/baselines and all availability text branches.
- Native object rectangles/scan order, ownership/alliance corner cases, tactic
  hierarchy, detachment/joining and modifier filters.
- Original outcome banner (200x30 centered in the view), pause/objectives/save/options
  overlays and their interaction with the existing engine result/navigation flow.
  Front-end geometry remains governed by frontend_screens.md; no front-end rewrite
  is bundled into this in-game mode.

No new dependency task is assumed merged. Work here uses the existing simulation
command adapters; missing native selection/group/input adapters, UI observation
and original-save interoperability prevent a full original-interface verdict.
The prohibited reference executable was not used to close these gaps.

## Verification

`interface_layout` uses invented world objects and no game installation. It checks
literal geometry at 800x600, integer scaling, half-open panel/button boundaries,
the L-shaped world corner, build/member/queue cells, minimap integer scaling,
tooltip anchors, drag thresholds and release-only completion, strict overlap,
selection modifiers, focus cancellation, every cursor precedence branch and all
ten digit assignments/recalls. Command clicks check actual submitted stance and
town-service payloads; a background gap must submit nothing. CLI checks cover
default/explicit layout, invalid values and missing arguments. Existing synthetic
`engine_unit` and `ui_screenshots` checks retain the modern layout contracts.

2026-11-22 validation: baseline Release build, all 82 baseline CTest entries
(zero failures, six skips) and pre-commit passed before editing. The final
Release build and all 83 CTest entries completed with zero failures (77 passed,
six private-data/restricted-process skips). Pre-commit covers tracked changes
and the two new files explicitly with read-only git metadata.

The private-copy `test_formats` and `test_engine` checks also passed separately
with both SDL dummy drivers under `unshare -rn`. A three-frame original-layout
clone render and UI trace at 800x600 succeeded in the same isolation; its cursor
trace records sprite 4's enum without reporting hidden modern buttons. Private
outputs remain inside the ignored build directory. This checks installed resource
loading and clone rendering only; no original screenshot comparison, reference
execution, native platform validation or full UI parity is claimed.

# Player commands and input audit

Status: partial implementation; needs native Windows comparisons. No original
executable was run. This is not an assertion of a complete command set or game
parity. Static inspection and synthetic tests cannot establish that acceptance.

V means a fact read from the private original inputs, not a live measurement.
G means an assumption, an unresolved original behavior, or a clone approximation.
Statements about the current clone describe source/tests, not original evidence.
Only independent English descriptions of behavior are recorded here; no assets,
extracted tables, script bodies, executable-derived code or captures are included.

## Sources and scope

- V: the installation's `readme.doc`, controls section, documents squad selection,
  mouse commands, camera gestures, production controls and menu access. Its
  instructions were read locally and paraphrased here.
- V: `Keys.ini` supplies named actions and bindings. The presence of an action
  name establishes a binding, not all of its behavioral branches.
- V: `Res/strbase.bds` contains interface labels for tactic modes, manual town
  services, production speeds, price/time, existing/in-progress buildings,
  unavailable construction, money, queues, scrolling and pause states. Strings
  alone do not establish a control's availability predicate or hotkey.
- V: private lifted input routines `f_0042e210`, `f_0042e880`, `f_0042e8c0`,
  `f_0042e900`, `f_00434af0`, `f_004355d0` and `f_00435bc0` distinguish held,
  toggle and dispatch behavior, install digit/menu bindings, read selection
  modifiers, centre the camera and handle fast scrolling. Function boundaries
  and corresponding decompilation were used as navigation aids; instruction
  comments were checked for the camera action calls. These private files stay
  outside the public tree. Exact timing and side effects still need observation.
- V: the existing tactic research in [rules_ai.md](rules_ai.md#tactic-attack-modes)
  identifies direct, defensive, aggressive and balanced simulation modes.
  Clone packet fields are engine conventions, not original wire compatibility.

The task is broader than adding conventional RTS shortcuts. In particular, V:
the manual describes persistent squads and towns, not an arbitrary flat selection
with a universally available worker command card. G: ordinary RTS expectations
about double-click, select-all, patrol or harvesting must not fill that gap.

V: the recognized Keys.ini action vocabulary was checked against its original
name table, reached from `f_0042e210`. All installed bindings are accounted for:

| Actions | Installed keys | Evidence boundary |
| --- | --- | --- |
| Add / Inv / Center | Both Shift / both Ctrl / both Alt | V bindings; selection and monitoring distinctions above remain important. |
| FastScroll / ViewAll / Temporary / PutOnTop | Caps / V / Tab / T | V bindings; Temporary behavior remains G. |
| Garrison / AutoRepair / Repair / Destroy | Grave / Q / W / E | V bindings; exact service transitions remain G. |
| Aggressive / Defencive / Normal / DirectControl | A / S / D / F | V bindings; original spelling of Defencive is retained for compatibility. |
| LastCaller / ToggleInterface / PrintScreen | Space / F12 / PrintScreen | V bindings. |
| ScrollLeft / ScrollRight / ScrollUp / ScrollDown | Arrows and keypad 4 / 6 / 8 / 2 | V bindings. |
| FilterType / FilterCondition / Send / Friend | No installed binding | V recognized names; G behavior and modifier interaction unresolved. |
| CheatWin | No installed binding | V recognized debug action; not exposed as a normal player command. |

V: Escape and digits are installed separately by the original input initializer.
G: this clone's no-file fallback uses the inspected installation's familiar
bindings; it does not reproduce every branch of the original no-Keys.ini table.
In particular, V: that table uses different automatic/manual-service keys and a
different screenshot key. Preserve the actual Keys.ini for native comparisons;
test a missing file as a separate case rather than silently mixing defaults.

## Selection and groups

| Input or operation | Original evidence | Clone and remaining gap |
| --- | --- | --- |
| Click an owned warrior | V: selects its squad | Now expands a mobile object's existing tactic to living owned mobile members. G: member order and mixed off-screen/hidden members need comparison. |
| Ctrl-click a warrior | V: creates a one-warrior squad | Selects one member. G: it does not detach that member into a new persistent simulation tactic. |
| Drag a box | V: creates a squad from the chosen warriors | Selects owned mobile objects. G: no persistent tactic creation/merge; four-pixel drag threshold remains an engine choice. |
| Shift-click or Shift-box | V: adds warriors to the squad | Adds IDs without duplicates through the remappable Add action. G: persistent squad membership changes remain missing. |
| Click a building | V: selects its town | Selects that object; production lookup resolves its town. G: no full town/squad list selection model. |
| Ctrl+digit; digit, including zero | V: assign and recall a squad/town number; input dispatcher reads the Inv modifier | Clone stores ten lists of IDs, supports recall, and additionally supports Ctrl+Shift assignment-add. G: the extra combination and assignment persistence are not established original behavior. |
| Double-click same type | G: the mouse layer detects a double click; same-type selection semantics were not established | No same-type selection is invented. Capture its effects on a member, portrait and list row separately. |
| Select all / type / condition filters | V: FilterType and FilterCondition are recognized action names; G: scope, defaults and effects unresolved | No select-all binding is asserted. ViewAll is a status display action, not select-all. |
| Tab temporary selection; T list priority | V: Temporary is bound to Tab; the manual describes T moving the current entry to the top | No temporary selection or reorderable squad/town lists. |

Selection gestures now cancel on focus loss or when their world input region
becomes inactive. G: this is a clone robustness policy; it is not a measured
original drag-across-panel rule. Selection changes remain presentation state;
gameplay actions go through deterministic simulation commands.

## Orders and town services

| Operation | Original evidence | Clone and remaining gap |
| --- | --- | --- |
| Move | V: right-click terrain or minimap | Supported. M targeting is an engine extension. |
| Attack | V: right-click an enemy warrior or building | Supported against visible objects. X targeting is an engine extension. G: damage eligibility, diplomat/allied ownership and cursor predicates need live comparison. |
| Stop | V: right-click the current portrait or squad/town list icon | H/button submits Stop. G: portrait/list right-click behavior remains missing; world right-click is not evidence for Stop. |
| Hold | G: no separate hold-position command established | Do not equate Stop or direct control with a verified Hold command. |
| Patrol / guard | G: separate commands and bindings not established | No speculative keys or packets added. |
| Escort / follow | V: right-click a friendly warrior; the manual also describes transfer to a friendly town | Friendly mobile tactic now submits FollowTeam. Existing simulation rejects cycles. G: town transfer, attachments and combined modifiers remain unsupported. |
| Attack-move | G: original gesture not established | X then terrain uses the clone AttackMove extension. |
| Waypoints | V: Shift-right-click adds a route checkpoint on terrain, minimap or a list entry | Missing simulation route queue and UI append semantics. Current clone right-click replaces the destination; this remains a known divergence. |
| Squad joining / rally routing | V: Ctrl+Shift-right-click is documented for joining and routing future town production to another squad/town | Missing town routing and persistent tactic membership changes. Do not describe a coordinate rally flag as the verified original rule. |
| Formations | G: no manual formation-choice binding established here | Automatic clone formation placement exists. Shape, heading, spacing and whether any player selector exists need capture. |
| Town troops | V: select a town and use the grave key to extract a squad | Garrison submits the existing extraction command. Selected vessels instead expose the clone Unload action. G: exact original context priority remains unmeasured. |
| Repair / destruction | V: Q/W/E bind AutoRepair/Repair/Destroy; strings distinguish manual repair and destruction | Q now submits automatic town service. Existing W/card cycles service modes; E toggles eligible marked buildings. G: this is not a verified match for the three original control transitions. |
| Tactic behavior | V: A/S/D/F bind aggressive/defensive/normal/direct control; strings and tactic research identify the four modes | These keys now submit AttackMode. G: exact key-to-button dispatch and all original membership side effects need native confirmation. Ctrl/Alt chords do not issue mode commands. |
| Build / training | V: left-click an available production entry starts it; the manual describes right-click interruption, current-item pause/cancel/resume and removal of waiting items | Existing paged card and explicit placement supported. G: right-click production controls, cancellation phases and exact card layout remain divergent. |
| Harvest | G: no direct worker-harvest command established | Clone economy runs through town producers and descriptor schedules. Do not add an invented gather key. |
| Production speed | V: the string database distinguishes fast/economy build modes | Simulation BuildMode exists; G: original selector cycling, labels and availability need capture before changing UI. |

Mode, follow and repair inputs submit commands; no direct simulation-state edits
were introduced. Synthetic tests check submission, application and command-log
replay. These tests demonstrate clone consistency, not equivalent original squads.

## Camera, game control and messages

| Operation | Original evidence | Clone and remaining gap |
| --- | --- | --- |
| Arrow/keypad scrolling | V: Keys.ini binds directional actions; the view update polls held actions | Existing remappable scrolling. G: acceleration, diagonals, repeat and boundary pixels are unmeasured. |
| Edge scrolling | V: mouse input checks the display edges | Existing focused/inside-window scrolling. G: exact margin, speed, panel-edge behavior and cursor timing need capture. |
| Caps-held mouse motion | V: manual and FastScroll branch move the camera in the mouse-motion direction | Now works without a mouse button. Uses the existing drag camera with reversed world-follow sign. G: zoom scaling and original gain are unmeasured; clone button-drag remains an extension. |
| Minimap click | V: changes viewpoint | Existing click/drag centring. G: exact clamp, drag behavior and right-click routing need capture. |
| Alt / current portrait / list icon | V: centre on selected squad/town; Ctrl-click list icon centres without selecting | Alt binding now centres selected IDs. G: centroid is a substitute for the original tactic origin. Portrait/list centring remains missing. |
| Ctrl+Alt monitoring | V: documented by the manual | Missing camera tracking mode. Alt centring does not establish monitoring parity. |
| Scroll/game speed options | V: both option labels and profile fields exist | Clone camera still uses its independent fixed speed, and simulation uses its fixed clock. G: no conversion from original options to scroll gain or simulation scheduling has been measured. |
| V status display | V: manual describes showing/hiding all warriors' status; dispatcher has a ViewAll action | Now toggles visible-object health bars. G: town inclusion, persistence, exact status contents and geometry need capture. Fog remains enforced. |
| Escape | V: opens the menu and pauses play | Existing pause menu; cancels targeting before opening it. G: original targeting/menu precedence needs capture. |
| Pause key / game speed keys | G: physical bindings and multiplayer effects not established by this audit | P simulation pause and unbound Space view-pause are clone extensions. Default Space is reserved for LastCaller, including fallback bindings. |
| Quick save/load | V: save/load menu labels exist; G: no original quick-key mapping established | Ctrl+S/Ctrl+L and replay saves are clone facilities, not native save compatibility. |
| Messages / Space | V: Space centres on the last event | Clone displays timed text but lacks event-location history/LastCaller handling. Space must not silently become pause with the fallback key map. |
| F12 / PrintScreen / Alt+F4 | V: manual documents interface toggle, screenshot and exit | Interface toggle and screenshots exist; SDL handles normal close events. G: original bitmap format, paths and window-manager behavior are not verified. |

The fallback binding set now reserves documented selection/camera/status/list
actions, even where implementation remains pending. This prevents engine-only
fallback keys from claiming them. User-supplied bindings remain authoritative.

## Cursor, panel and tooltip audit

V: the manual describes interacting with a current portrait and separate squad,
town, production and queue lists. V: string resources distinguish mode controls,
list scrolling, build progress, already-existing structures, construction failure
and insufficient money. G: those strings do not prove which of these appear as
hover text, disabled reasons, transient messages or a hidden button.

The clone shows a portrait, first-object name/HP/count, six generic actions, a
paged production grid and queue controls. It computes enablement from owned
living objects and available rules. Disabled commands now always include an
English reason in their tooltip, including repair/garrison/destruction; previously
those descriptions could conceal that the command was unavailable. The reasons
describe clone predicates and are G as original wording/presentation. Production
already has prerequisite/queue-full feedback; payment-at-start must not be
replaced with a universal affordability gate without evidence.

The clone cursor chooses arrow, move, attack, pan and eight scroll directions.
G: context-specific original boarding, follow, repair, forbidden placement,
friendly/enemy/fog, minimap and disabled-button cursors have not been established
by static action names. Existing atlas identities/animation timing are not new
measurements from this task. Tooltips use the user's strings where already wired;
no extracted strings or art are embedded by this change. Exact hover delay,
animation frame, clipping, button shade and reason priority remain open.

## Dependencies and native Windows run sheet

The existing SelectByNo/FollowTeam, tactic-mode, production, service and transport
simulation commands are present in this checkout; those parts did not require a
missing task. Persistent squad creation/detachment/merge, waypoint queues, town
production routing, event-location history, monitoring and original speed-option
mapping are missing behavior/model dependencies. The UI cannot reproduce them by
renaming buttons or substituting individual control-group IDs. Their absence is
explicit above. No reference execution or namespace bypass was attempted.

On the maintainer's Windows installation, record two independent repetitions of
each case below. Use a disposable copied profile/save, not the primary profile.
Retain evidence privately. Record original executable version/hash, clone revision
and diff, mission/save, resolution/display scaling, selected side, initial tactic
membership, camera origin, scroll/game speed, modifiers, cursor coordinates and
press/release times. Compare clone and original from matching initial conditions.
Do not infer a result from this run sheet or a requested screenshot filename.

| Case | Actions to capture | Measurements and first-divergence fields |
| --- | --- | --- |
| S1 selection | Click a member of a multi-member squad, Ctrl-click it, box part of the squad, Shift-add, click town structure; repeat with dead/hidden/off-screen members | Selected identities, persistent tactic membership after move/save/reload, portrait/list count, ring ownership, drag threshold |
| S2 filters/groups | All digits including zero, Ctrl assignment, Shift/Ctrl+Shift combinations, repeat recall, double-click world/portrait/list, Tab and any configured filters | Actual selection scope, assignment replacement/addition, list order, camera changes; do not pre-label double-click as same-type |
| O1 orders | Right-click terrain, enemy, friendly warrior, own portrait/list and friendly town; Shift-right-click three terrain/minimap targets; Ctrl+Shift routing | Order type, full route, target identity, attack/capture/stop phase, post-production destination, squad membership |
| O2 modes/services | A/S/D/F on squad and town; Q/W/E on town, damaged structure, healthy structure and no selection; grave on town/vessel | Highlighted control, changed mode, eligible objects, marking toggles, clear/reset conditions and rejected input feedback |
| O3 build | Available/locked/unaffordable/duplicate entries, started/paused/completed work, waiting queue, left/right click and wheel; each production-speed control | Card cell/order, visibility versus disabling, reason priority, exact text, cost/payment/refund tick, queue transitions |
| C1 camera | Each arrow/keypad direction, corner edge, panel edge, minimap click/drag, Caps mouse motion, Alt and Ctrl+Alt; repeat scroll-speed minimum/midpoint/maximum | Origin per frame, gain, acceleration, diagonal norm, clamps, monitoring anchor, motion while menu/pause/focus changes |
| G1 controls | Escape during selection/targeting/build placement, menu pause/resume, speed slider, any claimed quick keys, Space after multiple events, F12, V | Tick progression, screen order, event cycling/expiry/location, tooltip persistence, option/profile persistence |
| U1 contexts | Hover empty land, enemy/friendly/neutral/unit/building/vessel, fog, valid/invalid placement, minimap, every enabled/disabled control | Cursor resource/frame/hotspot, animation cadence, tooltip delay/anchor/clipping, button shade/highlight, disabled reason versus transient message |
| U2 selection panel | Single/mixed types, multiple squads, town, vessel, empty selection; short/tall display modes | Portrait identity, HP/count aggregation, card/list geometry, focus/release hit regions, right-click/keyboard equivalence |

For each row report `V observed` only after both native repetitions agree;
otherwise retain G/unresolved and the divergent observations. Separate a missing
control from a control that exists but is unavailable in the chosen context.
Probe patrol/guard/hold/formation/select-all/harvest/quick-key candidates only
when an original menu, binding or documented gesture supplies a candidate. Keep
screenshots, original saves, raw tables and input/capture logs outside the repo.

## Verification

Baseline: bootstrap with `--no-hooks`, independent Release configure/build,
69 CTest entries (63 passed, six skipped), and all-file pre-commit passed.
Skipped entries require private data or restricted network process creation.
The original installation was read only for research; no original game process
was started.

Post-change Release build and all 69 CTest entries completed: 63 passed, six
skipped, none failed. The same private-data/network entries skipped as at the
baseline. All-file pre-commit, explicit pre-commit for this untracked document,
and `git diff --check` passed. Added synthetic checks cover squad/individual/box
selection, gesture cancellation, held/remapped centring, fast-scroll gating,
mode and service command submission, friendly follow, replay consistency,
disabled repair tooltips, status rendering and fog exclusion. The gameplay
input-trace test now checks Alt centring and velocity reset after arrow scrolling.
Native Windows behavior and all fifteen mission completions remain unverified.

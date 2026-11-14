# Front-end screens

This is a **partial reconstruction**, not a front-end parity verdict. V means a
fact inspected in the private lifted instructions or installed data. G means an
assumption, engine policy, or presentation choice without an original observation.
A successful synthetic check establishes an engine contract, not original pixels,
sound timing, or game behavior. No reference executable was run for this task.

## Evidence and runtime resources

V: the inspected installation provides `Res/common.xbe`, `fnts.xbe`, `Pal.xbe`,
`Snd.xbe`, `strbase.bds`, `Maps/*.mp`, `Maps/*.mit`, and `Video/*.avi`. These are
read at runtime. The public tree contains no extracted images, fonts, audio,
credits transcript, screenshots, archive dumps, or executable-derived source.

V: private `gen_052.c`, function `f_0041eef0`, constructs the front-end widgets.
The source instruction addresses below identify research locations, not copied
implementations. `f_00427730` through `f_00427af0` associate controls with string
database ordinals. `f_00412960` selects normal state 1, hover/held state 2, and
disabled state 3; it does not establish separate hover and held colors.
`f_00412d30` centers text in its button rectangle and draws through font slot 4.
`f_00463740` constructs five font slots with bases spaced by 300; slot 4 uses
base 1200 in `fnts.xbe` group `0x44c`. The inspected glyphs name palette 53,
which contains multiple state palettes. Front-end text controls now use these
installed state palettes without an extra color tint or invented button frame.
The existing built-in font remains an asset-free fallback (G).

V: `common.xbe` group `0x44c` contains full-screen backgrounds 264, 270, 280,
284, 285 and 290; illustrated overlays 256, 271 and 272; and scroll artwork
289/291. The image headers and private visual inspection distinguish the temple
(280), framed landscape (284), briefing desktop (285), book (270), scroll (264)
and framed order (290). Presence of an image does not prove every use below.
V: sprite 4 is the menu mace; sprite 1 is the modal quill in `f_00464290`.
The engine now uses the mace in its front end. Modal cursor switching is pending.

G: the engine letterboxes an 800x600 coordinate space. Fractional font/cursor
scaling still rounds to integer pixel multipliers. Native resolution switching,
exact text baselines, clipping and original palette conversion need live captures.

## Player selection / main screen

V: the initial front end is a player chooser, rather than the clone's former
large title and vertical Campaign/Custom/Options/Quit buttons. The initialization
uses background 284 and the following rectangles. Rectangles use `(x,y,w,h)`;
the lifted constructor stores left/top/right/bottom, converted independently.

| Control | Rectangle | String ordinal | Evidence location |
| --- | --- | --- | --- |
| Create Player | (72,28,170,60) | 132 | 0x41f105; constructor 0x427730 |
| Remove Player | (314,28,172,60) | 133 | 0x41f0c5; constructor 0x427780 |
| Credits | (556,28,172,60) | 134 | 0x41f088; constructor 0x4277d0 |
| Accept Player | (72,512,170,60) | 136 | 0x41f046; constructor 0x427820 |
| Quit The Game | (556,512,172,60) | 137 | 0x41f009; constructor 0x4278c0 |

V: heading ordinal 30 is Choose Player. G: its current position and the single
engine `Player` row are provisional. Original player import, player creation,
removal and their name/confirmation dialogs are **not implemented** here; their
buttons are disabled. The existing engine campaign profile remains independent
of the original `.plr` files. Original quit confirmation is also pending.

G: Accept/Enter opens campaign selection; Escape quits. Text buttons require a
press inside and a release inside the same rectangle. Holding does not activate,
leaving the rectangle cancels activation on release, and focus loss or a screen
transition discards the armed control. This is independently tested as an engine
input policy; native drag-out/re-entry and keyboard focus need comparison.

## Campaign / new game and custom missions

V: `f_0041eef0` constructs Load Game at the top left, Custom Mission at the top
right, Play Mission at the bottom left, Begin Campaign at the bottom middle,
and Cancel at the bottom right, using the same six outer rectangles. The string
ordinals are 138, 37, 139, 140 and 148 respectively. The list construction at
0x41fd91 specifies origin `(140,250)` and nine rows. The list widget uses a
20-pixel row pitch. Previous and next controls are `(620,230,60,40)` and
`(620,400,60,40)` at 0x41fd32/0x41fce5.

G: the current list width is 450, title position is `(400,192)`, and selection
uses a text prefix. The engine selects a row before Play/Enter opens its briefing;
Begin Campaign opens mission 1. Fifteen engine campaign missions use two pages.
The existing engine completion mask controls unlocks; this is not original player
profile compatibility. Options occupies a convenient top-middle shortcut (G).
Custom missions reuse the list and paging geometry (G), with direct row-to-briefing
activation. New-game difficulty, original multiplayer and statistics are pending.

V: mission names/briefings are obtained from the installed mission files. No
mission-specific strings are embedded in this reconstruction. G: background 284
is used while the original composite layers, list selection decoration and
preview/statistics panel remain incomplete. Backgrounds 283/1002 and the journal
art must be checked in an original campaign capture before assigning those layers.

## Briefing desktop, diary, order and objectives

V: the illustrated hit regions are constructed at 0x424a9f through 0x424bb7.

| Control | Rectangle | Evidence |
| --- | --- | --- |
| Start Game | (643,338,135,176) | 0x424a9f |
| Back | (0,0,151,226) | 0x424bb7 |
| Diary | (339,18,256,139) | 0x424b2d |
| Order | (158,197,279,321) | 0x424b75 |
| Objectives | (462,308,148,161) | 0x424ae5 |

V: sprite 285 is the desktop; 256 contains Start Game art, 271 Diary art and 272
Back art. These are transparent full-width overlays, not images to stretch into
the hit rectangles. G: current hover placement uses screen x=0 and y=330 for
256, y=0 for 271/272. Alpha blending, pressed art and exact placement need captures.

V: installed `.mit` files provide sectioned briefing text. G: the engine assigns
the first three sections to diary/order/objectives, using backgrounds 270/290/264
respectively. Missing sections fall back to the mission briefing/objectives. This
section ordering and each background assignment need live confirmation. The
current text boxes are `(140,130,520,340)`, `(235,205,330,270)` and
`(265,170,230,290)` (G); diary pagination and original text fonts are incomplete.
Mouse wheel, arrows, PageUp/PageDown and Home/End scroll. Back first closes the
reading view, then returns to mission selection; Enter/Start launches the mission.

## Load and save

V: original menu navigation exposes nine slots, with a 20-pixel pitch. The dialog
constructor at 0x42312f gives outer bounds `(100,92,372,304)`; actions at
0x4230b8/0x423078 give `(150,328,72,60)` and `(350,328,72,60)`.
G: engine slot rows use `(140,148 + 20*i,300,20)` for zero-based index `i`.
The action rectangles are shared by load/save and Cancel; their complete original
mode mapping, dialog frame, name field and overwrite flow need live comparison.

G: nine **engine** slots now appear in menu and paused-game dialogs. Existing
slot 1-3 paths remain valid; slots 4-9 extend the same per-user naming scheme.
Selection and confirmation do not write; the mission loop performs the confirmed
save or load. Empty/unreadable loads cannot be selected. Escape cancels confirmation
before closing the dialog. A second explicit confirmation remains an engine policy.
Original `.sav` interoperability is a separate dependency: this UI does not turn
the original-save reader into a complete original simulation resume implementation.
Save names, timestamps, deletion and the original Enter Save Name dialog are pending.
The current full-screen landscape backing and text rows are G, not a reproduced
original modal frame.

## Options: sound, video and controls

V: the string database contains Options (48), Gamma (49), Music volume (50),
Effects volume (51), Speech volume (52), Scroll speed (54), Screen resolution
(142), Sampling rate (143) and Acknowledgement (144). These identify original
settings; they do not validate the clone's controls or values.

G: the current options screen retains engine controls for display mode, edge
scrolling, fog and lighting, and links to Audio, Controls and Game modifications.
Its rectangles and title are provisional. Audio controls adjust three mixer gains,
acknowledgements and subtitles, persist preferences and preserve the landscape
background in the front end. Controls documents engine bindings and toggles edge
scrolling; it is not an original key-rebinding editor. Video controls currently
cycle windowed/borderless/fullscreen. Original resolution/gamma/sampling selectors,
scroll/game speed controls and original slider/toggle imagery remain incomplete.
Game modifications is explicitly an engine extension, not an original screen.

V: sound resources exist in `Snd.xbe`; no verified front-end hover/click/transition
sound IDs or dispatch points were established. No guessed sounds are emitted.
The existing mission audio system still loads installed audio. Front-end music,
button sounds and video soundtrack playback remain pending. Slider behavior can
be tested synthetically, but audible comparison requires the maintainer.

## Mission result, continuation and video

V: `f_00419110` selects the mission's `AVI` from installed mission metadata.
`f_00458e40` selects `win.avi` or `lose.avi`; existing video research records the
final campaign ending. The installed movies use 576x420 frames at 12 fps.
The optional ffmpeg path now preserves those dimensions/frame rate instead of
stretching to 640x480 at 25 fps. Missing movies/ffmpeg leave the interactive flow
usable. Video audio, exact scheduling, skip timing and display aspect need live
comparison; wall-clock playback is an engine approximation (G).

G: a normal menu mission launch plays its metadata movie, while loading an engine
save skips it. Normal mission completion plays win/lose and the final engine
campaign mission additionally selects final. Scripted tests skip movies. The
result menu retains the existing Continue/Retry/Main menu policy with landscape
backing; original statistics, buttons, layout and original ending threshold remain
unverified. Result and retry clicks currently use the existing press policy,
not the front-end release policy. The task does not establish visual result parity.

G: engine flow is player chooser -> campaign/custom -> briefing -> mission ->
result -> next briefing or retry/menu. Existing campaign flow tests exercise all
15 synthetic missions, locked selection, victory, defeat, retry and final return.
A real victory persists the existing independent campaign progress; automation
intentionally cannot unlock a user's campaign. `--no-menu` and explicit mission
launches retain their direct-launch behavior. Completion of real missions and
original progression rules are separate simulation/live-validation dependencies.

## Credits

V: the inspected executable's initialized data contains a NUL-terminated credits
block at RVA `0xa9760`, with newlines and inline bytes 1/5. The engine optionally
reads this bounded text from a non-executable PE section at runtime. Neither code
nor credits text is compiled into the repository. Missing/malformed data produces
Credits unavailable. The reader is revision-specific and is not a general PE
credits locator; syntactic checks are not executable-version authentication.

G: inline markers are stripped and the existing single-byte font transport
preserves glyph indices. A scrollable text overlay replaces the original rolling
presentation. Original font changes, encoding, rolling speed, background, exit
behavior and music require a live credits capture. No original staff names are
invented or copied into public fixtures.

## Headless harness and outstanding live checks

The `ui_capture` CTest drives actual front-end input and renders every supported
front-end mode to private temporary PNGs, decodes them into RGB buffers, checks
buffer dimensions, and checks the V main/campaign/briefing rectangles against
independently transcribed expected values. It also checks the held/release phase,
background selection, navigation and repeated traces. The two JSON route files
contain input only, not images. `campaign_ui` renders save/load, audio, objectives,
victory and defeat to software buffers at several sizes and checks the save action
geometry and slot boundaries. Its invented PE fixture tests optional credits text,
executable-section rejection and truncated-input rejection. `campaign_flow` covers
all fifteen synthetic mission transitions. None of these are screenshot goldens.

```sh
cmake --build build --parallel 2
ctest --test-dir build -R 'campaign_ui|campaign_flow|ui_capture' --output-on-failure
# Optional engine-only synthetic captures, written into this ignored worktree directory:
mkdir -p work/frontend-captures
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy build/test_campaign_ui work/frontend-captures
```

The optional command uses invented data only. For installed data both programs
must follow the private-copy, dummy-SDL and namespace procedure in EXPERIMENTS.md.
Do not run the reference here; it is explicitly prohibited in this environment.
The maintainer should retain two original runs per screen/state privately and
record installation revision, language, resolution, profile/unlocks and options.

Pending live checks:

- Player chooser, name entry/removal, credits and quit confirmation; native keyboard
  focus, drag-out/re-entry, hover/held palettes and cursor switching.
- Campaign pages 1/2, locked/unlocked/completed rows, selection art, preview panel,
  Begin Campaign/new-game difficulty and custom-map acceptance.
- Briefing desktop and each reading section: exact art origins, masks, fonts,
  text margins, page breaks, scrolling, Start/Back and mission movie transitions.
- Empty/populated/unreadable save slots, save-name entry, overwrite/cancel/delete,
  timestamps, load failures and menu-to-paused-game consistency.
- Options layout and every sound/video/control setting; actual mixer gain,
  acknowledgement suppression, front-end music and UI sound event IDs/timing.
- Victory, defeat, statistics, retry, next briefing, final ending and restart-time
  campaign progress, using ordinary interactive play rather than automated unlocks.
- Pixel differences and tolerant metrics reported separately, at native 800x600
  and another supported original resolution. Record original and clone audio
  separately; a null audio observation is not a match.

The original-save normalization/resume and audited reference UI/audio observer
work are not supplied by this change. Independent rendering and engine save/flow
work proceeds without them; full original front-end reconstruction remains open.

## Verification on this worktree

2026-11-14: the independent Release build and all 69 CTest entries completed with
zero failures; six entries skipped in the default asset-free/restricted-process
configuration. The private-copy `test_formats` and `test_engine` checks then both
passed separately with dummy SDL under `unshare -rn`. An installed-data front-end
route completed 71 frames and 15 private captures covering all ten supported
front-end modes. This verifies resource loading and clone navigation only. The
reference executable was not launched. Pre-commit gates cover tracked changes
and the two new files separately because git metadata is read-only.

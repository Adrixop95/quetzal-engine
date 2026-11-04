# UI capture inventory and measurement status

Status: partial implementation, measured 2026-11-01. The catalogue has 173
requested screen/state IDs, including command-card entries for all 90 types in
`docs/data/types.csv`. This is a coverage inventory, not a verified enumeration
of every original screen or button. All routes retain evidence P. Unknown routes
and clone features are explicit blockers rather than invented screenshots.

`tools/ui_capture.py` reads `tests/parity/ui/screens.json`, creates a private game
copy per program and repetition, runs headless inside `unshare -rn`, and writes
800x600 lossless captures to ignored `work/parity/ui/<screen>/{reference,clone}.png`.
`after/` and `before/` retain each phase's scripts, scenario, source captures,
invocation, process log and report. Source image bytes and canonical PNG bytes
are checked independently across at least two process launches. A changed pixel
in a repeat is a deterministic-capture failure. Reports contain counts, MAE,
RMSE, maximum channel error and hashes; captures and detailed reports stay private.
The top-level PNGs describe the current phase; failed routes have no PNG.

Run `python3 tools/ui_capture.py --list` to list IDs. Run an individual entry with:

```sh
python3 tools/ui_capture.py --screen hud-mission-1 --phase before \
  --game /absolute/path/to/read-only/game \
  --runtime /absolute/path/to/rebuilt/aztec_recomp
# Apply a UI change, then repeat the same command with --phase after.
```

Use `--all` for the full inventory. Output defaults to `work/parity/ui`; override
`--output` only with another directory under this worktree's `work/parity/`.
A phase must be fresh. `before-after.json` records null for an unavailable metric.
An incomplete or differing comparison exits nonzero. Blocked specifications remain
in the report; they are not skipped silently. Routes accept move, held left-button
press/release, shared key press/release, and waits measured in frames. The shared
key subset is Escape, Return, Space and F1; more keys need explicit DIK mappings.
Coordinates are logical 800x600 pixels. See [EXPERIMENTS.md](EXPERIMENTS.md#frame-scripted-ui-catalogue)
for reference rebuilding and the exact hook contracts.

The baseline build, synthetic checks and pre-commit gates passed before changes.
A baseline private clone probe produced an 800x600 frame. Its reference counterpart
failed during namespace loopback setup with `Cannot open netlink socket: Operation
not permitted`, before launching the game. Therefore no baseline reference/clone
pixel difference was measurable. After implementation, 34 clone catalogue entries
produced byte-identical captures across two independent launches: **zero differing
pixels in every clone repeat**. The remaining 139 clone entries have explicit route
or feature blockers. There were 21 executable reference routes, each attempted twice
with the rebuilt runtime; all 42 attempts failed at the same mandatory namespace
setup step. The other 152 reference entries have explicit route blockers. No
reference frame was obtained, so no cross-program difference was measured before
or after. These failures must not be represented as zero error or a parity pass.

The table lists each ID, requested state/reach path and actual capture outcome.
`R` means repeatable clone capture with zero differing pixels between repeats;
`B` means an explicit unavailable route/feature; `F` means the reference launch
failed before execution. `N/A` means the reference/clone difference is unmeasured,
not zero. Mission selection entries share the clone campaign list because the clone
opens briefing on press instead of retaining a separate selected-row state.
Video options share its combined options page. Minimap/resource-bar entries capture
the full HUD, with no separately verified structural masks. Localized text comes
from the user's installation; it is never copied into public specifications.

L4 and L5 remain incomplete. Reference waits count guest presentations, which may
be partial redraws or stop while a menu is idle. The reference clock is not
virtualized, so reference repeatability and tick alignment are unverified. Clone
front-end waits count rendered frames; clone gameplay frames advance simulation
by one tick. Direct mission launches do not validate front-end-to-game transitions.
Neither equal frame indices nor byte-identical clone repeats establish matching
camera, selection, light, state sequence, structural masks or emitted commands.
Gameplay produces the existing raw clone UI trace, but no equivalent reference
semantic observer exists. The shared L0 normalization/observer dependency remains
open; `parity_visual.py` can compare aligned L4/L5 bundles once provided. Intro
frame stepping, loading observers, original dialog fixtures, lobby UI, held main
button visuals, per-type selection fixtures, and reproducible outcome routes are
also pending. This work does not implement those game screens.

| Screen/state ID | How to reach / state | Reference after | Clone after | Difference before | Difference after |
| --- | --- | --- | --- | --- | --- |
| `splash` | Start the program before profile selection; capture the splash transition. | B | B | N/A | N/A |
| `intro` | Start the program with video enabled; capture a specified decoded video frame. Video frame stepping is missing. | B | B | N/A | N/A |
| `profile` | Start reference, select a player and accept; clone has no player selector. | F | B | N/A | N/A |
| `main-menu` | Accept player selection; show the initial menu. | F | R | N/A | N/A |
| `main-campaign-normal` | Main menu, campaign button, normal state; original coordinates await observation. | B | R | N/A | N/A |
| `main-campaign-hover` | Main menu, campaign button, hover state; original coordinates await observation. | B | R | N/A | N/A |
| `main-campaign-pressed` | Main menu, campaign button, pressed state; original coordinates await observation. | B | B | N/A | N/A |
| `main-custom-map-normal` | Main menu, custom-map button, normal state; original coordinates await observation. | B | R | N/A | N/A |
| `main-custom-map-hover` | Main menu, custom-map button, hover state; original coordinates await observation. | B | R | N/A | N/A |
| `main-custom-map-pressed` | Main menu, custom-map button, pressed state; original coordinates await observation. | B | B | N/A | N/A |
| `main-options-normal` | Main menu, options button, normal state; original coordinates await observation. | B | R | N/A | N/A |
| `main-options-hover` | Main menu, options button, hover state; original coordinates await observation. | B | R | N/A | N/A |
| `main-options-pressed` | Main menu, options button, pressed state; original coordinates await observation. | B | B | N/A | N/A |
| `main-quit-normal` | Main menu, quit button, normal state; original coordinates await observation. | B | R | N/A | N/A |
| `main-quit-hover` | Main menu, quit button, hover state; original coordinates await observation. | B | R | N/A | N/A |
| `main-quit-pressed` | Main menu, quit button, pressed state; original coordinates await observation. | B | B | N/A | N/A |
| `main-new-game-normal` | Main menu, new-game button, normal state; original coordinates await observation. | B | B | N/A | N/A |
| `main-new-game-hover` | Main menu, new-game button, hover state; original coordinates await observation. | B | B | N/A | N/A |
| `main-new-game-pressed` | Main menu, new-game button, pressed state; original coordinates await observation. | B | B | N/A | N/A |
| `main-load-normal` | Main menu, load button, normal state; original coordinates await observation. | B | B | N/A | N/A |
| `main-load-hover` | Main menu, load button, hover state; original coordinates await observation. | B | B | N/A | N/A |
| `main-load-pressed` | Main menu, load button, pressed state; original coordinates await observation. | B | B | N/A | N/A |
| `main-multiplayer-normal` | Main menu, multiplayer button, normal state; original coordinates await observation. | B | B | N/A | N/A |
| `main-multiplayer-hover` | Main menu, multiplayer button, hover state; original coordinates await observation. | B | B | N/A | N/A |
| `main-multiplayer-pressed` | Main menu, multiplayer button, pressed state; original coordinates await observation. | B | B | N/A | N/A |
| `main-credits-normal` | Main menu, credits button, normal state; original coordinates await observation. | B | B | N/A | N/A |
| `main-credits-hover` | Main menu, credits button, hover state; original coordinates await observation. | B | B | N/A | N/A |
| `main-credits-pressed` | Main menu, credits button, pressed state; original coordinates await observation. | B | B | N/A | N/A |
| `new-game` | Main menu > New game. | B | B | N/A | N/A |
| `difficulty-easy` | New game > Difficulty > easy. | B | B | N/A | N/A |
| `difficulty-normal` | New game > Difficulty > normal. | B | B | N/A | N/A |
| `difficulty-hard` | New game > Difficulty > hard. | B | B | N/A | N/A |
| `campaign-selection` | Main menu > Campaign. | F | R | N/A | N/A |
| `mission-selection-1` | Campaign > select mission 1. Clone captures the campaign list; it opens briefing immediately on a mission press and has no separate selected-row state. | F | R | N/A | N/A |
| `briefing-1` | Campaign > mission 1 > Play; stop before Start. | F | R | N/A | N/A |
| `hud-mission-1` | Start mission 1; capture after four clone simulation frames. Reference timing is presentation-based and unaligned. | F | R | N/A | N/A |
| `mission-selection-2` | Campaign > select mission 2. Clone captures the campaign list; it opens briefing immediately on a mission press and has no separate selected-row state. | F | R | N/A | N/A |
| `briefing-2` | Campaign > mission 2 > Play; stop before Start. | F | R | N/A | N/A |
| `hud-mission-2` | Start mission 2; capture after four clone simulation frames. Reference timing is presentation-based and unaligned. | F | R | N/A | N/A |
| `mission-selection-3` | Campaign > select mission 3. Clone captures the campaign list; it opens briefing immediately on a mission press and has no separate selected-row state. | F | R | N/A | N/A |
| `briefing-3` | Campaign > mission 3 > Play; stop before Start. | F | R | N/A | N/A |
| `hud-mission-3` | Start mission 3; capture after four clone simulation frames. Reference timing is presentation-based and unaligned. | F | R | N/A | N/A |
| `mission-selection-4` | Campaign > select mission 4. Clone captures the campaign list; it opens briefing immediately on a mission press and has no separate selected-row state. | F | R | N/A | N/A |
| `briefing-4` | Campaign > mission 4 > Play; stop before Start. | F | R | N/A | N/A |
| `hud-mission-4` | Start mission 4; capture after four clone simulation frames. Reference timing is presentation-based and unaligned. | F | R | N/A | N/A |
| `mission-selection-5` | Campaign > select mission 5. Clone captures the campaign list; it opens briefing immediately on a mission press and has no separate selected-row state. | F | R | N/A | N/A |
| `briefing-5` | Campaign > mission 5 > Play; stop before Start. | F | R | N/A | N/A |
| `hud-mission-5` | Start mission 5; capture after four clone simulation frames. Reference timing is presentation-based and unaligned. | F | R | N/A | N/A |
| `custom-selection` | Main menu > Custom map. | B | R | N/A | N/A |
| `load-empty` | In-game pause menu > load; empty state. A reproducible slot fixture is needed. | B | B | N/A | N/A |
| `load-selected` | In-game pause menu > load; selected state. A reproducible slot fixture is needed. | B | B | N/A | N/A |
| `load-confirmation` | In-game pause menu > load; confirmation state. A reproducible slot fixture is needed. | B | B | N/A | N/A |
| `save-empty` | In-game pause menu > save; empty state. A reproducible slot fixture is needed. | B | B | N/A | N/A |
| `save-selected` | In-game pause menu > save; selected state. A reproducible slot fixture is needed. | B | B | N/A | N/A |
| `save-confirmation` | In-game pause menu > save; confirmation state. A reproducible slot fixture is needed. | B | B | N/A | N/A |
| `options` | Main menu > Options. | B | R | N/A | N/A |
| `options-video` | Options > Video; clone exposes display settings on its combined options page. | B | R | N/A | N/A |
| `options-audio` | Options > Audio. | B | R | N/A | N/A |
| `options-controls` | Options > Controls; full clone controls editor is missing. | B | B | N/A | N/A |
| `options-game` | Options > Game; clone game modifications panel. | B | R | N/A | N/A |
| `credits` | Main menu > Credits; clone credits screen is missing. | B | B | N/A | N/A |
| `multiplayer-connection` | Main menu > Multiplayer > connection; clone lobby front end is missing. | B | B | N/A | N/A |
| `multiplayer-host` | Main menu > Multiplayer > host; clone lobby front end is missing. | B | B | N/A | N/A |
| `multiplayer-join` | Main menu > Multiplayer > join; clone lobby front end is missing. | B | B | N/A | N/A |
| `multiplayer-players` | Main menu > Multiplayer > players; clone lobby front end is missing. | B | B | N/A | N/A |
| `multiplayer-ready` | Main menu > Multiplayer > ready; clone lobby front end is missing. | B | B | N/A | N/A |
| `loading-mission-1` | Start mission 1; intercept loading presentation. Clone loading observer is missing. | B | B | N/A | N/A |
| `loading-mission-2` | Start mission 2; intercept loading presentation. Clone loading observer is missing. | B | B | N/A | N/A |
| `loading-mission-3` | Start mission 3; intercept loading presentation. Clone loading observer is missing. | B | B | N/A | N/A |
| `loading-mission-4` | Start mission 4; intercept loading presentation. Clone loading observer is missing. | B | B | N/A | N/A |
| `loading-mission-5` | Start mission 5; intercept loading presentation. Clone loading observer is missing. | B | B | N/A | N/A |
| `command-card-footman` | Mission fixture containing own FOOTMAN; select it and capture its command card. Per-type placement and reference selection fixture are pending. | B | B | N/A | N/A |
| `command-card-rifleman` | Mission fixture containing own RIFLEMAN; select it and capture its command card. Per-type placement and reference selection fixture are pending. | B | B | N/A | N/A |
| `command-card-demolisher` | Mission fixture containing own DEMOLISHER; select it and capture its command card. Per-type placement and reference selection fixture are pending. | B | B | N/A | N/A |
| `command-card-truck-axethrower` | Mission fixture containing own TRUCK_AXETHROWER; select it and capture its command card. Per-type placement and reference selection fixture are pending. | B | B | N/A | N/A |
| `command-card-cannon` | Mission fixture containing own CANNON; select it and capture its command card. Per-type placement and reference selection fixture are pending. | B | B | N/A | N/A |
| `command-card-bear` | Mission fixture containing own BEAR; select it and capture its command card. Per-type placement and reference selection fixture are pending. | B | B | N/A | N/A |
| `command-card-knight` | Mission fixture containing own KNIGHT; select it and capture its command card. Per-type placement and reference selection fixture are pending. | B | B | N/A | N/A |
| `command-card-zeppelin` | Mission fixture containing own ZEPPELIN; select it and capture its command card. Per-type placement and reference selection fixture are pending. | B | B | N/A | N/A |
| `command-card-truck-flamethrower` | Mission fixture containing own TRUCK_FLAMETHROWER; select it and capture its command card. Per-type placement and reference selection fixture are pending. | B | B | N/A | N/A |
| `command-card-truck-katusha` | Mission fixture containing own TRUCK_KATUSHA; select it and capture its command card. Per-type placement and reference selection fixture are pending. | B | B | N/A | N/A |
| `command-card-rus-forpost` | Mission fixture containing own RUS_FORPOST; select it and capture its command card. Per-type placement and reference selection fixture are pending. | B | B | N/A | N/A |
| `command-card-rus-village` | Mission fixture containing own RUS_VILLAGE; select it and capture its command card. Per-type placement and reference selection fixture are pending. | B | B | N/A | N/A |
| `command-card-rus-farm` | Mission fixture containing own RUS_FARM; select it and capture its command card. Per-type placement and reference selection fixture are pending. | B | B | N/A | N/A |
| `command-card-rus-mine` | Mission fixture containing own RUS_MINE; select it and capture its command card. Per-type placement and reference selection fixture are pending. | B | B | N/A | N/A |
| `command-card-rus-barracks` | Mission fixture containing own RUS_BARRACKS; select it and capture its command card. Per-type placement and reference selection fixture are pending. | B | B | N/A | N/A |
| `command-card-rus-stable` | Mission fixture containing own RUS_STABLE; select it and capture its command card. Per-type placement and reference selection fixture are pending. | B | B | N/A | N/A |
| `command-card-rus-bearlair` | Mission fixture containing own RUS_BEARLAIR; select it and capture its command card. Per-type placement and reference selection fixture are pending. | B | B | N/A | N/A |
| `command-card-rus-watch-tower` | Mission fixture containing own RUS_WATCH_TOWER; select it and capture its command card. Per-type placement and reference selection fixture are pending. | B | B | N/A | N/A |
| `command-card-rus-warehouse` | Mission fixture containing own RUS_WAREHOUSE; select it and capture its command card. Per-type placement and reference selection fixture are pending. | B | B | N/A | N/A |
| `command-card-rus-town` | Mission fixture containing own RUS_TOWN; select it and capture its command card. Per-type placement and reference selection fixture are pending. | B | B | N/A | N/A |
| `command-card-rus-workshop` | Mission fixture containing own RUS_WORKSHOP; select it and capture its command card. Per-type placement and reference selection fixture are pending. | B | B | N/A | N/A |
| `command-card-rus-armory` | Mission fixture containing own RUS_ARMORY; select it and capture its command card. Per-type placement and reference selection fixture are pending. | B | B | N/A | N/A |
| `command-card-rus-labs` | Mission fixture containing own RUS_LABS; select it and capture its command card. Per-type placement and reference selection fixture are pending. | B | B | N/A | N/A |
| `command-card-rus-fortress-tower` | Mission fixture containing own RUS_FORTRESS_TOWER; select it and capture its command card. Per-type placement and reference selection fixture are pending. | B | B | N/A | N/A |
| `command-card-rus-fortress` | Mission fixture containing own RUS_FORTRESS; select it and capture its command card. Per-type placement and reference selection fixture are pending. | B | B | N/A | N/A |
| `command-card-rus-air-facility` | Mission fixture containing own RUS_AIR_FACILITY; select it and capture its command card. Per-type placement and reference selection fixture are pending. | B | B | N/A | N/A |
| `command-card-rus-cannon-tower` | Mission fixture containing own RUS_CANNON_TOWER; select it and capture its command card. Per-type placement and reference selection fixture are pending. | B | B | N/A | N/A |
| `command-card-archer` | Mission fixture containing own ARCHER; select it and capture its command card. Per-type placement and reference selection fixture are pending. | B | B | N/A | N/A |
| `command-card-mage` | Mission fixture containing own MAGE; select it and capture its command card. Per-type placement and reference selection fixture are pending. | B | B | N/A | N/A |
| `command-card-tuzem` | Mission fixture containing own TUZEM; select it and capture its command card. Per-type placement and reference selection fixture are pending. | B | B | N/A | N/A |
| `command-card-spider-crossbow` | Mission fixture containing own SPIDER_CROSSBOW; select it and capture its command card. Per-type placement and reference selection fixture are pending. | B | B | N/A | N/A |
| `command-card-jaguar` | Mission fixture containing own JAGUAR; select it and capture its command card. Per-type placement and reference selection fixture are pending. | B | B | N/A | N/A |
| `command-card-bizon` | Mission fixture containing own BIZON; select it and capture its command card. Per-type placement and reference selection fixture are pending. | B | B | N/A | N/A |
| `command-card-eagle` | Mission fixture containing own EAGLE; select it and capture its command card. Per-type placement and reference selection fixture are pending. | B | B | N/A | N/A |
| `command-card-deltaplan` | Mission fixture containing own DELTAPLAN; select it and capture its command card. Per-type placement and reference selection fixture are pending. | B | B | N/A | N/A |
| `command-card-spider-flamethrower` | Mission fixture containing own SPIDER_FLAMETHROWER; select it and capture its command card. Per-type placement and reference selection fixture are pending. | B | B | N/A | N/A |
| `command-card-spider-axethrower` | Mission fixture containing own SPIDER_AXETHROWER; select it and capture its command card. Per-type placement and reference selection fixture are pending. | B | B | N/A | N/A |
| `command-card-azt-forpost` | Mission fixture containing own AZT_FORPOST; select it and capture its command card. Per-type placement and reference selection fixture are pending. | B | B | N/A | N/A |
| `command-card-azt-village` | Mission fixture containing own AZT_VILLAGE; select it and capture its command card. Per-type placement and reference selection fixture are pending. | B | B | N/A | N/A |
| `command-card-azt-farm` | Mission fixture containing own AZT_FARM; select it and capture its command card. Per-type placement and reference selection fixture are pending. | B | B | N/A | N/A |
| `command-card-azt-warehouse` | Mission fixture containing own AZT_WAREHOUSE; select it and capture its command card. Per-type placement and reference selection fixture are pending. | B | B | N/A | N/A |
| `command-card-azt-barracks` | Mission fixture containing own AZT_BARRACKS; select it and capture its command card. Per-type placement and reference selection fixture are pending. | B | B | N/A | N/A |
| `command-card-azt-archer-tower` | Mission fixture containing own AZT_ARCHER_TOWER; select it and capture its command card. Per-type placement and reference selection fixture are pending. | B | B | N/A | N/A |
| `command-card-azt-hut` | Mission fixture containing own AZT_HUT; select it and capture its command card. Per-type placement and reference selection fixture are pending. | B | B | N/A | N/A |
| `command-card-azt-stable` | Mission fixture containing own AZT_STABLE; select it and capture its command card. Per-type placement and reference selection fixture are pending. | B | B | N/A | N/A |
| `command-card-azt-jaguarlair` | Mission fixture containing own AZT_JAGUARLAIR; select it and capture its command card. Per-type placement and reference selection fixture are pending. | B | B | N/A | N/A |
| `command-card-azt-nest` | Mission fixture containing own AZT_NEST; select it and capture its command card. Per-type placement and reference selection fixture are pending. | B | B | N/A | N/A |
| `command-card-azt-town` | Mission fixture containing own AZT_TOWN; select it and capture its command card. Per-type placement and reference selection fixture are pending. | B | B | N/A | N/A |
| `command-card-azt-mine` | Mission fixture containing own AZT_MINE; select it and capture its command card. Per-type placement and reference selection fixture are pending. | B | B | N/A | N/A |
| `command-card-azt-crossbow-tower` | Mission fixture containing own AZT_CROSSBOW_TOWER; select it and capture its command card. Per-type placement and reference selection fixture are pending. | B | B | N/A | N/A |
| `command-card-azt-temple` | Mission fixture containing own AZT_TEMPLE; select it and capture its command card. Per-type placement and reference selection fixture are pending. | B | B | N/A | N/A |
| `command-card-azt-workshop` | Mission fixture containing own AZT_WORKSHOP; select it and capture its command card. Per-type placement and reference selection fixture are pending. | B | B | N/A | N/A |
| `command-card-azt-air-facility` | Mission fixture containing own AZT_AIR_FACILITY; select it and capture its command card. Per-type placement and reference selection fixture are pending. | B | B | N/A | N/A |
| `command-card-ninja` | Mission fixture containing own NINJA; select it and capture its command card. Per-type placement and reference selection fixture are pending. | B | B | N/A | N/A |
| `command-card-hypno` | Mission fixture containing own HYPNO; select it and capture its command card. Per-type placement and reference selection fixture are pending. | B | B | N/A | N/A |
| `command-card-chief` | Mission fixture containing own CHIEF; select it and capture its command card. Per-type placement and reference selection fixture are pending. | B | B | N/A | N/A |
| `command-card-snowman` | Mission fixture containing own SNOWMAN; select it and capture its command card. Per-type placement and reference selection fixture are pending. | B | B | N/A | N/A |
| `command-card-elefant` | Mission fixture containing own ELEFANT; select it and capture its command card. Per-type placement and reference selection fixture are pending. | B | B | N/A | N/A |
| `command-card-wheel-katusha` | Mission fixture containing own WHEEL_KATUSHA; select it and capture its command card. Per-type placement and reference selection fixture are pending. | B | B | N/A | N/A |
| `command-card-wheel-firethrower` | Mission fixture containing own WHEEL_FIRETHROWER; select it and capture its command card. Per-type placement and reference selection fixture are pending. | B | B | N/A | N/A |
| `command-card-paraplan` | Mission fixture containing own PARAPLAN; select it and capture its command card. Per-type placement and reference selection fixture are pending. | B | B | N/A | N/A |
| `command-card-wheel-axethrower` | Mission fixture containing own WHEEL_AXETHROWER; select it and capture its command card. Per-type placement and reference selection fixture are pending. | B | B | N/A | N/A |
| `command-card-wheel-crossbow` | Mission fixture containing own WHEEL_CROSSBOW; select it and capture its command card. Per-type placement and reference selection fixture are pending. | B | B | N/A | N/A |
| `command-card-chi-forpost` | Mission fixture containing own CHI_FORPOST; select it and capture its command card. Per-type placement and reference selection fixture are pending. | B | B | N/A | N/A |
| `command-card-chi-village` | Mission fixture containing own CHI_VILLAGE; select it and capture its command card. Per-type placement and reference selection fixture are pending. | B | B | N/A | N/A |
| `command-card-chi-farm` | Mission fixture containing own CHI_FARM; select it and capture its command card. Per-type placement and reference selection fixture are pending. | B | B | N/A | N/A |
| `command-card-chi-mine` | Mission fixture containing own CHI_MINE; select it and capture its command card. Per-type placement and reference selection fixture are pending. | B | B | N/A | N/A |
| `command-card-chi-barracks` | Mission fixture containing own CHI_BARRACKS; select it and capture its command card. Per-type placement and reference selection fixture are pending. | B | B | N/A | N/A |
| `command-card-chi-firethrow-tower` | Mission fixture containing own CHI_FIRETHROW_TOWER; select it and capture its command card. Per-type placement and reference selection fixture are pending. | B | B | N/A | N/A |
| `command-card-chi-hypno-tower` | Mission fixture containing own CHI_HYPNO_TOWER; select it and capture its command card. Per-type placement and reference selection fixture are pending. | B | B | N/A | N/A |
| `command-card-chi-warehouse` | Mission fixture containing own CHI_WAREHOUSE; select it and capture its command card. Per-type placement and reference selection fixture are pending. | B | B | N/A | N/A |
| `command-card-chi-town` | Mission fixture containing own CHI_TOWN; select it and capture its command card. Per-type placement and reference selection fixture are pending. | B | B | N/A | N/A |
| `command-card-chi-hypno-school` | Mission fixture containing own CHI_HYPNO_SCHOOL; select it and capture its command card. Per-type placement and reference selection fixture are pending. | B | B | N/A | N/A |
| `command-card-chi-prison` | Mission fixture containing own CHI_PRISON; select it and capture its command card. Per-type placement and reference selection fixture are pending. | B | B | N/A | N/A |
| `command-card-chi-workshop` | Mission fixture containing own CHI_WORKSHOP; select it and capture its command card. Per-type placement and reference selection fixture are pending. | B | B | N/A | N/A |
| `command-card-chi-armory` | Mission fixture containing own CHI_ARMORY; select it and capture its command card. Per-type placement and reference selection fixture are pending. | B | B | N/A | N/A |
| `command-card-chi-fortress` | Mission fixture containing own CHI_FORTRESS; select it and capture its command card. Per-type placement and reference selection fixture are pending. | B | B | N/A | N/A |
| `command-card-chi-stable` | Mission fixture containing own CHI_STABLE; select it and capture its command card. Per-type placement and reference selection fixture are pending. | B | B | N/A | N/A |
| `command-card-chi-cave` | Mission fixture containing own CHI_CAVE; select it and capture its command card. Per-type placement and reference selection fixture are pending. | B | B | N/A | N/A |
| `command-card-chi-air-facility` | Mission fixture containing own CHI_AIR_FACILITY; select it and capture its command card. Per-type placement and reference selection fixture are pending. | B | B | N/A | N/A |
| `command-card-chi-watchtower` | Mission fixture containing own CHI_WATCHTOWER; select it and capture its command card. Per-type placement and reference selection fixture are pending. | B | B | N/A | N/A |
| `command-card-chi-airtower` | Mission fixture containing own CHI_AIRTOWER; select it and capture its command card. Per-type placement and reference selection fixture are pending. | B | B | N/A | N/A |
| `command-card-abdula` | Mission fixture containing own ABDULA; select it and capture its command card. Per-type placement and reference selection fixture are pending. | B | B | N/A | N/A |
| `command-card-catapult` | Mission fixture containing own CATAPULT; select it and capture its command card. Per-type placement and reference selection fixture are pending. | B | B | N/A | N/A |
| `command-card-boat` | Mission fixture containing own BOAT; select it and capture its command card. Per-type placement and reference selection fixture are pending. | B | B | N/A | N/A |
| `command-card-zeppelin2` | Mission fixture containing own ZEPPELIN2; select it and capture its command card. Per-type placement and reference selection fixture are pending. | B | B | N/A | N/A |
| `command-card-shipyard` | Mission fixture containing own SHIPYARD; select it and capture its command card. Per-type placement and reference selection fixture are pending. | B | B | N/A | N/A |
| `command-card-hopcannon` | Mission fixture containing own HOPCANNON; select it and capture its command card. Per-type placement and reference selection fixture are pending. | B | B | N/A | N/A |
| `command-card-johnka` | Mission fixture containing own JOHNKA; select it and capture its command card. Per-type placement and reference selection fixture are pending. | B | B | N/A | N/A |
| `command-card-camera` | Mission fixture containing own CAMERA; select it and capture its command card. Per-type placement and reference selection fixture are pending. | B | B | N/A | N/A |
| `build-menu` | Select a town and open its construction menu. | B | B | N/A | N/A |
| `placement-mode` | Choose a building and move to a legal map location. | B | B | N/A | N/A |
| `selection-panel` | Select an own unit; inspect selection details. | B | B | N/A | N/A |
| `minimap` | Start mission 1; inspect minimap. | F | R | N/A | N/A |
| `resource-bar` | Start mission 1; inspect resources. | F | R | N/A | N/A |
| `messages` | Start mission 1 and trigger a specified message at a specified tick. | B | B | N/A | N/A |
| `pause-menu` | Start mission 1 > Escape. | F | R | N/A | N/A |
| `objectives` | Start mission 1 > Objectives. | B | R | N/A | N/A |
| `mission-result-victory` | Complete a mission with a deterministic winning command replay. | B | B | N/A | N/A |
| `mission-result-defeat` | Lose a mission with a deterministic losing command replay. | B | B | N/A | N/A |
| `victory` | Finish the campaign; show final victory. | B | B | N/A | N/A |
| `defeat` | Lose the campaign mission; show defeat. | B | B | N/A | N/A |

## Generated custom missions

Catalogue entries `ztest-all` and `ztest-battle` cover the generated custom-map
showcase and combat setup. Generate them in a private game copy with
`tools/make_testmaps.py`; see [EXPERIMENTS.md](EXPERIMENTS.md#generated-showcase-and-battle-maps)
for direct captures and cleanup. Race/category labels are mission notes and
private layout coordinates; terrain remains a runtime copy of campaign terrain.
The catalogue runner currently only accepts numbered campaign missions, so both
custom entries retain explicit blockers. Direct clone captures exercise loading,
not catalogue repeatability or reference pixel parity. The reference launch is
blocked by namespace loopback permissions. Construction, selection, queues and
burning/death input recipes and remaining adapter dependencies are recorded in
[the provisional scenario](../tests/parity/testmaps.json).

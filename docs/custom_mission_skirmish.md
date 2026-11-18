# Custom mission (skirmish) mode — task 125

This implements an independent single-player custom-map flow. Original-game
behavior has only static evidence here; native Windows comparison remains
**needs live run**. V means a fact inspected in private original material, not a
live parity result. G means an assumption or clone policy. No original process
was launched and no private game file, script body, capture or lifted code is
included in this change.

## Reference findings

| Evidence | Finding | Read-only research location |
| --- | --- | --- |
| V | The map browser enumerates `Maps/*.mp`, reads `[Globals] Name` and `MapSize`, and sorts the displayed names using byte comparisons. The list entry retains its source path separately from its title. | `f_0043f620`, `gen_071.c`; its INI key and search-path strings were checked in the local installation. |
| V | `MaxPlayers` is read in a separate browser-mode branch; it is not a count of all map sides. Neutral map sections also exist. | `f_0043f620`, `f_0042eae0`, installed custom `.mp` files. |
| V | Side initialization reads `Sides`, then each section's race, colour, starting cash, neutral status, disabled types, cameras and defences. `StartMoney` defaults to 100. The selected side's `Start_X` and `Start_Y` drive the initial camera. | `f_0042ffb0`, `gen_064.c`; `f_004306d0`; installed `.mp` files. |
| V | Starting cash is passed to the side initializer. The initializer retains that amount and initializes a separate accounting value from half the amount; that second value is not evidence of halving the player's cash. | `f_004498f0` and the call at `0043014f`. |
| V | Maps can specify different cash and colours for different sides. Neutral sections can have disabled starts. Placed towns have their own race/type/owner and preplaced objects. | Installed custom `.mp` files; `f_00430770`. No installation values are embedded in the tests. |
| V | `OnlyComp` is not read in the inspected side-loading path. The simulation's computer predicate consults a runtime player table, independently of `.mp` editor flags. The side-update path gates autonomous strategy on that predicate. | `f_0042ffb0`, `f_0043d2c0` in `gen_070.c`, `f_00449980`; see also [rules_ai.md](rules_ai.md). |
| V | The ordinary mission loader loads placed map entities and the sibling scenario script. Inspected custom scripts express defeat after loss of local towns and units, and victory after no enemies remain. These are script conditions, not proof that every custom map uses one universal victory rule. | `f_0042ec90`, `gen_063.c`, and private custom `.scn` files. |

The runtime player-table construction, default human slot, side-selection
controls, difficulty/handicap effects and exact first-update state are not fully
established by these findings. Static constructor reads alone cannot establish
what the front-end or startup scripts change before the player sees the game.

## Implemented flow and explicit assumptions

- G: Main menu → Custom map lists parsed display names and map sizes. Canonical
  campaign entries stay in the campaign browser. Malformed descriptors and
  directories with an `.mp` suffix are omitted. Duplicate names retain separate
  source filenames, which break sorting ties. Empty titles fall back to filenames.
  Sorting UTF-8 titles is provisional for non-ASCII installation names.
- G: Selecting a row loads the map and shows a setup summary. The first
  non-neutral side with an in-bounds start is human; every other side, including
  neutral sides, uses the existing computer planner. Editor `OnlyComp` does not
  select the human. This is a deterministic default, not a measured original
  slot-selection rule. There is currently no human-slot, race or colour editor.
- V/G: Map colours, side cash, race, disabled types, placements and initial
  diplomacy are retained through the existing loader. No blanket cash grant,
  random race, colour reassignment, new starting town or extra army is invented.
  The existing neutral-diplomacy implementation and all planner/economy behavior
  retain their documented engine limitations.
- G: Setup displays human/AI/neutral ownership, race, colour index and starting
  cash. Start is disabled for invalid setup, missing terrain or a missing/invalid
  script. Back returns to the list. A loading frame precedes play. A load failure
  after confirmation returns to the main menu with an explanation.
- V/G: Play attaches the map's `.scn` through the existing script VM and enables
  the existing AI for computer towns. The map script determines the result;
  there is no implicit elimination override. Existing unsupported script actions
  remain unsupported. Townless sides have no town planner; no new standalone
  army strategy is claimed.
- G: Victory/defeat use the existing result overlay. Retry reconstructs the
  mission with skirmish ownership; returning leaves campaign progress unchanged.
  Skirmishes cannot advance to a campaign successor, even when explicitly
  launched against a campaign filename. Simulation updates stop at the result
  through the existing mission driver.
- G: The custom browser uses the current front-end journal layout, with nine
  entries per page, previous/next buttons and mouse-wheel paging. It retains map
  dimensions and opens skirmish setup instead of campaign briefing. Setup uses
  an 800×600 logical canvas: title at `(400,22)`, side rows from `(90,150)` at
  34-pixel spacing, Back at `(90,535,150,36)` and Start at `(550,535,170,36)`.
  These setup rectangles are clone layout choices, not V geometry. Art, font,
  sound, transition and pixel parity remain unmeasured.

The explicit launch flag prevents this policy from changing ordinary campaign
launches, map inspectors, generated reference probes or existing scenario tests:

```sh
build/aztec_engine --game /path/to/private/game --skirmish "Custom map basename"
build/aztec_engine --headless --game /path/to/private/game \
  --rules-dir docs/data --skirmish --no-auto-orders --max-ticks 14400 \
  --report work/custom-report.json "Custom map basename"
```

These are invocation examples for an isolated private installation. On Linux,
wrap real-data engine launches in the private-copy, dummy-SDL, timeout and
`unshare -rn` procedure in [EXPERIMENTS.md](EXPERIMENTS.md). No isolation bypass
was used here. Synthetic CTest fixtures do not read an installation.

### Save continuity

New AZS3 writes use format version 5, with replay version unchanged. The high bit
of the existing settings word records skirmish mode; the other bits retain the
modification settings. The version-5 viewing side and local selection fields are retained. Version 4
containers with a compatible replay version remain readable as ordinary launches,
without local UI fields, and must not use the new bit. Readers predating skirmish
support reject the unknown settings bit rather than silently changing ownership. Other versions and unknown flags remain rejected.
The existing [engine save guide](engine_saves.md) predates this format addition.

Replay applies custom setup to a fresh private world before initialization and
checks the resulting state hash. Changing/removing the mode fails hash validation.
Normal-mode state hashes are preserved; custom mode adds a distinct hash tag.
Map filenames with spaces, apostrophes, parentheses and other basename characters
are accepted. Separators, drive prefixes, control characters and traversal
basenames remain rejected. Original saves cannot be converted to skirmishes.

### Task 109 coordination

The custom browser now follows the journal layout in
[frontend_screens.md](frontend_screens.md). It preserves task 109's button
semantics, pagination, campaign navigation, credits and controls. The new
`custom-setup` and `loading` screens retain the provisional geometry above;
their native measurements remain pending.

## Synthetic verification

`custom_mission_skirmish` generates a small invented map, empty resource archives,
three sides with a neutral slot before the human, unequal cash/colours, two towns
and independently authored scripts. Nothing is copied from installed maps.
It checks metadata sorting/filtering, filename fallback, setup validation,
the neutral all-enemies sentinel, preservation of map setup, AI planner advancement, scripted
elimination with neutral survivors, alternate timed victory/defeat, mode-aware
save/replay and continuation, headless launch/repetition/resume, actual menu
selection/loading, result/return/retry and disabled Start after a missing map.
Menu automation returns a concrete selection, which the test passes to the
mission driver; the existing CLI `--menu-script` intentionally stops at selection.
These checks are internal consistency, not original-game parity.

`engine_save` also checks version-4 compatibility and rejection of its undefined
mode bit, malformed input, transactional reads and current-format round trips.
All outputs are temporary invented fixtures. No private-data test was claimed.

Verification in this worktree: Release configure/build passed; the full CTest
suite reported 64 passed and six skipped (five private-data checks and the
restricted UDP process check). After the final cached campaign-successor fix,
the build and all six affected custom/campaign/save checks passed again.
Pre-commit passed for all tracked files and explicitly for the new files.
The cached hook environment worked; initializing a fresh hook environment was
blocked by unavailable GitHub DNS. No reference or native Windows execution was
performed. Changes remain uncommitted because git metadata is read-only.

## Native Windows run sheet — needs live run

The maintainer must collect two independent original runs for each case in a
fresh private installation/profile copy. Preserve the unmodified source
installation. Retain captures, saves, map/profile hashes, inputs and observations
privately, outside tracked paths. Record the exact original revision, language,
resolution, difficulty, handicap and initial profile. Run matching clone cases
with the same map and rules revision; do not equate clone hashes with original
hashes. No native Windows results are available from this task.

| Case | Required original observation, repeated twice | Clone comparison / acceptance |
| --- | --- | --- |
| Browser | Name/size columns, title language, wildcard/filter behavior, campaign/tutorial inclusion, ordering of duplicate/mixed-case/non-ASCII names, visible rows, scrolling and empty-list behavior. | Compare every listed identity and size; resolve the G campaign filter and UTF-8 sort. Record missing/extra rows separately from ordering errors. |
| Entry and confirmation | Exact input route, screen order, button rectangles/assets/fonts, hover/press/release behavior, sounds and time to loading/play. | Compare list, setup and loading captures at aligned boundaries. Replace G rectangles only after both original captures agree. |
| Human and colour | Default human side on a map whose first section is neutral, available side/race/colour controls, disabled starts, duplicate-colour handling and local camera. | Confirm or replace the G first-enabled-side rule. If the original offers selection, implement and persist that measured setup before declaring parity. |
| AI | Runtime human/computer membership including neutral sides, initial planner activation, difficulty/handicap, first build and first autonomous attack. Include a map with multiple non-neutral `OnlyComp=0` sections. | Compare actual membership and accepted commands at aligned ticks. Existing AI determinism does not verify composition, timing or townless-army behavior. |
| Resources | Cash before the first update and after startup scripts for each side; initial town types/races, units, technology, build restrictions and modifications. | Distinguish descriptor values from script grants and first income/spending. Resolve any front-end bonuses before changing engine initialization. |
| Victory | Preserve the map's actual script; destroy the last hostile town while an enemy unit survives, then the last enemy unit. Keep neutral objects alive. Repeat on a map with a different objective. | Check which script condition ends play, exact terminal tick, result screen/video/sound and whether further commands advance simulation. Do not replace special objectives with universal elimination. |
| Defeat and retry | Lose the last town while a local unit survives, then the last local unit; retry and inspect side setup/cash/camera/script reset. | Compare terminal predicate and tick, result actions and fresh setup after retry. |
| Save and return | Save mid-skirmish, exit, load, finish and return. Inspect campaign profile/unlocks before and after. | Check AI ownership, colours, resources, objectives and continuation; ensure no campaign advancement. Native original-save interoperability remains a separate incomplete feature. |
| Invalid input | Missing terrain/script, malformed descriptor, all-neutral/disabled sides and unsupported scripts. | Record actual original behavior; current clone refusal is a G policy. |

Return a private case manifest with both original observations and both clone
observations, observed screen order, aligned starting/terminal ticks and the
first repeatable divergence. A missing capture, timeout, failed isolation or
unsupported observer is **needs live run**, never an agreement. This run sheet
does not authorize bypassing the repository's namespace restrictions.

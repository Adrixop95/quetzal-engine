# Sight and fog validation (2026-10-28)

**V inspected facts / independent implementation:** sight is a light-mask ID,
not a radius in pixels. The simulation loads these existing descriptor fields:

| Family | Descriptor field | Mask IDs in the supplied metadata |
|---|---|---|
| Marine | d6 | 64..69, 71 |
| Spider | d8 | 70 |
| Building | d55 | 69..71 |
| Zeppelin | d7 | 71 |
| Truck | d6 | 66 |
| Bird | d8 | 69..71 |
| Headtruck | d8 | 67, 69, 70 |
| Defence | d58 | 68, 70, 71 |
| Cannon | d6 | 70 |
| Boat / Johnka | fixed | 71 |

Each type selects its own descriptor row; no building-wide eight-tile default
is used when lighting resources are loaded. Masks are user resources, never embedded bytes. The simulation uses
48-pixel spacing and a width+1 by height+2 grid. Mask translation has a 19-cell
extent; aligned stamps select the next interior vertex. Fractional stamps
truncate each weighted contribution separately along each axis. Target-position
sampling retains 8.8 fractions and requires brightness strictly greater than 32.
Ambient brightness uses the next daylight key's integer level times eight,
changing at key boundaries; it does not use the renderer's interpolated tint or
its extra draw shade. Glow adds up to 152; side vision takes the minimum darkness.
Exploration retains up to 32 even when current illumination drops below it.

**Inputs and scope:** `RulesData::load_sight(game_directory)` reads the user's
light archive, animation frames and daylight keys. `load_dir` also calls it when
`AZTEC_GAME` is set. Set that variable to the same directory as `--game` when
running this implementation. Missing lighting inputs leave fog untouched and
retain the bounded six-tile automatic-acquisition fallback so asset-free
simulations can still fight; that fallback remains P. Explicit attack commands
retain their existing weapon behavior. Synthetic inputs can be supplied through `set_sight`.
With lighting inputs loaded, no substitute radius or invented mask is used.
The 2026-11-04 consumer integration below supersedes the earlier renderer gap.

**P remaining:** acquisition still uses the engine's nearest-target policy and
four-tick scan rather than the original tactic scan bounds, bounded candidate
arrays, kill-time selection and pursuit rules. Aircraft height/transport state,
exact construction light accessors, composite/secondary glow producers and
reference tick alignment remain unverified. Task 01's normalized observer and
packet/object mapping are still prerequisites for a differential comparison.
These changes do not establish complete target-acquisition parity.

**Synthetic checks:** representative descriptor rows across all nine families,
aligned and half-cell mask translation, clipping, saturation, missing masks,
strict 32/33 and fractional target thresholds, side isolation, every-tick updates,
night exploration retention, death removing vision, targets beyond the old
six-tile radius, state-hash sensitivity, viewing-side independence and per-tick
command replay are covered without game assets. Resource-loading checks use the
user's archives only when the game-data test is explicitly enabled.

**M evidence:** none added. The supplied reference runtime was attempted with a
private symlinked game copy, dummy SDL drivers and `unshare -rn`. Network namespace
setup failed at loopback configuration with a denied netlink socket, before the
runtime launched. Record/replay and day/night acquisition measurements could not
run; no fallback outside isolation was used. Private logs stay in `work/sight/`.


**Validation:** Release configure/build and `test_engine --pure` pass. Full CTest
with the supplied external game installation passes 15 of 16 checks, including
all three game-data checks. The sole failure is `lockstep_processes`: UDP socket
creation is denied by the sandbox, as in the baseline. Pre-commit passes for
all tracked files and the new simulation header. An untouched baseline built
inside this worktree also passes the external engine test. The script timer test
uses resource-free rules and inspects the trigger immediately at tick 1401,
instead of depending on combat outcomes during a later 100-tick interval.


**Landing regression correction:** the newer develop AI skirmish runs without
private game data. Requiring lighting inputs for every automatic acquisition
silently disabled its combat and eliminated casualties. The resource-loaded
mask/brightness path is unchanged; missing inputs now retain the former bounded
six-tile acquisition fallback, explicitly P. Synthetic checks cover acquisition
inside that fallback, rejection beyond it, and fog remaining untouched without
resources. The 32/33 threshold and beyond-six-tile tests continue to cover the
resource-loaded path. The skirmish casualty assertion is retained.


**Landing validation:** a clean archive of `origin/develop` at `e81fabf` plus the
original patch reproduced the reported zero-casualty skirmish failure and hash.
With this correction applied, Release builds and the unchanged `ai_unit` and
14,400-tick `ai_skirmish` tests pass, including casualties, viewing-side hash
agreement and command replay. The full 18-test run has 14 passes, three missing
private-data skips and the existing sandbox UDP failure in `lockstep_processes`.
The worktree's 16-test suite with external game data still has 15 passes and that
same UDP failure. Pre-commit passes; no live oracle measurements were added.


**Landing helper correction:** a failed attempt left the task branch in place.
The next harvest used `checkout -b`, ignored its failure and reported a prepared
branch even though no patch reached the detached landing checkout. The helper
now checks every required Git command, refreshes an existing branch only when
its commits are already integrated, and rejects an empty staged result before
reporting success. Seven portable tooling checks pass; an isolated synthetic Git
experiment also verifies repeated existing-branch retries, inclusion of new
files and preservation of unmerged commits. Pre-commit and the Release build
pass; the worktree CTest result retains only the known sandbox UDP failure.
The active external runner must receive the corrected helper before retrying.


**Latest-develop validation:** the patch also builds against a clean archive of
`origin/develop` at `bc4d12c`. Its full CTest run passes with 15 checks passed and
four skipped: three missing private-data checks and the unavailable UDP process
check. Both AI tests, including the complete skirmish, pass. Seven tooling tests
and pre-commit pass. Detailed sight notes now live in this document, with links
from the three status rows, avoiding a conflict with newer status-document
appendices. The external runner still needs the helper fix before harvesting;
all task changes remain uncommitted in the read-only-metadata worktree.


## V06 consumer integration (2026-11-04, needs live run)

**V construction / synthetic evidence:** `World::visibility_brightness` separates
simulation raw brightness from loaded `.vis` darkness. Legacy tile layers are
converted when the simulation expands them to width+1 by height+2. The convention
participates in state hashing. `position_visible` keeps fractional brightness
through the strict >32 test, reads border neighbours and rejects positions outside
the playable map. Raw missing/malformed layers fail closed; asset-free legacy UI
and the six-tile acquisition fallback retain their earlier policies.

Unit/building drawing, effect eligibility, hover/picking, minimap markers,
automatic acquisition, new explicit target packets and AI town/tactic intelligence
use that common raw query. Rendering uses simulation positions for eligibility,
so interpolation cannot reveal a hidden target early. Owned tactic-number
selection stays independent of fog and excludes other sides with the same number.
Existing pursuit orders are unchanged; pursuit loss rules remain P.

The renderer shades terrain and sprites from the simulation's side layer when
fog is enabled. It no longer evolves separate exploration at render frequency.
The fallback render grid shares the simulation's stamp origin, 19-cell extent,
clipping and per-axis integer truncation. Fog accepts border grids, reverses raw
brightness into overlay opacity, and maps vertices to texel centres without
stretching border rows across the map. Its minimap texture refreshes at version
zero and on viewing-side, dimension and convention changes.

`render_contracts` checks asymmetric/fractional stamps, every border vertex,
32/33/fractional eligibility, map bounds, malformed input, AI intelligence and
picking. `animation_headless` renders invented sprites and reads software-SDL
pixels for raw minimap opacity, checks unit/building visibility, side changes and
texture resizing. `engine_unit` covers acquisition, direct target packets, owned
tactic selection, exploration after death and viewing-side replay hashes.

**P / needs live run:** no new reference evidence or synchronized visual result
exists. `tests/parity/fog.json` records the provisional scenario and blockers;
[EXPERIMENTS.md](EXPERIMENTS.md#v06-sightfog-consumer-run-sheet-needs-live-run)
contains exact private-copy commands. V02/V10 observation/input adapters remain
prerequisites for aligned comparison. Tactic bounds/scans/assignment, glow producer
completeness and missing-resource policies remain unresolved. Synthetic checks
establish internal consistency, not original-game parity.

Validation on 2026-11-05: Release configure/build passed in this worktree's
`build/`. CTest passed all 40 runnable checks; five private-data checks and the
restricted UDP process check skipped. Full pre-commit, explicit checks for the
new header/scenario and `git diff --check` passed. The first baseline run reported
an animation-clock script-load failure; subsequent complete runs passed without
changing that test. No live reference process or private-data capture was run for
this task. Changes remain uncommitted because Git metadata is read-only.

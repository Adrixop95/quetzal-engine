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
With lighting inputs loaded, no substitute radius or invented mask is used. The renderer and core `.vis`
comments still use their earlier darkness convention; they were outside this
sim-only change. The simulation now exposes raw brightness and border cells,
so the renderer's fallback fog/selection consumers need a separate integration
change. Its independently computed lighting is not a simulation parity check.

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

# Building placement and town build radius

Evidence labels: **V** means a reference fact inspected statically, **V(doc)**
means an existing documented reference fact, and **G** means an inference or an
engine approximation. Synthetic checks establish the implementation only. No
original executable or reference runtime was run for this task.

## Reference predicate

**V:** `0x45bd50` supplies the hall tile and radius 8 to `0x409940` for the
player preview. `0x409ce0` also uses radius 8 for income-building site search.
The mask is a 7 by 7 grid centred on the candidate tile. Every cell equal to 2
must satisfy the following predicate. Values 0 and 1 do not affect acceptance.

| Requirement | Evidence |
| --- | --- |
| Body cell lies inside the map | V `0x409940` |
| Squared distance from the hall tile is strictly less than 64 | V `0x409940`, `0x45bd50` |
| Terrain layer 0 is nonzero | V `0x409940`, `0x46c330` |
| Object cell is empty | V `0x409940` |
| Four orthogonal neighbours contain no building; diagonal buildings and neighbouring units do not block | V `0x409940` |
| Supplied side-map byte at the body tile is greater than 32 | V `0x409940` |

**V:** terrain layer 0 is the large-ground layer at `0x515a78`. Its initial
construction in `0x46bdb0` maps `.dat` values 0 and 1 to zero and other values to
9. This agrees with [movement rules](rules_movement.md). A resource deposit does
not bypass the terrain test. There is no independent rejection of mineral bytes
or graphical map overlays in the placement function.

**V:** for non-defence descriptors, `needA = (d60 != 0)` and
`needB = (d61 != 0)`. With an output mask, at least one body cell must contain a
nonzero byte in **either requested channel**. A is the low resource byte and B
the high byte. If both are requested, one nonzero byte in either channel suffices;
there is no requirement for both channels, minimum yield, full coverage, terrain
income flag, or active income schedule. Defence does not request either channel.
The resource rejection clears the entire output mask. Otherwise body cells get
1 or 0 according to their individual validity; non-body cells get 2.
See [economy rules](economy_rules.md), sections 1.1–1.3.

**V:** packet `0x1c` handling in `0x4577b0` does not repeat the placement test.
Admission in `0x45cec0` checks the build tree, creates/reserves the object and
queues it without payment. Payment occurs when production starts. This does not
prove that no other original command path performs validation (**G**).

## Engine implementation and limits

Faithful placement uses the shared `placement_cells` query for UI colours,
click validation, playtest validation and AI offers. Only body cells are emitted.
The test uses the live producing hall, strict squared distance per body cell,
`.dat >= 2`, exact structure masks, tile occupancy for other living objects,
orthogonal structure neighbours and descriptor income channels. Mineral bytes
and overlay IDs alone do not obstruct placement. Missing resources cannot meet
a requested channel. Missing side layers retain the engine's permissive fallback.

The command handler retains build-tree, queue-capacity and coordinate storage
checks, but does not call the placement query again. Waiting construction is
also not revalidated before payment. Changes to light, terrain, nearby objects
or helper settings therefore do not invalidate an admitted site. Duplicate or
otherwise invalid coordinates supplied directly through commands can be admitted;
normal UI clicks validate before submitting. Admission itself does not pay.
Replay semantics version is 14, so older engine saves are refused.

The optional `relaxed_placement` helper bypasses building-neighbour spacing and
the side-light threshold. It retains body bounds, radius, buildable terrain,
body occupancy and required resource channels. It is a clone modification, not
an original rule. Existing accepted construction survives helper changes.

Remaining approximations and dependencies:

- **G:** side `+0x690` is represented by the engine's side brightness grid. Its
  exact identity has not been established. The implementation reads a tile byte
  directly, without the interpolation used for picking. Legacy darkness grids
  are converted with `152 - min(152, byte)` before applying the threshold.
- **G:** the engine reconstructs object occupancy from structure masks and the
  current tile of other objects, including fliers. Exact original grid membership,
  scenery/sentinel objects, fractional-position updates and airborne exceptions
  need confirmation. Dead/removed engine objects do not obstruct placement.
- **G:** rectangle fallback for missing descriptor masks, a completed live hall
  requirement, in-map anchors and reservations without live objects remain engine
  policies. Original masks whose anchors are outside the map are not supported.
  Hall upgrades use the existing in-place path, not the player placement path.
- **G:** the automatic planner retains its additional diagonal gap and stable ring
  search. The original non-income and defence selectors have distinct searches;
  their ranking, tie breaking and candidate limits are not implemented here.
  This change establishes the shared body predicate, not AI site-choice parity.
- **G:** exact preview colours, artwork and admission/activation timing still need
  live observations. Task 64's audited observer and task 66's placement captures
  remain dependencies for live comparison; this work completes the static-rule
  implementation independently of those missing observations.

## Regression checks

`engine_unit` uses invented descriptors and maps to cover the strict axial and
diagonal circle, a footprint crossing the radius, body/map edges, ignored mask
cells, `.dat` classes, orthogonal/diagonal building neighbours, neighbouring units,
body occupants, dead/removed objects, byte thresholds 32/33, both resource flags
separately and together, a single resource byte, ignored-cell resources, defence
exemption, full red resource rejection, unpaid admission and production starting
without another site check. Existing UI tests check shared validity, mixed colours,
helper switching and replay. Mine/construction fixtures and two synthetic route
fixtures use sites within the smaller circle. These are not live parity evidence.

Validation on 2026-11-21: Release build succeeded. The 79-entry CTest run passed
72 checks and skipped six; its remaining construction stress fixture was moved
inside the strict circle, its distance model updated, and the focused rerun
passed. All pre-commit gates and `git diff --check` passed. Separate private-data
format and engine tests and a 12-tick clone mission smoke passed with dummy SDL
inside `unshare -rn`, using private game copies. No original/reference process
was launched. Private outputs remain in ignored `work/`; changes are uncommitted.

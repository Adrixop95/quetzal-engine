# Fog and vision implementation

V = a rule supported by reference inspection; G = an assumption or remaining
approximation. Synthetic tests verify the independent implementation, not parity
with a running original. The original executable was not run. This document
supersedes conflicting implementation claims in the task-120
[fog audit](fog_visibility_minimap.md); the reference contract is in
[fog, sight and minimap](fog_sight_minimap.md).

## Audit and changes

| Rule | Evidence and implementation |
|---|---|
| Per-side vertex brightness | V: one byte per vertex on a `(width+1) x (height+2)` grid. Zero is never seen, 1..32 is explored, and greater than 32 is currently visible. Task 120 already preserves these values and converts startup darkness before acquisition. Retained. |
| Illumination | V: daylight level times eight, plus additive glow saturated at 152, minus minimum sight darkness. Previous brightness is capped at 32 before merging new brightness by maximum. The loaded `SightData::daylight` already contains the multiplication by eight. Retained. |
| Ownership | V: sight sources belong to the exact side whose map is rebuilt; diplomacy does not share sight. Existing mutual-alliance and source-removal tests already cover this. Retained. Glow alone cannot reveal outside a side's sight mask. |
| Terrain and altitude | V: mask placement uses ground coordinates, with no terrain, height or line-of-sight term. Retained. Building display bounds may include render height; that is a visibility-query rectangle, not an altitude term in sight generation. |
| Units and effects | V: the interpolated 8.8 brightness must be strictly greater than `0x2000`. Retained interpolation and threshold; removed unconditional ownership exemptions from raw-fog object and effect presentation. An owned unit can therefore be hidden at brightness 32. Owned command-group membership remains independent of picking. |
| Buildings and defences | V: clip descriptor display bounds to the playable map and test the four outer vertex corners, each strictly greater than 32, without interpolation (`0x410340`). Implemented through the shared object predicate, so targeting, picking and presentation use the same current visibility. A bright center alone is insufficient. G: absent descriptors use the common visible-object rectangle. |
| Remembered buildings | V: display proxies copy visible source state and freeze when hidden (`0x42a940`). Added per-side last-observed snapshots, including type, owner, position, height, construction state, health and animation clocks. Both scene and minimap consume the snapshots. Hidden destruction retains the picture until its last position becomes visible. An observed replacement clears overlapping orphan pictures. Memory never creates a live target or grants selection. |
| Minimap background | V: `0x464ba0` builds a 180 x 180 indexed buffer from local fog bytes shifted right three, without sampling terrain or preview pixels. The blit uses palette resource 15, first palette block (`0x47b820`, `0x478330`, `0x478800`); the archive constructor's 16 argument is output pixel depth, not a palette resource. Replaced task 120's preview-shade path with that independent fog background, including when no preview exists. |
| Minimap buildings | V: draw remembered occupied footprint cells rather than one current-object dot. Implemented descriptor occupancy masks; G: missing masks use existing rectangular footprint dimensions. |

## State and rendering boundaries

Building snapshots update after fog at each simulation tick and once during
initialization. All side memories update together, independently of the selected
viewing side and number of rendered frames. They are derived presentation state,
excluded from simulation decisions and hashes. Normal command-log replay rebuilds
them. G: imported original saves do not yet restore their serialized historical
proxy records; only currently observed buildings can seed that memory.

Renderer position interpolation, fractional brightness sampling, fog texture
filtering and terrain lighting are preserved. Frozen building objects retain
fixed animation clocks; visible units still interpolate between simulation ticks.
G: exact reference palette gradients, resource-driven active/placed overrides,
special construction stages, marker team/selection colors, minimap margins and
non-square map scaling still need synchronized captures. The UI scales the
180 x 180 background into its existing map rectangle; it does not reproduce the
original integer scale and unused buffer margins. Missing minimap palette data
uses an explicitly provisional grayscale fallback. Disabled fog and legacy data
retain the preview/averaged-terrain diagnostic path.

Task 120 already established startup acquisition before the first end-of-tick
fog reconstruction and separate ally layers. Those behaviors and existing tests
are retained. Its description of the minimap as a shaded terrain preview and its
unresolved building-memory conclusion are superseded here. Alert producers,
minimap event timing and full screenshot parity remain outside this correction.

## Verification and remaining evidence

Focused synthetic regressions cover all four building corners at 32/33, a lit
center with dark corners, clipped bounds, first observation, independent side
memory, hidden ownership/construction/health/animation changes, observation
refresh, hidden destruction and later removal. Unit tests cover the ownership
threshold correction. Invented SDL palettes check truncating minimap indices,
side switching, never-seen index zero, fog toggles, and identical backgrounds
with or without a preview. Existing tests retain alliance isolation, daylight,
glow saturation, persistent exploration, replay and renderer interpolation checks.

The baseline Release build passed. Baseline CTest passed 73 tests with six skips;
all pre-commit hooks passed. Final Release build and the complete CTest suite
also passed: 73 tests passed and the same six private-data/network checks
skipped. All pre-commit hooks, explicit hooks for this new document, and
`git diff --check` passed.
No game resources, executable-derived implementation, tables or captures are
included. Live cross-program validation requires matched semantic setup and
paired observer captures as described in task 64; no such comparison was
performed here. The independent implementation and synthetic checks are
complete. The maintainer should use the task-120 Windows run sheet, adding
hidden building upgrades/capture/destruction and re-observation at corner-only
visibility. Record failures and exact versus tolerant image comparisons separately.

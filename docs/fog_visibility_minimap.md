# Fog, visibility and minimap audit

Status: **needs live run**. Read-only source inspection and synthetic corrections
are complete for the items below; this is not a claim of full game parity.
The reference executable was not launched. Git metadata is read-only; changes
remain uncommitted. No private captures, game files or executable-derived code
are included.

V means a behavior established by inspecting the private reference instructions
and their call sites. G means an assumption or an approximation still requiring
measurement. Test results describe the independent engine, not reference runs.
Addresses are research locators, not copied implementation. Private sources used:
`/home/adrox/aztec-re/work/gen/gen_064.c`, `gen_101.c`, `gen_113.c`,
`gen_115.c`, `gen_131.c`, with the private decompilation used to navigate them.
Existing [sight notes](sight_fog.md) and
[lighting contracts](sprites_mapping.md#light-masks-and-per-tile-maps) supply the
earlier descriptor and interpolation research.

## Sight and exploration

| Subject | Evidence and comparison |
|---|---|
| Sight range | V: sight selects a resource mask, not a fixed circular distance. Marine/Truck/Cannon use descriptor d6; Spider/Bird/Headtruck d8; Zeppelin d7; Building d55; Defence d58; Boat/Johnka select mask 71. The engine already loads these fields. G: a universal tile-radius conversion would discard daylight and fractional-position behavior; none is added. |
| Mask geometry | V: the vision masks describe darkness; their minimum combines sources. Ground position locates the mask on a 48-pixel grid with map width+1 and height+2 samples. A 20-sample mask spans 19 cells. Fractional stamping truncates weighted terms on each axis. The engine already implements this contract. |
| Illumination | V: ambient daylight and animation/effect glow build a separate brightness grid, saturated at 152. Visibility subtracts vision darkness from that brightness and clamps negative results to zero (`0x47cb30`, `0x47d070`, `0x47d430`). An illuminated enemy does not itself grant another side vision. |
| Explored versus visible | V: the persistent side grid is capped at 32 before current illumination is merged by maximum (`0x4583a6`, `0x47d460`, `0x47d430`). Exploration survives loss of a source. A target requires interpolated raw brightness strictly greater than 32; exploration alone is insufficient. These are two meanings in one persistent grid, not two independently maintained Boolean maps. |
| Buildings and units | V: the side loop queries object ownership and the virtual vision accessor at ground position (`0x458420`, `0x45842b`, `0x45845a`). Buildings use their descriptor masks too. G: exact construction-stage overrides, carried units, special vessels, secondary glow and altitude interactions remain incompletely audited. The engine's current alive/not-removed eligibility is retained. |
| Allied vision | V: the inspected update stamps only objects whose owner equals the side being updated. There is no diplomatic union of the layers in this loop (`0x458420` through `0x45848a`). The clone already keeps them separate; new tests explicitly use mutual friendship. G: exceptional script or multiplayer modes that might rewrite ownership or layers need a separate capture. |

V: mission loading (`0x430ae0`) combines the file's temporary darkness with
startup daylight before simulation acquisition. It extends the file's last
column and one last row to support border sampling. Unit sight is not stamped
as part of that file conversion. The previous clone instead queried legacy
darkness before tick 1 and converted against a constant full brightness at the
end of the tick. That could acquire a night-hidden enemy early.

The correction converts valid mission layers at simulation initialization using
the startup ambient value. Missing/malformed layers start dark when sight
resources exist. The extra unused bottom row remains zero. Imported original
saves keep their recorded convention and are not reinitialized. Without sight
resources the existing asset-free fallback remains G: permissive legacy UI and
a bounded acquisition distance.

V: computer initialization in `0x449980` writes raw value 24, below current
visibility. The clone's brightness convention previously substituted 128. The
raw path now preserves 24 too; resource-free legacy behavior is unchanged.

## Tick order and rendering

V: the reference updates objects/tactics before the fog reconstruction near the
end of `0x4577b0`. It caps previous exploration, stamps each side's current vision,
and merges global brightness afterward. Acquisition therefore sees the previous
raw side layer, including the startup layer on tick 1. The engine retains this
order. Its every-four-tick object scan and nearest-target policy are still G,
not an audited replacement for the original tactic scheduler.

New synthetic regressions force an acquisition scan on tick 1. Loaded darkness
is chosen so startup brightness is either 32 or 33. Only 33 permits acquisition
on that tick; both become visible after the fog update, and the next scheduled
scan can acquire the formerly hidden target. Replay must reproduce every final
state. Tests also check right/bottom startup borders, a building sight source,
mutual allies without shared sight, removal of a source and retained exploration.

V: world rendering uses interpolated grid brightness and palette shades; uniform
shade rounding is `(brightness+4)/8`. The minimap uses a different rule below.
G: the clone's terrain currently selects one sampled shade per tile and its
fallback Fog texture uses linear alpha filtering. This is not the reference's
full per-pixel palette gradient. Exact edge softening, sprite compositing and
night tint remain visual acceptance items. Existing synthetic tests establish
fractional sampling, bounds, visibility eligibility and fallback texture behavior,
not exact reference pixels.

## Minimap contents and input

V: `0x464ba0` builds tile lighting from the local side's raw layer, using
`brightness >> 3` (`0x464cc3`, `0x464d1c`, `0x464d24`, `0x464d70`). It repeats
tile samples at the minimap scale and shades the preview through the palette.
It does not interpolate a world fog overlay across minimap tiles. The clone
previously used one preview shade, an RGB daylight tint and a smooth black
overlay. For loaded previews and raw fog, it now selects the user's palette
variant per tile, without the extra RGB tint. Palette data stays in user files.

Synthetic SDL tests use an invented palette whose shades have different hues;
this catches substitutions with black overlays or RGB multiplication. They
check the truncating shade choice, side changes, fog toggles and reinitialization.
G: maps without a preview still use the engine's averaged-tile fallback and fog
overlay. That fallback is not a measured original behavior.

| Content | Reference evidence and remaining clone gap |
|---|---|
| Terrain | V: the shaded preview is drawn before object markers (`0x464ba0`). Corrected for the loaded-preview raw-fog path. |
| Units | V: marker passes check liveness and visibility, use side colors, and distinguish locally controlled markers. Marker extent is at least two pixels and grows with minimap scale. G: the clone's fixed 2x2 dots, provisional team RGB colors and interpolated positions remain approximate. |
| Buildings | V: a distinct marker pass enumerates occupied positions before ordinary unit markers. G: the clone currently draws a single dot for buildings; footprint shapes and explored-only building memory require verification. Do not infer persistent enemy-building memory from terrain exploration. |
| Camera | V: the view draws a white outline after markers. G: the clone's fractional camera rectangle and general zoom policy differ from the reference's tile-based viewport bounds and require measurement at the original resolutions. |
| Alerts and attack ping | V: `0x462510` accepts a location only for the viewing side and resets a presentation counter to four. `0x464ba0` draws shrinking outlined squares and decrements that counter per minimap draw, not simulation tick. Callers include town notification paths. G: the full set of attack/production notification triggers and throttling is not established. The clone has no equivalent notification-to-minimap producer yet. A permanent `last_hit_by` field is insufficient to reconstruct event timing, so no guessed per-hit ping is added. |

V: camera centering `0x449290` subtracts half the view size and clamps to the
playable extent, with reference-specific right/bottom margins. G: exact minimap
press/hold/release routing and right-button command behavior need a live input
trace. The clone currently recenters on left press/hold, clamps a drag outside
the map, and issues ground movement on a right click or an explicit Move target.
The shared minimap-to-world conversion now clamps inputs and handles an
uninitialized/zero-sized widget. Synthetic tests cover center and outside points.
No change is claimed to establish exact reference click timing.

## Maintainer Windows run sheet

Task 64's audited semantic setup/input adapter remains absent from this checkout
(`tasks/queue/64-shared-runner-observer.md`). Exact cross-program acquisition,
alert and interaction comparisons depend on it. The table can also be captured
manually on the Windows installation. Do not invent a working adapter command.
Do not run the original in the restricted Codex environment or omit isolation
to bypass its network-namespace restrictions.

Use a private installation copy/profile and private output directory. Keep game
assets, generated maps, input logs, state traces and captures outside tracked
paths. Use the same executable revision, map, side, daylight, difficulty,
modifications, resolution, camera and tick schedule on both implementations.
Record two reference replays and two clone replays, retaining setup/input identity
and run failures. Manual screenshots alone cannot establish acquisition ordering.

| Case | Setup/actions | Required private measurements; acceptance |
|---|---|---|
| Startup acquisition | Place a stationary aggressive source and hostile target at a loaded-visibility boundary. Repeat at daylight and night. Pause before tick 1, then single-step. | Startup raw samples, interpolated target brightness, acquisition scan tick and selected ID before/after fog. Compare at strict 32/33 and fractional boundaries; do not equate clone scan cadence with reference cadence. |
| Range and producers | For each descriptor family, move one source across whole/half-cell positions; include each construction stage, transport boarding, airborne source and source death. | Raw layer samples and first/last visible coordinates by tick. Preserve mask IDs and source identity privately; publish only behavior conclusions. |
| Exploration and allies | Explore, leave, kill the scout; change day to night. Repeat with two mutually friendly sides and a hostile observer. | Each side's raw layer, terrain appearance and enemy unit/building eligibility before/after leaving. Distinguish actual shared vision from shared ownership. |
| Fog edges | At fixed camera, capture bright/dark plateaus and asymmetric borders at zoom 1 and supported original resolutions. | Exact and tolerant pixel metrics separately for terrain, units, buildings and border regions. Record palette/shade indices as well as final pixels; do not accept a tolerant screenshot as full parity. |
| Minimap terrain | Repeat at several raw brightness values within one shade bucket and across a bucket boundary; move camera without advancing simulation. | Shade changes must follow truncation, not rounding. Record tile-aligned edge locations and preview dimensions. Check that camera movement does not evolve exploration. |
| Markers and viewport | Place own, allied, hostile, neutral, building, vessel, dead and removed objects at center and map edges. Repeat side switch and scale change. | Marker footprint, side/selection colors, fog eligibility, draw order and camera rectangle bounds. Include fully explored but currently hidden enemy buildings. |
| Alert/ping | Attack an owned unit and owned town separately; trigger production notification; repeat attacks rapidly and while offscreen. Pause simulation while continuing draws. | Event producer, side, location, replacement/throttle behavior, four minimap draws and any alert text/audio. Establish whether each attack actually produces a ping before implementing a trigger. |
| Click/drag | Left press, hold, drag outside each edge and release; then right click and explicit Move/Attack target modes. Repeat with UI pause/modal/focus loss. | Mouse events, resulting camera bounds, command packets/target positions and whether a command consumes the click. Check all corners and non-square maps. |

Portable synthetic verification, with build outputs inside this worktree:

```sh
sh tools/bootstrap.sh --no-hooks
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DAZTEC_GEN_DIR=
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
pre-commit run --all-files
pre-commit run --files docs/fog_visibility_minimap.md
git diff --check
```

Baseline: Release build, all 63 runnable checks and all pre-commit hooks passed;
five private-data entries and the restricted UDP process entry skipped. Focused
post-change checks passed: `engine_unit`, `render_contracts`, `animation_headless`,
`ai_unit`, `ai_skirmish`, and `ui_screenshots`.

Final verification (2026-11-15): Release configure/build passed. The full suite
passed all 63 runnable checks, with the same six skips as baseline. All pre-commit
hooks, explicit hooks for this new document, and `git diff --check` passed. The
last run includes the computer-side raw-24 initialization regression and the
sharp minimap shade-boundary pixel checks. No reference runtime or private-data
capture was run. Alert producers, building marker shapes, exact fog gradients
and live input timing remain open as listed above; task 64 and the Windows run
sheet are required before claiming complete parity.

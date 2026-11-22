# World renderer implementation

This implements the factual rules in [renderer_rules.md](renderer_rules.md) and
its correction to [fog_sight_minimap.md](fog_sight_minimap.md#correction-2026-11-21-from-docsrenderer_rulesmd).
**V** means an inspected reference rule implemented independently and checked
with invented inputs. **G** marks an inference, fallback or incomplete mapping.
Neither label claims live pixel parity. No original executable was run for this task.

## Presentation modes

`--original-look` selects current tick positions for objects and effects, light
surface palette rows, and one simulation interval per interactive presentation
pass, sleeping for the remaining interval. It disables render interpolation and
catch-up batches in this mode. View/wind phases follow completed logic ticks;
paused repeated presentations do not advance them. The normal presentation
retains interpolation, catch-up updates, zoom and arbitrary window dimensions.

For a reproducible original-style capture, use `--original-look --size 800x600
--zoom 1 --software --no-vsync --no-hud`, a fixed camera and an explicit tick
boundary (`--ticks` or scripted capture). Use dummy SDL inside `unshare -rn`
and a private installation copy as specified in EXPERIMENTS.md. Capture and user
profile outputs belong under ignored work directories. This mode leaves window
size and zoom configurable; it does not force an 800x600 desktop or claim exact
reference UI geometry. Headless L0 observation retains its existing renderer
configuration and is not silently relabelled as original-style evidence.

**G:** loader/menu forced redraw scheduling, original menu's every-eighth-draw
refresh, loading spinner, view-mutex timing and exact pause phases remain outside
this mode. Wind strengths still need their simulation producer. Animated terrain
with fewer than eight cells retains safe modulo wrapping.

## Draw order

**V:** terrain draws first. A provider can flag a `SpriteDraw` as `underlay`;
those parts draw immediately in object collection order, before any sorted item.
Their light sampling is two-dimensional. Remaining object parts, terrain
overlays, debris, effects and aircraft use the shared comparator: height -1
first, then ground y ascending and x descending. There is no air-last pass.
Underlay-tagged parts and height -1 sorted entries are distinct cases.

Each ordinary sorted item finishes its selection ellipse, first shadow/body,
second shadow/body, health art and marker before the next item. Shadows remain
at ground position and select shade 12; bodies retain provider altitude offsets.
The ellipse follows the elevated position. `SpriteItem::marker` selects common
sprite 5 for kind 1 and sprite 2 otherwise, with the view phase modulo its cells.
The post-world callback now draws the drag box followed by placement preview;
the panel/minimap are drawn afterwards by the application.

**G:** equal keys retain engine insertion order; underlay mutual ordering is not
established by the reference comparator. Existing construction overlay selection
is retained: it is not reclassified as virtual +0x2c without evidence. Native
object-query ordering, exact sprite-bound culling and marker/flash producers
remain unresolved. Path-dot drawing and original menu dim-box geometry are not
implemented here; existing application menus retain their presentation.

## Lighting and palettes

**V:** the original-style path reads local-side brightness vertices, falling
back to the engine mask grid when simulation fog bytes are absent. Uniform light
surface blocks use `byte >> 3`, whereas directly drawn uniform terrain uses
`(byte + 4) >> 3`. Gradient blocks bilinearly interpolate four corners over 48
pixels and quantize with an ordered 4x4 threshold. Sprite shade variants are
cached in the atlas; equal horizontal runs share geometry. This computes the
light surface on demand instead of allocating an entire map-sized pixel buffer.

Bodies, effects and overlay bodies sample across the width at their ground y,
independent of hotspot or altitude. Collected underlays and height -1 debris use
full two-dimensional sampling. The `SpriteItem::flash` input replaces body light
with flat row 19 on odd view ticks. Selection and health use palette 3, row 0.
Unit body/underlay/preview header palette ids below 4 add `16 + side_color * 4`;
fixed header palettes remain fixed. Side colour is the mission colour mapping,
not necessarily its owner index. Palette resources come only from user files.

**G:** the exact original dither thresholds are unknown. A conventional Bayer
matrix supplies the ordered threshold pattern; fixed-point interpolation
rounding is not certified byte-for-byte against the original. The legacy
interpolated presentation retains centre/anchor shade sampling. The shadow
palette selector is unresolved, so shadows retain translucent silhouettes even
though their shade key is 12. RGB daylight uses the existing vertex tint rather
than the original palette-table rebuild/ramp. Alpha blending and raw 16-bit
source shading retain existing decoder limitations. These gaps prevent exact
original-look claims even when a screenshot appears similar.

## Status art and placement

**V:** status art uses the documented size thresholds 44, 55, 67, 83, 102, 125,
154 and 189 against twice the supplied display size. Ellipses select that record;
health art selects the preceding record, clamped at zero. Ratios use integer
`hp * 256 / max_hp`; level thresholds are strictly above 178 and 102, so frame
is 0, 1 or 2. The source width is clipped to `width * ratio >> 8`, with no
horizontal stretching of the retained pixels. Status draws only for visible
entries, with nonzero HP ratio/max HP and selection or ViewAll enabled.

**G:** exact native display-size and bar-rectangle producers are not available
in this checkout. `SpriteItem::display_size` accepts an explicit size; otherwise
the renderer retains approximate unit/structure values 24/77. The last known
art record is used above the last documented threshold. The bar top retains the
old unit/structure offset adjusted for height. Missing health art retains a
rectangular diagnostic fallback. Native extra health-visibility virtual flags
and exact selection bounds remain unmapped.

**V:** placement draws one building image clipped independently to a 7x7 grid
of 48-pixel cells, after the drag box. Shade status 0/1/other selects palette rows
20/21/22 with the local team palette. Clipping respects and restores an existing
SDL clip rectangle and uses shared rounded cell edges at fractional zoom.

**G:** the simulation currently exposes occupied valid/blocked cells rather than
all 49 native status values: these map to 0/1, other cells to 2. The placement
object's dedicated sprite accessor is unresolved, so its ordinary animation
frame (slot 0, frame 0) is used at the snapped cell anchor. Missing art retains
the existing diagnostic grid. Exact anchor bias and row colours depend on further
reference evidence and the user's palette data.

## Validation and remaining evidence

The Release baseline passed all runnable CTest entries and pre-commit before
changes. Synthetic renderer checks cover sort-key direction, equal keys and
strict-order properties; uniform rounding versus truncation, gradient quantization,
health thresholds/clipping, size transitions, palette arithmetic and flash ticks.
The dummy-SDL gallery checks actual queued terrain/body shade rows, no-interpolation
positions, elevated ellipses, per-item health/marker order, collected underlays,
gradient terrain and one-row body sampling. All sprites, palettes and animation
records used by these checks are invented.

Validation: Release configure/build passed; the full 82-entry CTest suite had
76 passes, six expected skips and no failures. The isolated private-data format
and engine checks passed separately with dummy SDL under `unshare -rn`. The
independent clone also saved an 800x600 original-style screenshot in that
isolation using a private game copy. All-file pre-commit and explicit checks of
new files passed. A final renderer-only rerun covers the zero-ratio health edge
case added during review. No record/replay comparison or synchronized original screenshot was
performed: original execution is prohibited for this task. Missing native
presentation producers and original observations remain dependencies; synthetic
success establishes implementation properties only.

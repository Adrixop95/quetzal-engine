# Sprite and lighting integration inventory

Updated 2026-10-26. Reference labels **R/C/T/P** retain the meanings in
[sprites_mapping.md](sprites_mapping.md). An implementation or synthetic control
is not new reference evidence. No new P evidence was obtained in this worktree.
The historical rebuild checklist is not the integration inventory.

| Contract | Implementation and synthetic controls | Evidence / remaining uncertainty |
| --- | --- | --- |
| Kind-specific body selection | `AnimSpriteProvider` uses `body_sequence`, full-angle/cycle-ready inputs, and separate transport selectors. Moving Spider/Headtruck legs and Bird flight can continue during attack. | R/C; attack/move phase synchronization with the reference remains pending. The current simulation supplies only eight facing directions. Independent body and firing clocks now preserve locomotion phase across shots; full-angle aim and steering cooldown producers remain pending. |
| Timed prefix followed by hold | Resource durations remain authoritative through `AnimSet::at_tick`; no unconditional idle fallback on a missing death frame. Only Marine holds its final corpse image. | R/C; synthetic provider controls exercise ticks before, at and after the hold boundary. |
| Secondary sprites | Ordered base shadow/body then secondary shadow/body; firing completion returns turret to slot 0. Simulation retains `has_fired` for later idle selection. | R/C/T; exact secondary attack pixels pending. Independent turret aim/cooldown behavior remains incomplete. |
| Body and shadow heights | Ground body subtracts frame height; Spider height is zero; aircraft uses explicit `Object::render_height`; shadows stay on the ground. | R/C/P history; aircraft altitude producer remains pending, default height is zero. No guessed altitude is claimed verified. |
| Construction and damage | Construction seeks by HP with a hidden prefix; built buildings continue looping slot 1 independently of HP. Explicit overlay input requests slot 6. Fires 154/155/156 are separate display items at Y+1. | R/C/T; synthetic threshold controls pass. Construction-overlay producer remains pending. Simulation-owned fires use independent effect clocks and replace provider preview decorations when present. |
| Destruction | Known-radius buildings select 116/115/114, stable engine variant, resource timing, and 250-tick retention. Vehicle/aircraft death uses separate 152; ground vehicles also create underlay 113. | R/C/T; radius producer remains unknown (`-1`), so ordinary engine structure deaths use a provisional small-rubble effect. Known radii retain provider-selected rubble without duplicating that effect. Construction cancellations stay hidden. Engine variant choice does not establish original RNG parity. |
| Shared ordering | Terrain overlays, ordinary bodies, fires, explosions and debris share height=-1 first, ground Y ascending, X descending ordering. Each item completes its own layers. No air-last pass or global shadow pass. | C; synthetic equal-Y/X, altitude and underlay controls. Overlapping reference pixels pending. |
| Overlay frames | Header cells/hotspots and `overlay_frame` replace tile-coordinate phase guesses. Wind/counter are explicit scene options. | R/C; region strength and unpaused view-counter producer pending (default still wind). |
| Light resources | `GameData` already loaded light.xbe. `LightGrid` now consumes its masks; no mask samples are embedded. Vision ids load from descriptor CSV fields documented in rules_types.md. | R/C; synthetic masks are invented asymmetric planes, not copied resources. |
| Grid placement and combination | 48-pixel spacing, map width+1/height+2; position minus nine tiles, four-sample interpolation, clipped stamps, saturated additive glow, minimum vision darkness, explored cap 32, nonnegative subtraction. Sources use ground positions; secondary animation light is not implicitly added. | R/C contract; independent interpolation/truncation and clipping controls. Exact rounding and footprint-edge behavior still need synchronized reference P. |
| Spatial shade and visibility | Renderer samples the combined grid bilinearly for tile centres/object ground positions and directly selects palette shades. RGB tint is separate. Other-side object visibility requires brightness >32. The old black fog overlay is omitted when mask lighting is active. | Engine integration, not P. Per-pixel palette gradients, structure bounding-corner visibility, original persistent .vis initialization, and minimap fog remain incomplete. Simulation AI sight remains a separate placeholder. |
| Effect ownership | Simulation effects and provider preview decorations share the renderer; known-radius rubble and simulation-owned fires are drawn once. | Owners **104/153 remain unknown**, R only. Projectile flight, impact effects and effect light dispatch are integrated with the same display list and light grid. Flight and splash behavior remain provisional; unresolved descriptor damage/scheduling cannot fire generic shots. |
| Alpha and pixel metrics | Alpha sprites now use the same team-palette remap as ordinary sprites. Comparator preserves exact RGB565 and separately named tolerant counts. Partial-alpha pixels are skipped unless a registered background is supplied. | Synthetic straight-alpha/background controls only. RGB565 exactness is not full RGB888 identity. Original alpha/shadow blending and rounding still require P. |

## Private probe status

`tools/zspr_contract_probes.py` schedules attack/death (Headtruck, Spider,
Zeppelin, Bird), overlapping objects, and a candidate night-time capture. It
calls the existing isolated runner; it never bypasses network namespaces or
shares writable game state. It writes its inventory only under ignored `work/`.
The candidate night time is configurable: confirm actual ambient brightness,
active glow and local vision before calling it a night-lighting observation.
A process exit of zero only means a capture completed, not P verification.

The current attempt wrote `work/render-contract-probes/inventory.json` and exited
2. All three cases are **pending**: `game/Aztec.exe`, mission data,
`build/aztec_recomp` and `work/vtables.txt` are absent. No reference run or pixels
were produced. Earlier namespace failures remain failures, not evidence here.

With private prerequisites available:

```sh
python3 tools/zspr_contract_probes.py --game /private/game \
  --runtime /private/aztec_recomp --vtables /private/vtables.txt \
  --output work/render-contract-probes-new
python3 tools/zspr_compare.py /private/game work/render-contract-probes-new/overlap \
  --objects s_0.txt --shot shot_04.ppm --after-objects s_0.after.txt \
  --require-stable --tolerance 40 --report work/overlap-comparison.json
```

The runner writes a pre-shot scan, screenshot, post-shot scan, and three-column
`samples.tsv`. `--require-stable` rejects unequal raw or dereferenced dumps. A
stable bracket is necessary evidence of unchanged sampled state; it is **not**
an atomic render/tick snapshot: the reference host presents staged frames on a
separate thread. Task 01 synchronization infrastructure can replace this gate
when available. Pending transient attack/death brackets must not be promoted to
P just because a nearby frame scores well. `PROBE_SCRIPT` can supply private
orders/setup commands before sampling; verify target orders instead of treating
idle no-hit results as immunity.

For alpha, supply `--background /private/registered-background.ppm`. The report
names the assumed straight-alpha/floor/RGB565 model, included alpha count and
skipped alpha count. Compare occluded/overlapping items with a known full scene
or visibility mask; per-component scores are diagnostic and cannot prove the
composed display order. Keep screenshots, pixel arrays, backgrounds and reports
under ignored paths.

## Checks

Baseline: Release configure/build and CTest passed (six independent checks,
two game-data skips); pre-commit passed. The cache was relocated to
`build/ccache` after the default cache attempted a read-only home-directory write.
After integration: Release configure/build and ten CTest entries passed (eight
independent checks, two game-data skips). `render_contracts` covers provider
selection/layers, heights, construction/damage boundaries, ordering and light
controls. `zspr_tools` checks exact versus tolerant comparisons, alpha/background
math, scan stability, descriptor HP and mobile Cannon/building probe placement.
Probe maps clear inherited troops and scenario actions. These validate independent implementation, not game
parity. Final pre-commit checks include new files explicitly because git metadata
is read-only; changes remain uncommitted.

## Remaining animation states (2026-10-27)

`GameSim` supplies independent body and attack ages in simulation ticks. The
renderer samples installed frame durations at 12 Hz, including timed prefixes,
holds and one-shot completion. Firing can finish after the legacy eight-tick
simulation animation tag returns to idle; body cycles do not restart on a shot.
Construction completion starts the built loop at tick zero. These visual clocks
are included in replay state hashes; weapon scheduling remains provisional.
Normal Boat/Johnka UGI ids now use the same scripted selectors, zero height and
vision mask as their render-only transport ids. Explicit construction state 6
is recognized without relying on the progress fallback. Unaligned aircraft and
Truck motion seeks the direction frame until cycle readiness and alignment hold.

Synthetic controls cover idle and supported movement for all 90 initialized
types (Camera intentionally has no body), independent turret/body ages, long
firing sequences, corpse retention, construction, direction alignment and clocks
beyond 32-bit ticks. `animation_headless` renders invented RGB565 sprites through
`SceneRenderer` with dummy SDL drivers. It checks exact pixels across attack
completion and repeated renders at different interpolation fractions, and checks
that drawing does not advance simulation clocks. These are implementation checks,
not reference pixel parity.

Missing dependencies remain explicit: this checkout has no gather/cast orders or
animation-state producers, and the mapping establishes no dedicated gather/cast
slots. Economy harvesting is an accumulator, not a unit gather action. No slots
are guessed for these names. Boat/Johnka phase transitions, independent turret
aim, Bird steering cooldown, Cannon hop restarts and aircraft altitude still need
simulation producers.

Release build and all nine independent CTest checks passed; the two configured
game-data entries skipped. Both game-data binaries subsequently passed against a
private game copy inside `unshare -rn` with dummy SDL drivers. The independent
engine also rendered mission 1 after 24 simulation ticks in that namespace:
`work/animation-engine.png` (800x600), with output in `work/animation-engine.log`.
These local artifacts remain ignored. Pre-commit passed using the existing cache.
The private reference probe used symlinked assets, private writable state, dummy
SDL drivers and `unshare -rn`; namespace interface setup failed with
`Cannot open netlink socket: Operation not permitted`. Its log is ignored at
`work/animation-state-probe/out.txt`. No reference pixels or timing observations
were obtained, so synchronized reference animation validation remains pending.

# All-type functional coverage

Audit: 2026-11-25. **V** identifies established descriptor or inspected behavior
facts; **G** identifies unresolved behavior or an explicit engine policy. A passing
synthetic check establishes an engine contract, not original-game parity or a
complete playable-game verdict. No original executable or reference runtime was
run. The inputs are the existing public `data/rules.json`, the loaded CSV rules,
[descriptor use matrix](descriptor_use_matrix.md), and
[specials completion matrix](specials_completion_matrix.md).

`type_coverage` exercises all **90 distinct IDs**, all **123 build-tree recipes**
and successful production of all **78 distinct recipe types**. The other 12 types
have no ordinary build-tree entry; synthetic placement tests their lifecycle
without inventing recipes. Entries for an already attained hall level are checked
as disabled, while higher upgrades complete and change the town level.

## Checks shared by every group

- Python derives expectations from the existing JSON at test time. C++ compares
  the loaded kind, cost, build time, development and supported V descriptor fields
  against them, including primary/secondary HP, regeneration, armour, body/turret
  resources, weapon, sight, speed, altitude, turn step and sound IDs. G words and
  guest pointers are excluded. No extracted fixture or generated table is added.
- Each type is placed on an invented map. Invented animation banks encode their
  resource identity in sprite IDs: body and turret selection must match loaded
  descriptors. Camera must have no body sprite. Marine/Defence marker lookup also
  uses the descriptor resource. These are sprite-selection checks, not pixel or
  original-animation validation.
- Move either changes position with the expected movement controller or leaves
  the object stationary. Johnka uses its enabled scripted controller; a waiting
  ferry ignores player Move. Aircraft use flight rather than a ground agent.
- Every armed type releases its descriptor weapon against an eligible target,
  delivers positive damage and emits the configured projectile/muzzle/hit/impact
  resources. Projectile identities must belong to the selected weapon or its
  components/payload. Unarmed types cannot release a shot. Paraplan and the air
  tower receive airborne targets; the bomber receives a short-range ground target.
- Every type receives Rifleman category damage, Tuzem physical-arrow damage and
  Spider flame-stream damage, or the applicable protection/air eligibility rejects
  the hit. Physical and fire damage are checked independently against the loaded
  armour factors at the damage callback, before regeneration. Secondary HP is
  preserved. Synthetic large instance HP keeps the experiment alive; it does not
  replace the descriptor maximum.
- A replayable script kills each type, checking Marine death state, vehicle
  explosion, ground debris, Abdula's extra decoration and the engine's provisional
  unknown-radius structure rubble. This also checks the explicit kill path for
  scripted objects that ordinary weapon damage cannot kill.
- Versioned engine save containers are serialized, decoded and restored through
  `replay_save` during movement, combat, construction and death. Hashes must match
  at restoration and on each of three further ticks. This checks the engine's
  replay saves, not original-save continuation.
- Every recipe is compared with JSON, including ordered prerequisites and scope.
  Missing prerequisites disable it; moving a prerequisite out of town only keeps
  side-wide prerequisites satisfied. Construction finds a legal site; queued
  production/upgrades run to completion with descriptor health and a saved
  continuation before and after completion. No missing prerequisite is bypassed.

## Group matrix

Counts partition the 90 types; groups describe behavior rather than repeating the
data table. All rows run the shared checks with the applicability described here.

| Type group | Count | Production / placement | Movement and combat / death covered | Remaining G or dependency |
| --- | ---: | --- | --- | --- |
| Marine: infantry, animals and heroes | 17 | Build-tree production where present; Spider axethrower and Abdula use placement | Descriptor speed; direct, sweep, slow, drain, arrow, axe, paired-carrier and standalone area families; corpse animation and Abdula decoration | Exact native shot phases, crowded spatial ordering, Tuzem end-of-mission death latch; special candidates remain as documented in the specials matrix |
| Spider vehicles | 2 | Production and placement | Ground agent; crossbow projectile/payload or flame stream; explosion and debris | Turret turn integration, native stream deletion/animation handshake and collision parity |
| Headtruck vehicles | 8 | Production and placement | Ground movement; paired axes, siege carriers, arrows, flame and scattered salvos; explosion and debris | Exact muzzle geometry, turret phases, projectile ordering and collision parity |
| Truck: Catapult | 1 | Placement; absent from ordinary build tree | Mobile descriptor speed, carrier/payload, explosion and debris | Native turn/release phase and ground launch height |
| Cannon: Hopcannon | 1 | Production and placement | Move and explicit Attack accepted; ground agent, carrier/payload, explosion and debris | Constant six-pixel pacing approximates the V eight-tick tile animation; per-frame hops, restart/turn phases, ground launch height and collision class remain unresolved |
| Bird aircraft | 3 | Production and placement | Flight/altitude; Eagle unarmed, Deltaplan paired arrows, Paraplan direct anti-air; explosion without ground debris | Exact steering/arrival/animation phases and paired muzzle geometry |
| Zeppelin aircraft | 2 | Bomber production; unarmed Zeppelin placement | Flight/altitude, unarmed rejection or paired falling bombs; explosion without ground debris | Exact hover/arrival and bomb geometry/timing; no inferred passenger ability |
| Ordinary buildings and prerequisite facilities | 44 | Construction, queued production or upgrades where the tree permits; outposts and alias type 68 use placement | Fixed footprint, no weapon, descriptor protection and armour; provisional unknown-radius rubble | Native automatic placement/queue phase, rubble inputs and rendering parity; no inferred flame turret for type 68 |
| Armed Defence towers | 7 | Production/construction and placement | Stationary; descriptor weapons, turret/body identity, damage and death effects | Native target scoring, origin/height, paired geometry and projectile/animation phases; see tower-specific tests |
| Unarmed Defence: watchtower and shipyard | 2 | Watchtower production; shipyard placement | Stationary, no fabricated weapon, armour/protection and death path | Native rubble inputs and presentation; no independent shipyard production command |
| Boat | 1 | Placement, no ordinary recipe | Waiting ferry rejects Move and weapon damage; explicit script kill and save | Live ferry timing/collisions and death presentation remain G; boarding/sailing/unloading are covered separately by `transport` |
| Johnka | 1 | Placement, no ordinary recipe | Enabled scripted diagonal movement; weapon 54 salvo; ordinary damage rejected, explicit kill and save | Native visual-think cadence, phase/order, collisions and death presentation; no inferred passenger command |
| Camera | 1 | Synthetic marker placement, no recipe or descriptor family | No body or player movement; zero maximum getter, engine marker damage/kill/save behavior | Synthetic marker combat is an engine policy, not a verified camera mechanic. Actual temporary vision/lifetime uses `script_cameras`, covered separately by `script_unit` |

## Defect corrected

The all-type movement check failed for Hopcannon 87: `RulesData::init_defaults`
excluded kind 9 from mobile types. It had no movement agent; Move and Attack
were filtered out as commands requiring mobility, and the UI disabled those
actions. This contradicted the V animation finding in
[animation timing](animation_timing.md): its eight walk frames cover one 48-pixel
tile. Kind 9 now gets a ground agent. Constant six-pixel pacing supplies that
average cardinal speed; `speed_src` and collision classification remain
placeholder because native per-frame displacements and restart rules are not
implemented. A separate regression orders an attack on the farther of two
enemies and requires that exact target, preventing automatic nearest-target fire
from hiding command rejection. Simulation semantics changed, so the engine save
replay version advances from 16 to 17; older replay saves are explicitly refused.

## Validation and completion boundary

Run with no private installation:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DAZTEC_GEN_DIR= -DGAME_DIR=
cmake --build build --parallel 2
ctest --test-dir build -R '^type_coverage$' --output-on-failure
ctest --test-dir build --output-on-failure
pre-commit run --all-files
pre-commit run --files tests/test_type_coverage.cpp tests/test_type_coverage.py docs/type_coverage_matrix.md
```

The coverage driver is portable C++20/Python with generated temporary inputs and
no original executable dependency. Linux validation does not establish native
Windows/macOS success. `combat_unit`, `ground_weapons`, `towers_specials`,
`hypno_ownership`, `transport`, `script_unit` and the save suites retain their more
specific contracts; this integration matrix does not replace them.

Validation completed on 2026-11-26: the Release build passed; the full 86-entry
CTest suite had 80 passes, six skips and no failures. The skips were five
private-data checks and restricted-network process creation. Separate `formats`
and `engine` checks passed with private game symlinks, separate mutable state and
dummy SDL video/audio under `unshare -rn`. All-file pre-commit and explicit checks
of the three new files passed. No original executable or reference runtime was
launched; native Windows and macOS checks remain for CI.

The descriptor and special-mechanic dependencies are present in this checkout.
Exact non-Marine/non-Defence animation-marker scheduling, native movement and
projectile phases, real-data presentation and original-save continuation remain
incomplete. Passing all 90 cases therefore does **not** mean every type is fully
reference-equivalent or that the complete playable-game goal has been achieved.
Unknown G behavior stays explicit rather than acquiring invented mechanics.

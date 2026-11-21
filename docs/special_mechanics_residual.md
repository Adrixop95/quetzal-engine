# Special mechanics residual: capture, Johnka, healing, latches, effects, Hypno

Date 2026-11-21. Static analysis only (corpus `all_full.c`, objdump of `Aztec.exe`, vtable
dumps, campaign map text). V = read from code or data, G = inference. Numbers are hex (decimal).
Extends `docs/ownership_mechanism.md`, `docs/economy_residual.md` (B, F) and
`docs/towers_air_specials.md` section 3; nothing here contradicts them except where noted.

## 1. Object predicates resolved from vtables (V)

Slot dumps of the object vtables (think is slot `+0x60`; `0x402950` = return false,
`0x471240` = return true):

| Class (vtable) | kind `+8` | `+0xa4` | `+0xe8` | `+0xf0` | `+0xf4` | `+0xfc` | heal `+0xd8` |
|---|---|---|---|---|---|---|---|
| Marine `0x4932cc` | 0 | false | desc byte `+0x2d` | false | not byte `+0x2d` | false | `0x402990` |
| Cannon `0x490380`, Spider `0x4940ec`, Truck `0x4948e8` | 0 | false | false | true | false | false | `0x402990` |
| Headtruck `0x492c38` | ? | false | false | true | false | false | `0x41cbe0` |
| Johnka `0x492d64` | 7 | false | false | true | false | false | `0x402990` |
| Boat `0x48faa8` | 6 | false | false | true | false | false | `0x402990` |
| Bird `0x48f97c`, Zeppelin `0x4951b8` | ? | **true** | false | false | false | false | `0x402990` |
| Defence `0x4905bc` | 1 | false | false | true | false | **true** | `0x41cbe0` |
| Building `0x48fd20` | 1 | false | false | true | false | false | `0x40fa50` |

- `+0xa4` = **airborne** (only Bird and Zeppelin). Closes the open item in economy_residual G.
- `+0xf0` = **machine** (vehicles, boats, Johnka, towers, buildings).
- Marine byte `+0x2d` is the Soldier flag (`0x4337e0` returns "Soldier"/"Animal"). `+0xe8` =
  soldier, `+0xf4` = **animal**. From `desc_marine.csv` d11 (V): animals are Snowman (param 4),
  Knight (6), Bear (7), Elefant (11), Jaguar (12), Bizon (13); all other Marines are soldiers.
- `+0xfc` true only for Defence: the "armed building" test.
- Kind `+8`: Marine/Cannon 0 (`0x402430`), Building/Defence 1 (`0x45f030`), Boat 6
  (`0x4083b0`), Johnka 7 (`0x42d930`).

## 2. Town capture

Call chain (V): town vtable `0x494648` slot `+0x138` = town update `0x45e8e0`, which calls in
order `0x454820`, `0x45d270`, `0x459bb0`, CheckCapture `0x45b1e0`, RepairUnits `0x45d4d0`.
The tick function around `0x457f71..0x4581e8` calls slot `+0x138` for every tactic in the
lists `0x5156f8` and `0x4d9b50` each pass (V); one pass per simulation tick is G (no modulo
gate seen).

### CheckCapture `0x45b1e0` (V)

- Anchor = town virtual `+0x94` (hall tile). Radius r = 8 tiles if `+0x15c` != 0, else 4
  (`+0x15c` is the town level per economy_residual; which levels are non-zero is G).
- Branch: spatial box query `0x46ff20` of half-size r*0x30 px when 4*r*r < object count
  `0x4cc3e8` (i.e. more than 0x40 (64) objects for r=4, 0x100 (256) for r=8), else the global
  object list `0x4cc3d0` with tile distance squared <= r*r.
- **Capturer candidate**: not allied (`0x4472e0` false), not airborne (`+0xa4`), tile distance
  squared <= 4 (objdump `0x45b36b`/`0x45b514`: `cmp ...,4; jg`), i.e. the hall tile, its 8
  neighbours and the 4 tiles two steps away orthogonally. The last one scanned wins.
  Distance uses the object's tile (virtual `+0x88`).
- **Defender**: live (`+0x5c`), kind != 1 (any unit, boat or Johnka, including unarmed ones)
  or kind 1 with `+0xfc` true (Defence: towers and the Shipyard), tile distance squared <= r*r.
  Global branch: any allied side counts; spatial branch: only objects in the town side's own
  list (`side +0x678`). Ordinary buildings never defend.
- Capture runs when there is a candidate, no defender, and the object being built
  (`+0x110` slot 4) is not a Defence answering `+0xfc` true. The candidate must have a tactic
  (`+0x30` != 0, checked at the top of `0x45a920`).
- Alarm (computer towns only): any non-allied object within r whose tactic side is human and
  town `+0x180` < 0x10: `0x461a30(side)`, `0x45dd50`, `0x45a880` (cancel with refund),
  `0x45cf80` (empty ring), `+0x180` = 0x10 (16). Human-owned towns reset `+0x180` = 0 on a tick
  with no candidate.

### CaptureTown `0x45a920` (V)

1. Old-side receiving group = `0x4474d0(old side)`. For each town tactic member: kind 1
   (buildings, towers) gets virtual `+0x7c` (side change); any other member is AddMember'ed to
   that old-side group (slot `+0xd4`): **units in the town stay with the old owner**.
2. `0x45cf80` empties the waiting ring (nothing was paid), `0x45dd50` releases `+0x188`,
   repairer (`+0x128`) slot `+0x3c` resets the repair mode, `+0x184` = 0, `+0x180` = 0,
   `+0x16c` parent link reset to self (refcounts `+0xac`).
3. Refund `0x45a880` only when old owner is computer and capturer human.
4. Selection: if the town was selected, `0x4498a0` deselects; selected members have their
   `+0xf0`/`+0xf4` flag virtuals called per selection bytes `+0x94/+0x9c/+0x9d` (G: UI only).
5. Old owner human: message 0x7e (126) or race variant 0x1aa (426)/0x146 (326) selected by
   `0x4c4f3c`, `0x51543c` and town race `+0x164`; voice at the hall pixel, minimap ping
   `0x462510`.
6. Town removed from old side (`0x447220`), `+8` = `0x4dd2f4 + new*0x69c`, hall `+0x7c`, added to
   new side (`0x446fe0`), build tree slots `+0x1c`/`+0x20` rebound.
7. Item under construction (`+0x110`): building -> side change, keeps progress; unit -> timer
   cleared and the unit object destroyed (virtual `+0x128`), no refund.
8. Vtable slot 0 (G: refresh), BuildSpeed `+0x158` = 1, virtual `+0x10c(new side)`,
   `+0xb4` = -1, `0x411720(0x4dd300,...)` (G: minimap/fog refresh). New owner human: message
   0x7d (125) / 0x1a9 (425) / 0x145 (325) and ping.

Not transferred: side money, units of the town tactic, waiting queue. Destroyed: the unit
under production. Timers: none; capture happens on the first town update with the condition
true (G: every tick). No hold time.

## 3. Johnka (type 0x58 (88)) and tactic kind 4

- What (G from data): a scripted ship ("junk"): visual object with zero max HP getter,
  terrain-independent diagonal motion 5 px per axis per think (`0x42dce0`), weapon 54 volley
  (30 rockets, scatter 96, range 300, reload 30 ticks) against non-airborne targets
  (`0x42d950`). It is a machine (`+0xf0` true), kind 7, so it counts as a capture defender and is
  repairable by a workshop.
- Tactic kind 4 (vtable `0x492e90`, kind virtual `0x446310`): think slot `+0x138` =
  `0x42e0a0` (V). Byte `+0xf0` = active, byte `+0xf1` = requested enable (script). If active,
  request off and the leader (`+0x58`) has state `+0x60` == 1 (turn phase), the leader is
  deactivated through its `+0xdc(tile, 0)` and active = 0. If inactive and request on, the
  leader `+0xdc(tile, 1)` and active = 1. Tactic position `+0x10/+0x14` follows the leader
  tile every update; the common tactic update `0x454820` (targeting) runs only while active.
- Spawn: scenario loader `0x42f6e0`, map sections `[Johnka#]` with ScenarioID, Side, X, Y,
  Enabled, Up (V map text). **Only Mission 15 places Johnkas**: Johnka0 (ScenarioID 50) and
  Johnka1 (51), side 2, tiles (12,172) and (11,173), Enabled=0, Up=0. Missions 1, 3, 4, 5, 7, 9,
  11, 13, 14 carry only `Johnkas=0`. Enabling is by script (G: which trigger not read).

## 4. Healing and repair

- RepairUnits `0x45d4d0` runs once per town update (see section 2), so every tick (G for tick).
- Flags (objdump `0x45d509..0x45d5be`, V): workshop (types 0x14/0x33/0x4a = 20/51/74) ->
  local byte `esp+0x12` -> paired with **`+0xf0` (machines)**; stable (0xf/0x2c/0x4d =
  15/44/77) -> `esp+0x11` -> **`+0xf4` (animals)**; air facility (0x19/0x34/0x4f = 25/52/79) ->
  `esp+0x13` -> `+0xa4` (airborne). Nothing runs unless workshop or stable exists.
- Range: object count <= 0x100 (256): list scan, tile distance squared < 0x41 (65) from the town
  anchor, no airborne clause (decompile; G that the air flag is ignored here). Count > 256: box
  0x180 (384) px around the hall, airborne clause active. So soldiers are never serviced.
- `+0xd8(10)` effects (V): `0x402990` (Marine, vehicles, boat, Johnka, Bird, Zeppelin) sets
  `+0x54` = 10; generic update `0x43edc0` (slot `+0x120`) decrements `+0x54` once per update.
  Marine think `0x433db0` regenerates `HP += desc word 2` per think while HP < cap, cap = desc
  word 3 (natural threshold) when `+0x54` == 0, else desc word 1 (max HP). The window lasts 10
  ticks after the last service. For soldiers word 3 == word 1, so the window changes nothing;
  for animals (e.g. Elefant 14000 vs 28000) the stable lifts the cap to max. Vehicles: whether
  their think reads `+0x54` was not checked (G: same pattern).
- Defence and Headtruck `0x41cbe0`: +10 HP immediately, capped at descriptor word 3; Building
  `0x40fa50`: +10 HP capped at descriptor word 1 (`+0x64 +4`). With a workshop, towers and
  buildings in range gain 10 HP per tick (G: tick rate) on top of the credit repair queue.
- Repair queue: when the repairer list (`+0x128` slot `+0x40`) is empty, every live damaged kind 1
  member other than the hall (and not the object at `+0x110`) is queued (`+0x128` slot `+0x30`).

## 5. Globals and effect objects

- `0x4c12c0` (V): cleared at mission load `0x430f4f`; set to 1 only by `0x43a280` (called from
  the tick at `0x4581e8`) when countdown `0x49b368` reaches 0 after being positive. The only code
  writer of `0x49b368` is the mission load (`0x430f6d`, value -1, so no countdown) besides the
  decrement; data initial value 0xffffffff. Readers: Marine think `0x433df2` (Tuzem, kind 0x1d,
  state 4 dying: animation stops on a long frame), Marine death `0x43396e`, Bird `0x4079cd`
  (when set, clears its slot in object array `0x4cc3d4`). G: a "freeze dying corpses / end of
  game" latch armed by a code path not found (save data or an indirect write); in normal play
  it stays 0, so the Tuzem freeze never triggers.
- Effect objects are animated sprites from pool `0x4c1978` created by `0x4189d0(pos, set,
  seq, loop)` (V, Spider death `0x44afa3`, `0x44b018`). Sets from `sprites_mapping.json` (V):
  0x98 (152) 3 sequences of 11/10/10 frames, chosen rand%3, loop 0 = **explosion**;
  0x71 (113) 20 sequences of one frame, rand%20, position variant -1 = **wreck/debris** still;
  0x69 (105) 1 sequence of 30 frames = **smoke puff** (tower damage sparks, Cannon smoke).
  Names are G; they are not spawners.

## 6. Hypno school, hypno tower, Hypno and secondary HP

- Build tree (V `buildtree.csv`, race 2 Chinese): school 72 is a constructible building
  (flagB 1) requiring barracks 67; Hypno 54 and hypno tower 69 require 67 and school 72 (flag A
  0x8000000). No other code queries type 0x48 (corpus). Role: prerequisite only.
- Hypno predicate `+0xf8` = desc byte `+0x2e`, set only for Marine param 0 (Hypno) (V d11 =
  0x10101). Kill-time estimate `0x46fac0` uses the target's secondary HP (d4) instead of current
  HP when the attacker is a Hypno; damage still goes to primary HP only (O, hypno_recon). For
  animals d4 equals max (Elefant 28000, Bear 36000) while the regen threshold is lower, so a
  Hypno overestimates kill time against animals (G: affects target choice only). No conversion.

## Unknowns

| Item | Kind | How |
|---|---|---|
| Tick function is one pass per simulation tick | G | static read of the caller of `0x457f..` or live counter |
| Which town levels set `+0x15c` (8-tile radius) | G | static `0x45b5f0`/upgrade writer, then live |
| Small-pool RepairUnits ignores airborne | G | objdump `0x45d6ac..0x45d766` |
| Vehicle think uses `+0x54` window | G | read Cannon/Spider/Truck thinks |
| Writer that arms `0x49b368` | open | search indirect writes, save loader |
| Johnka enable trigger in Mission 15 script | open | read `Mission_15.scn` |
| Capture timing and alarm behaviour | needs live run | mission 7 outpost, human vs computer town |
| Stable heal of animals to max; tower +10 HP per tick | needs live run | park a damaged Knight/tower near a stable/workshop |

# AI tables, kill time and the light map (numeric leftovers)

V = read in the corpus, disassembly or data; G = inference. All addresses are Aztec.exe.
Builds on `docs/ai_scoring_threat.md`, `docs/fog_sight_minimap.md`, `docs/weapon_class_map.md`,
`docs/economy_rules.md` section 3.3.

## 1. Kill time `0x46fac0(attacker, victim)` ("wep_CalcKillTime")

V, whole function:

1. `w = attacker->vfunc+0xa8(victim)` (weapon chosen for this target; weapon table `0x515ac0`).
   `w == NULL` -> return **-1**.
2. `dmg = w->vfunc+0x5c(attacker, victim)` (per hit damage against this victim);
   `reload = w->vfunc+0x60()`. `dmg == 0` -> return **-1**.
3. `hp = attacker->vfunc+0xf8() ? victim->vfunc+0x78() : victim->vfunc+0x38()`.
   +0xf8 is the Hypno predicate (Marine `0x433000`), +0x78 the secondary "mind" points
   (Marine descriptor d4, object +0x3c), +0x38 the current hit points.
4. `hits = (hp - 1 + dmg) / dmg` (signed C division, i.e. ceil for positive values);
   `hits == 0` -> 1.
5. return `hits * reload` (ticks).

- Armor and category factors are not in `0x46fac0`; they are inside the damage getter +0x5c
  of each weapon class (V, see `weapon_class_map.md`: e.g. arrow `0x403d00` = base part
  `0x403da0` (victim armor `+0x10c`, airborne test `+0xa4`) + `0x403e10` multipliers +
  optional chained weapon `rec[0x30]`; class 9 `0x444160`; melee `0x46e580` 3000/1500).
- Impossible matchups: no weapon or zero damage -> -1. Wrapper `0x44df10` (V) turns -1 into
  **0x400 (1024)** and otherwise returns `kill_time + 1`.
- Edge cases (V arithmetic): hp <= 0 gives hits = 1, so the result is `reload`. Building
  current HP `0x40f9b0` reports 0 unless object state `+0x68 == 1` (G: whether a fresh
  prototype has state 1 is not checked; if not, every building victim costs one reload).
  Negative damage is not guarded.
- Sibling `0x46fbd0` ("wep_CalcKillTimes", V): same formula for an area weapon, list from
  weapon +0x64 (capacity 0x80 = 128 targets), entries with 0 damage set to -1.

## 2. Tables of `0x44dc10`

Built **once per process** (V): `0x44dbe0` (side AI update) calls it only while byte
`0x4d6c0c == 0` and sets the byte to 1; no other write of `0x4d6c0c` exists in the corpus.
So the tables are never rebuilt (not per mission, not on load). G: harmless only if type
descriptors (HP, weapons) do not change between missions.

Prototypes: one object per type 0..89 from `0x43e0b0(type)`, kept on the stack, destroyed
(virtual +4) after the weight pass. All arithmetic is 8.8 fixed point (`0x402fb0` = x<<8,
`0x403020`/`0x44df50` = (a<<8)/b in 64 bit signed, truncating).

### Matrix M at `0x4ced7c`, index `a*90 + b` (8100 = 0x1fa4 dwords), V
Loop a = 89..0 (attacker prototype), b = 89..0 (victim prototype):
- attacker kind (virtual +8) == 1 (any building, defence towers included): **raw 1**;
- else `k = 0x44df10(a, b)`; `k <= 0x100 (256)`: **raw = 256 - k** (k = 256 gives 0);
- else (k > 256, including 1024 = impossible): **raw 1** if victim kind == 1, else **0**.
Value meaning: (256 - k)/256, i.e. "fraction of 256 ticks left after killing one b".

### Weight W at `0x4cec14` (90 dwords, W[89] at `0x4ced78`), V
- First pass, t = 89..0: `W[t] = maxHP(t) << 8` with `maxHP = proto->vfunc+0x3c()`;
  `max = max over t` (start 0, comparison `max <= v`).
- Second pass: `W[t] = (W[t] << 8) / (max << 8)` = **floor(256 * maxHP(t) / max)**, raw 8.8,
  range 0..256 (the toughest type is exactly 1.0). Division by zero if every type had 0 HP.

### Per development level rows at `0x4cdc70`, V
- 11 rows (L = 0..10) of 0x5b (91) dwords: dword 0 = count, then type ids. Whole block
  0x3e9 (1001) dwords zeroed first.
- Source order: the non-building list `0x4cd6d0` (filled with type 89 first, so it is
  89..0) walked from its **last** entry back to the first, i.e. types in increasing id order.
- Type t goes into every row L >= `0x45c920(t)` (= `DevLevel` table `0x515580`), only when
  that level is < 11; a 90 x 11 byte mark table prevents duplicates. Unavailable types have
  DevLevel 10 (rules_types.md), so they are only in row 10.
- Building lists from the same function (V): defence `0x4cd838` (count `0x4cd6c0`,
  virtual +0xfc true); buildings with descriptor `+0x100 != -1` or type 0x12/0x28/0x46
  (18/40/70) -> `0x4cd9a0` (count `0x4cd6c4`); others -> `0x4cdb08` (count `0x4cd6c8`).

## 3. From plan entries to trained types

- No translation table (V): plan entries `{kind 1, type}` (`0x456f20`) hold the object type
  id 0..89, the same id space as the build tree (`0x4af130`) and the 0x4cdc70 rows. Candidate
  choice is in `0x44cd30` (ai_scoring_threat.md section 3).
- Build test = town build tree (town +0xf4, class vtable `0x48ffb4`) slot +4 = `0x4119b0`
  `CanBuild(type, out)` (V): searches the current availability list (tree +0x2e8, entries
  of 0x88 bytes = 34 dwords: type, missing prerequisites..., -1). True when the type is
  listed and its missing list is empty. `out` (optional) receives the entry; a type not
  listed gives `{type, 0x5a, -1}`. The `0x44cd30` test "out index < 90" therefore means
  "listed but blocked by a real missing prerequisite" -> remembered as wanted type and
  passed to town virtual +0x15c (G: recommendation +0x18c used by phase 10).
- Availability rebuild `0x411ac0` (V), triggered through tree virtual +0x28 by:
  slot +0 `0x411990` (queued[type]++, rebuild when it went 0 -> 1), slot +8 `0x411a80`
  (queued[type]--, assert when already 0, rebuild), slot +0x2c `0x411db0` (byte +0x2e1 := 1),
  slot +0x30 `0x411da0` (byte +0x2e1 := 0).
- Correction to economy_rules.md 3.3 (V from `0x411ac0`): with +0x2e1 == 0 every listed
  type gets missing list `{0x5a, -1}`, so CanBuild is **false for all types** (not "all
  available"). Admission `0x45cec0` sets +0x2e1 = 0 when the admitted item is a hall upgrade
  (`0x45c4f0 != -1`); removal `0x45cfd0` sets it back to 1. G: the town builds nothing else
  while a hall upgrade is queued.
- Admission `0x45cec0(town, type)` (V): CanBuild(type, NULL) -> create object `0x43d5d0`
  -> ring (town +0xf8) virtual +8 insert; ring full -> object destroyed, NULL. On success:
  tree +0 (queued++), and for a non-defence building town `+0x17c := -1`.
- Caps per type (V): only the build tree's one-per-town flag 0x4000000 (marker -2 completed,
  -3 queued) and DisabledID. No per-type count cap in the plan or admission. Other limits:
  plan 0x100 (256) entries, ring 50, intelligence own counts saturate at 0x20 (32) per type,
  batch `rand() % (L+1) + 1`.

## 4. Light: who writes the side map above 0x20

Per tick in `0x4577b0` (fog_sight_minimap.md section 2). Writers, V:
- `0x47d460` -> `0x480f07`: clamp side map to <= 0x20 (explored).
- `0x47d070(map, x, y, id)`: loads mask `id` (type 0x455) from `res\light.xbe` and stamps it
  into the side's darkness map with per cell **minimum** (darkness starts at 0x98).
- `0x47d430(side, ambient, darkness)` -> `0x480d20` (asm, V): for each byte
  `v = ambient - darkness`; if no borrow and `v >= side`, `side := v`. This is the only
  writer of values above 0x20.
- Ambient = `daylight << 3` (`0x47c980`), plus additive glow masks (`0x47cb30`, cap 0x98).

### Mask archive `Res/light.xbe` (V, parsed: one group 0x455, ids 1..71, all 20 x 20 bytes)
- Ids 1..16: glow masks, 0 outside, centre (10,10) peak 10, 19, 29, ... 152 (about 9.5 * id).
- Ids 17..64: identical sight mask (48 copies). Ids 65..71: larger sight masks.
- All sight masks: border 0x98 (95 cells), an octagon of 0x60 (96) out to radius 9, and a core
  falling to 0. Centre index (10,10); cells with darkness < 0x98: 305 for every id.
- Radius along row 10 (cells from the centre) and cell counts:

| id | darkness < 0x10 | < 0x40 | < 0x60 |
|---|---|---|---|
| 17..64 | r2, 21 | r3, 37 | r3, 45 |
| 65 | r3, 37 | r3, 45 | r4, 69 |
| 66 | r3, 45 | r4, 69 | r5, 89 |
| 67 | r4, 61 | r5, 89 | r5, 109 |
| 68 | r4, 69 | r5, 109 | r6, 145 |
| 69 | r5, 97 | r6, 137 | r7, 177 |
| 70 | r5, 109 | r7, 177 | r8, 213 |
| 71 | r6, 137 | r7, 193 | r8, 249 |

Consequence (V arithmetic, test "> 0x20" from `0x402490`/`0x410340`): visible needs
`ambient + glow - darkness > 0x20`. At full day ambient = 12*8 = 0x60, so the core
(darkness < 0x40) is lit; the 0x60 ring gives 0 without glow but lets glowing objects
(up to +0x38) show through; outside the octagon (0x98) nothing. At night ambient = 6*8 = 0x30,
only darkness < 0x10 is lit. G: the mask origin (pos + cell - 10 cells) puts the centre one
cell off unless the light map has a one cell border.

### Daylight and time of day `0x4c1968` (V)
- Range 0..0x1c1f (7199); period **7200 (0x1c20) ticks**. `0x4184a0` adds 1 per simulation
  tick (called once in the tick block of `0x4577b0`); at 7200 it wraps to 0 and segment 0.
- Start: `0x418380` reads `StartTime` from the mission ini (negative -> 0, > 7198 -> 7199).
  Load: `0x418650` reads time and segment index `0x4c196c` from the save.
- Keyframes: 9 dword records, table `0x4a8f68` when `Globals/Tileset` (`0x4dd2e8`) is 0,
  else `0x4a9338`. Record = end time, 3 doubles (G: RGB palette scale for `0x4786f0`),
  daylight level (-> `0x5153f8`), a value -> `0x5153fc` (G: shading class 3/2/1).
  Step function (no interpolation): segment advances when time reaches its end time.
- Levels (table 0x4a8f68, end times): 6 until 800, 7 to 1000, 8 to 1200, 9 to 1400, 10 to
  1600, 11 to 1800, 12 from 1800 to 5400, then 11 to 5600, 10 to 5800, 9 to 6000, 8 to 6200,
  7 to 6400, 6 to 7200. Table 0x4a9338: same levels, dawn shifted +400 (day 12 from 2200 to
  5000) and dusk shifted -400.
- Clock display: `time / 0x78 (120)` in the panel draw `0x469e50` (V), 60 steps per day (G: clock icon index).

## 5. Unknowns
- Whether a fresh building prototype reports nonzero HP (state +0x68) in the M build.
- Victim +0x78 for non-Marine classes (Hypno attacker against buildings or vehicles).
- Exact meaning of the record doubles and of `0x5153fc`; light map border offset.
- Which object classes use sight ids 65..71 (descriptor values not tabulated here).
- Whether anything resets `0x4d6c0c` outside the corpus (e.g. hand-written asm).

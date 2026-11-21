# Attack and walk animation timing (static dump)

Sources: Units.xbe group 0x515 Anim items (header 0x208, slot table at +8, 32-byte frame records,
the terminating record has sprite 0). The type, anim, weapon and reload values come from
`docs/data/rules.json`. Class 12 reload is taken from component `rec[0]`, and class 8 reload from `rec[3]`.
Raw per-sequence dumps are in `work/gaps/k2/anim_atk.txt` and `anim_walk.txt`. Ticks are logic ticks.

## Global findings (V, data)

- Every attack sequence of every armed type has exactly **one** marker frame. The byte at +0xc is 1,
  and that frame always lasts **1 tick**. So each attack animation fires one shot, and the multi-shot risk
  named in `unit_state_machines.md` does not occur in the shipped data.
- There are no hold frames (duration 0) in any attack sequence. All 8 (Marine, Truck, Cannon) or 32
  (turret) facing sequences are present for every armed type.
- The Marine walk sequences 1..8 and attack sequences 9..16 have dx/dy = 0 on every frame. The only
  exception is the Hypno attack, which has non-zero per-frame offsets that sum to 0 over the sequence.
  Marine movement therefore comes only from the descriptor speed (`+0x14`, px/tick). The death
  sequences 17..19 of several Marines do carry a net displacement of up to about 15 px.
- Cannon (Hopcannon, anim 28) move sequences 1..8 are 8 frames of 1 tick each, and each sums to
  exactly 48 px per axis (one tile: (48,0), (48,-48), (0,-48) ...). So one tile step takes 8 ticks.
  Sequence 1 has the frame order of the other seven rotated by one: its 2 px frame is first, while in
  the others it is last.

## Timing rules (V, code)

The animation advance `0x403460` adds 1 to the tick, and when tick >= dur it moves to the next record.
In a non-looping sequence the end gives a null record, or sentinel `0x4c1300`. L below is the sum of the
frame durations. "off" is the tick of the shot relative to the entry tick T.

| class | think | where the marker is tested in the entry tick | shot tick | period |
|---|---|---|---|---|
| Marine | `0x433db0` | entry is in the prelude (state=3, seq facing+9, reload=r+1), then `case 3` tests frame 0 in the same tick; later ticks advance first (top of think), then test | T+off | max(r+1, L+1). The sequence end sets state 1 inside case 3, after that tick's entry check |
| Spider / Headtruck turret | `0x44b360` / `0x42c880` | entry (phase 1, seq dir32+2, reload=r), then the phase-1 block tests frame 0 in the same tick, then advances | T+off | max(r, L) |
| Defence | `0x41d790` | entry and phase-1 handling are if/else, so the first test is at T+1 | T+1+off | max(r, L+1) |
| Truck (Catapult) | `0x460a10`, anim `0x4607b0` | state 3 -> 4 starts seq dir8+9, and the state-4 block tests in the same tick | T+off | G: about max(r, L) plus turn ticks |
| Cannon (Hopcannon) | `0x417090` | case 2 starts seq dir8+9 (state 4), case 4 tests from the next tick on | T+1+off | G: about r + 1..2 (end goes to 1 or 2, then state 2 is re-entered before state 4) |

## Per type table

Columns: type id, name (UGI id only), animation, sequence set, marker frame index / tick offset
"off" (offset from the sequence start), L, reload r, period. Where facings differ, the main variant is
listed first.

### Marine (seq = facing+9, 9..16)

| id | name | anim | weapon | marker idx / off | L | r | period |
|---|---|---|---|---|---|---|---|
| 0, 36 | FOOTMAN, SPIDER_AXETHROWER | 4 | 4 | 2/4 (seq 9,10,13-16); 3/5 (seq 11,12) | 7 / 8 | 9 | 10 |
| 1 | RIFLEMAN | 18 | 9 | 3/5; 4/6 (seq 11) | 10 | 20 | 21 |
| 2 | DEMOLISHER | 25 | 38 | 7/7 (seq 10,12,13,14,16); 8/8 (seq 9,11,15) | 12 | 30 | 31 |
| 5 | BEAR | 15 | 10 | 3/6 (seq 9,13: L14; seq 10,16: L12); 4/8 (seq 11,12,14,15: L14) | 12-14 | 16 | 17 |
| 6 | KNIGHT | 12 | 8 | 12/12 | 19 | 19 | 20 (anim and reload equal) |
| 27 | ARCHER | 19 | 14 | 4/6 | 11 | 12 | 13 |
| 28 | MAGE | 16 | 24 | 1/1 | 5 | 20 | 21 |
| 29 | TUZEM | 2 | 31 | 2/4 | 7 | 27 | 28 |
| 31 | JAGUAR | 21 | 11 | 2/4 (seq 10-16, L6); 3/5 (seq 9, L7) | 6-7 | 11 | 12 |
| 32 | BIZON | 24 | 33 | 2/4 | 6 | 33 | 34 |
| 53 | NINJA | 3 | 13 | 1/2 | 3 | 19 | 20 |
| 54 | HYPNO | 1 | 56 | 7/14 | 30 | 20 | **31 (animation-bound)** |
| 55 | CHIEF | 30 | 55 | 8/8 | 11 | 6 | **12 (animation-bound)** |
| 56 | SNOWMAN | 7 | 16 | 4/4 | 9 | 25 | 26 |
| 57 | ELEFANT | 20 | 47 (c12 -> 46) | 1/1 | 15 | 40 | 41 |
| 82 | ABDULA | 8 | 59 | 2/3 (seq 9,12,13,14,16, L4); 1/2 (seq 10, L3); 2/3 (seq 11, L5); 3/3 (seq 15, L5) | 3-5 | 11 | 12 |

### Spider and Headtruck turrets (seq = dir32+2, 2..33; all 32 present)

| id | name | turret anim | weapon | marker idx / off | L | r | period |
|---|---|---|---|---|---|---|---|
| 30 | SPIDER_CROSSBOW | 50 | 1 | 0/0 | 1 | 65 | 65 |
| 35 | SPIDER_FLAMETHROWER | 54 | 34 | 0/0 (frames 1+10) | 11 | 40 | 40 |
| 3, 61 | TRUCK_AXETHROWER, WHEEL_AXETHROWER | 51 | 7 (c12 -> 5) | 0/0 | 1 | 35 | 35 |
| 4 | CANNON | 52 | 39 (c12 -> 40) | 0/0 | 3 | 45 | 45 |
| 8 | TRUCK_FLAMETHROWER | 50 | 58 | 0/0 | 1 | 65 | 65 |
| 9, 58 | TRUCK_KATUSHA, WHEEL_KATUSHA | 53 | 19 (c8) | 0/0 | 1 | 80 | 80 |
| 59 | WHEEL_FIRETHROWER | 54 | 34 | 0/0 (frames 1+10) | 11 | 40 | 40 |
| 62 | WHEEL_CROSSBOW | 50 | 48 | 0/0 | 1 | 75 | 75 |

### Defence (turretless: base anim seq 8; turret: seq ((h+4)>>3)+2)

| id | name | anim / seq | weapon | marker idx / off | L | r | period |
|---|---|---|---|---|---|---|---|
| 17 | RUS_WATCH_TOWER | 215 / 8 | 15 | 0/0 (shot at T+1) | 1 | 13 | 13 |
| 23 | RUS_FORTRESS_TOWER | turret 51 / 2..33 | 7 | 0/0 | 1 | 35 | 35 |
| 26 | RUS_CANNON_TOWER | turret 52 / 2..33 | 51 (c12 -> 50) | 0/0 | 3 | 45 | 45 |
| 42 | AZT_ARCHER_TOWER | 314 / 8 | 14 | 0/0 | 1 | 12 | 12 |
| 49 | AZT_CROSSBOW_TOWER | 315 / 8 | 28 | 0/0 | 1 | 20 | 20 |
| 69 | CHI_HYPNO_TOWER | 414 / 8 | 57 | 0/0 | 1 | 10 | 10 |
| 81 | CHI_AIRTOWER | turret 55 / 2,6,..,30 (8 headings, step 32) | 20 (c8) | 0/0 | 1 | 50 | 50 |

### Truck and Cannon classes (seq = dir8+9)

| id | name | anim | weapon | marker idx / off | L | r | period |
|---|---|---|---|---|---|---|---|
| 83 | CATAPULT (Truck class) | 10 | 32 | 1/1 (durs 1,1,3,2) | 7 | 30 | G: about 30 plus turn |
| 87 | HOPCANNON (Cannon class) | 28 | 2 | 0/0 (shot at T+1) | 3 | 37 | G: 38-39 |

Only HYPNO and CHIEF are limited by their animation. KNIGHT is tied: anim and reload are equal. All
others are reload-bound. For turrets and towers the period equals the weapon reload.

## Marine walk cycle (seq facing+1, looping; per-frame dx/dy = 0 on all frames and facings)

| id | name | frames x dur | cycle ticks | descriptor speed px/tick | px per cycle |
|---|---|---|---|---|---|
| 0, 36 | FOOTMAN, SPIDER_AXETHROWER | 6x1 | 6 | 9 | 54 |
| 1 | RIFLEMAN | 6x1 | 6 | 6 | 36 |
| 2 | DEMOLISHER | 6x1 | 6 | 7 | 42 |
| 5 | BEAR | 4x2 | 8 | 5 | 40 |
| 6 | KNIGHT | 8x1 | 8 | 15 | 120 |
| 27 | ARCHER | 6x1 | 6 | 8 | 48 |
| 28 | MAGE | 6x1 | 6 | 5 | 30 |
| 29 | TUZEM | 6x1 | 6 | 10 | 60 |
| 31 | JAGUAR | 5x1 | 5 | 16 | 80 |
| 32 | BIZON | 5x1 | 5 | 12 | 60 |
| 53 | NINJA | 4x1 | 4 | 11 | 44 |
| 54 | HYPNO | 6x1 | 6 | 10 | 60 |
| 55 | CHIEF | 8x1 | 8 | 10 | 80 |
| 56 | SNOWMAN | 12x1 | 12 | 4 | 48 |
| 57 | ELEFANT | 10x1 | 10 | 4 | 40 |
| 82 | ABDULA | 6x1 | 6 | 10 | 60 |

The cycle is the same for all 8 facings of each type. The animation does not drive Marine movement, so
the real px/tick equals the descriptor speed (diagonal handling is covered by the velocity code in
`0x433db0`, not by the animation).

## Functions (all V, read in `all_full.c`)

`0x403460` advance, `0x4034c0` start, `0x403510` switch keeping the frame, `0x4033c0` seek;
`0x433db0` Marine (prelude entry and case 3); `0x44b360` Spider; `0x42c880` Headtruck;
`0x41d790` Defence; `0x460a10` and `0x4607b0` Truck; `0x417090` Cannon; `0x46d900` weapon factory.

## Unknowns

- Truck and Cannon: the exact period, which depends on the turn and state 1/2 re-entry ticks.
  Needs a trace (G).
- All periods assume the fire target is still present when reload reaches 0. The Marine target is
  re-requested by tactic `+0x120` in the same prelude, and the turret target is released at the
  sequence end (early re-request window). If Local_assign is delayed, the period grows.
- Turret classes require an exact bearing. A moving target can add turn ticks.
- UGI_CHI_FIRETHROW_TOWER is listed as kind Building (param 16) in `type_anims.hpp` and is not
  covered here.
- Live confirmation of the ±1 offsets (Marine r+1 versus L+1, Defence T+1) has not been done.

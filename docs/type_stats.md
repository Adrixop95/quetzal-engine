# Type statistics (from observing live objects of a test map)

Source: test map with one unit of each type (`tools/ztest_run.sh`). `hp_max` = object field `+0x3c`. For class Marine (kind 0) the type description record lies at
`0x4ac430 + parameter*0x4c` (parameter = second column of the type table); the constructor `0x433190` sets: `object+0x7c = Anim(animation id = record[0])`, `object+0x38 = record[1]` (starting health),
`object+0x3c = record[4]` (maximum health). The other record fields (19 dwords): [2], [5], [6], [7] numbers of undetermined meaning, [8..10] = -1, [11] flags 0x101/0x10101, [12..15] sizes 0x40..0x100, [16..18] = 0, 1, pointer.

| id | type | class | parameter | hp_max | Marine record (hex dwords) |
|---|---|---|---|---|---|
| 0 | FOOTMAN | Marine | 3 | 6000 | 4 1770 8 1770 1770 9 40 4 ffffffff ffffffff ffffffff 101 100 100 100 2 0 1 |
| 1 | RIFLEMAN | Marine | 9 | 5000 | 12 1388 8 1388 1388 6 44 9 ffffffff ffffffff ffffffff 101 100 100 100 2 0 1 |
| 2 | DEMOLISHER | Marine | 14 | 8500 | 19 2134 e 2134 2134 7 44 26 ffffffff ffffffff ffffffff 101 50 100 100 2 0 1 |
| 3 | TRUCK_AXETHROWER | Headtruck | 1 | 20000 |  |
| 4 | CANNON | Headtruck | 2 | 15000 |  |
| 5 | BEAR | Marine | 7 | 36000 | f 9c40 2d 4e20 8ca0 5 41 a ffffffff ffffffff ffffffff 1 100 80 80 3 0 0 |
| 6 | KNIGHT | Marine | 6 | 10000 | c 2ee0 16 2710 2710 f 43 8 ffffffff ffffffff ffffffff 0 100 100 100 0 0 0 |
| 7 | ZEPPELIN | Zeppelin | 0 | 3000 |  |
| 8 | TRUCK_FLAMETHROWER | Headtruck | 0 | 17500 |  |
| 9 | TRUCK_KATUSHA | Headtruck | 6 | 16000 |  |
| 10 | RUS_FORPOST | Building | 0 | 0 |  |
| 11 | RUS_VILLAGE | Building | 10 | 0 |  |
| 12 | RUS_FARM | Building | 7 | 0 |  |
| 13 | RUS_MINE | Building | 8 | 0 |  |
| 14 | RUS_BARRACKS | Building | 5 | 0 |  |
| 15 | RUS_STABLE | Building | 9 | 0 |  |
| 16 | RUS_BEARLAIR | Building | 6 | 0 |  |
| 17 | RUS_WATCH_TOWER | Defence | 3 | 0 |  |
| 18 | RUS_WAREHOUSE | Building | 11 | 0 |  |
| 19 | RUS_TOWN | Building | 1 | 0 |  |
| 20 | RUS_WORKSHOP | Building | 4 | 0 |  |
| 21 | RUS_ARMORY | Building | 28 | 0 |  |
| 22 | RUS_LABS | Building | 29 | 0 |  |
| 23 | RUS_FORTRESS_TOWER | Defence | 0 | 0 |  |
| 24 | RUS_FORTRESS | Building | 2 | 0 |  |
| 25 | RUS_AIR_FACILITY | Building | 3 | 0 |  |
| 26 | RUS_CANNON_TOWER | Defence | 1 | 0 |  |
| 27 | ARCHER | Marine | 10 | 3500 | 13 dac 8 dac dac 8 42 e ffffffff ffffffff ffffffff 101 100 100 100 2 0 1 |
| 28 | MAGE | Marine | 8 | 6000 | 10 1770 c 1770 1770 5 43 18 ffffffff ffffffff ffffffff 101 40 100 80 7 0 1 |
| 29 | TUZEM | Marine | 1 | 2600 | 2 a28 c a28 a28 a 42 1f ffffffff ffffffff ffffffff 101 100 40 100 2 0 1 |
| 30 | SPIDER_CROSSBOW | Spider | 0 | 26000 |  |
| 31 | JAGUAR | Marine | 12 | 23000 | 15 59d8 a 2cec 59d8 10 43 b ffffffff ffffffff ffffffff 1 100 100 100 5 0 0 |
| 32 | BIZON | Marine | 13 | 23000 | 18 32c8 f 1964 59d8 c 44 21 ffffffff ffffffff ffffffff 0 100 180 100 0 0 0 |
| 33 | EAGLE | Bird | 2 | 8000 |  |
| 34 | DELTAPLAN | Bird | 1 | 12000 |  |
| 35 | SPIDER_FLAMETHROWER | Spider | 1 | 28000 |  |
| 36 | SPIDER_AXETHROWER | Marine | 3 | 6000 | 4 1770 8 1770 1770 9 40 4 ffffffff ffffffff ffffffff 101 100 100 100 2 0 1 |
| 37 | AZT_FORPOST | Building | 12 | 0 |  |
| 38 | AZT_VILLAGE | Building | 13 | 0 |  |
| 39 | AZT_FARM | Building | 20 | 0 |  |
| 40 | AZT_WAREHOUSE | Building | 26 | 0 |  |
| 41 | AZT_BARRACKS | Building | 19 | 0 |  |
| 42 | AZT_ARCHER_TOWER | Defence | 4 | 0 |  |
| 43 | AZT_HUT | Building | 31 | 0 |  |
| 44 | AZT_STABLE | Building | 24 | 0 |  |
| 45 | AZT_JAGUARLAIR | Building | 21 | 0 |  |
| 46 | AZT_NEST | Building | 23 | 0 |  |
| 47 | AZT_TOWN | Building | 14 | 0 |  |
| 48 | AZT_MINE | Building | 22 | 0 |  |
| 49 | AZT_CROSSBOW_TOWER | Defence | 5 | 0 |  |
| 50 | AZT_TEMPLE | Building | 25 | 0 |  |
| 51 | AZT_WORKSHOP | Building | 27 | 0 |  |
| 52 | AZT_AIR_FACILITY | Building | 18 | 0 |  |
| 53 | NINJA | Marine | 2 | 7500 | 3 1d4c a 1d4c 1d4c b 42 d ffffffff ffffffff ffffffff 101 100 80 100 2 0 1 |
| 54 | HYPNO | Marine | 0 | 6000 | 1 1770 c 1770 1770 a 45 38 ffffffff ffffffff ffffffff 10101 40 40 0 7 0 1 |
| 55 | CHIEF | Marine | 15 | 5000 | 1e 1388 19 1388 1388 a 44 37 ffffffff ffffffff ffffffff 101 80 80 100 2 0 1 |
| 56 | SNOWMAN | Marine | 4 | 22500 | 7 afc8 28 57e4 57e4 4 42 10 ffffffff ffffffff ffffffff 0 80 40 80 6 0 0 |
| 57 | ELEFANT | Marine | 11 | 28000 | 14 6d60 18 36b0 6d60 4 44 2f ffffffff ffffffff ffffffff 0 100 80 100 4 0 0 |
| 58 | WHEEL_KATUSHA | Headtruck | 5 | 14000 |  |
| 59 | WHEEL_FIRETHROWER | Headtruck | 7 | 16000 |  |
| 60 | PARAPLAN | Bird | 0 | 7000 |  |
| 61 | WHEEL_AXETHROWER | Headtruck | 3 | 16000 |  |
| 62 | WHEEL_CROSSBOW | Headtruck | 4 | 15000 |  |
| 63 | CHI_FORPOST | Building | 15 | 0 |  |
| 64 | CHI_VILLAGE | Building | 38 | 0 |  |
| 65 | CHI_FARM | Building | 35 | 0 |  |
| 66 | CHI_MINE | Building | 36 | 0 |  |
| 67 | CHI_BARRACKS | Building | 33 | 0 |  |
| 68 | CHI_FIRETHROW_TOWER | Building | 16 | 0 |  |
| 69 | CHI_HYPNO_TOWER | Defence | 8 | 0 |  |
| 70 | CHI_WAREHOUSE | Building | 39 | 0 |  |
| 71 | CHI_TOWN | Building | 16 | 0 |  |
| 72 | CHI_HYPNO_SCHOOL | Building | 42 | 0 |  |
| 73 | CHI_PRISON | Building | 30 | 0 |  |
| 74 | CHI_WORKSHOP | Building | 40 | 0 |  |
| 75 | CHI_ARMORY | Building | 41 | 0 |  |
| 76 | CHI_FORTRESS | Building | 17 | 0 |  |
| 77 | CHI_STABLE | Building | 37 | 0 |  |
| 78 | CHI_CAVE | Building | 34 | 0 |  |
| 79 | CHI_AIR_FACILITY | Building | 32 | 0 |  |
| 80 | CHI_WATCHTOWER | Defence | 2 | 0 |  |
| 81 | CHI_AIRTOWER | Defence | 7 | 0 |  |
| 82 | ABDULA | Marine | 5 | 3000 | 8 2ee0 14 2ee0 bb8 a 47 3b ffffffff ffffffff ffffffff 101 100 100 100 2 0 1 |
| 83 | CATAPULT | Truck | 0 | 3000 |  |
| 85 | ZEPPELIN2 | Zeppelin | 1 | 3000 |  |
| 86 | SHIPYARD | Defence | 6 | 0 |  |
| 87 | HOPCANNON | Cannon | 0 | 28000 |  |

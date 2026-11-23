# Gap reconnaissance (2026-11-08)

Evidence labels (audit 2026-11-23): V = inspected source fact, with reference
or clone scope stated; M = measurement rechecked from retained captures or an
offline clone probe; G = inference, proposed procedure or uncorroborated
historical report. A clone V does not establish native parity. A marker covers
one paragraph/list item or the entire immediately following table. Unresolved
meanings remain G even inside a table whose codec structure is V.
See [label_audit.md](label_audit.md) for counts and evidence limits.

**G** (historical report). Historical inventory from 2026-11-08. Current status is in [engine_rules_status.md](engine_rules_status.md), [specials_completion_matrix.md](specials_completion_matrix.md) and the linked implementation reports. The rank table below records investigation leads, not current missing-feature assertions.

**G** (historical report). Purpose: find mechanics that are unclear, approximated or missing in the clone and that
the campaign actually reaches, so each can be resolved the way the ferry was (live
measurement of the original plus the lifted code). Method: three read-only passes over
the documentation (about 960 raw open or assumed items, many repeated), cross-checked
against which object types the 15 mission files place, and for which side.
Only mechanics and mission numbers are listed here; no object counts or map data.

## Documentation state

- **G** (historical report). `docs/ROADMAP.md` and parts of `docs/engine_rules_status.md` are stale: they still say
  the 19 transport script sites are unsupported, which the campaign sweep disproved.
  They need a refresh from the current state (one dedicated task, not a side effect of
  other tasks).
- Most open rows in `docs/parity.md` and `docs/parity_matrix.md` are measurement rows that
  failed earlier for environment reasons (network namespace, loopback); the live harness
  has since worked, so these should be re-run rather than treated as unknown mechanics.

## Mechanics that the campaign reaches and the clone does not fully model

Ranked by how directly they can block a mission. "Enemy" means the opponent owns them.

**G** (historical report).

| Rank | Mechanic | Missions | Status in the clone (from the docs) | How to resolve |
| --- | --- | --- | --- | --- |
| 1 | Hypnosis: resolved for the Hypno unit (plain ranged damage, no ownership transfer, see hypno_recon.md); the hypno tower and school still need a check | 7, 9, 11, 12, 15 | Hypno weapon verified from the decompilation | check the tower and school classes the same way |
| 2 | Air units: Eagle, Deltaplan, Paraplan, Zeppelin, Zeppelin2 (both sides), air facilities and air towers | 4, 5, 7, 8, 11 to 14 | speeds default to 8 px per tick, weapon fields of Bird and Zeppelin unresolved, altitude open | who can shoot air units, speed, weapons, bombing; live observation plus lifted code |
| 3 | Siege and special weapons: Cannon, Hopcannon, Katusha trucks and wheels, Demolisher, Catapult | 1, 6, 8 to 10, 12 to 15 (player owned in 6, 13, 14, 15) | siege approximations, hit timing and splash unmeasured | measure range, damage, splash, reload on a few targets |
| 4 | Fire weapons: flamethrower trucks, Spider flamethrower, wheel firethrower, Chinese firethrow tower | 12, 14, 15 | "standalone FireThrow and burn scheduling remain disabled" | measure burn damage over time and what burns |
| 5 | Defence towers (watch, fortress, cannon, archer, crossbow, hypno, air tower) | most missions | "dedicated reference tactic behaviour remains unimplemented" | live observation of target selection and range |
| 6 | Special infantry: Mage, Ninja, Chief, Snowman, Elefant, Abdula (hero) | Mage in every mission; others 4, 7, 9, 11, 12, 15 | partly covered by task 110 (inventory), measurements missing | per type: command card, abilities, effects |
| 7 | Johnka (enemy) and shipyard | 2, 15 | no passenger capacity established, autonomous patrol open | after ferry work, observe in mission 15 |
| 8 | Economy details: refunds, hall upgrades, service timing, regeneration and facility refresh | all | open in the rules status | covered by task 118 (audit), measurements missing |
| 9 | Original save resume, speech routing and some UI parity | not blocking | open | not on the critical path |

## Proposed order of live reconnaissance

1. **G** (historical report). Hypnosis (rank 1), because the player cannot plan around it and the enemy uses it in five missions.
2. **G** (historical report). Air units and anti-air (rank 2).
3. **G** (historical report). Siege weapons the player owns (rank 3), then fire (rank 4).
4. **G** (historical report). Towers (rank 5) and the special infantry (rank 6), in the missions where they appear.

Each item gets: a short live protocol (which mission, what to do, what to record), the
observations as V rows in a document, the lifted functions involved, and a Codex task
that implements the rule with synthetic tests.

## Label audit corrections (2026-11-23)

Evidence checked: Reference: current descriptor, weapon and aircraft audits supersede these early leads; 0x43d5d0 distinguishes kinds, while 0x433820 reads descriptor speed. Clone: `Rules::load`, weapon dispatch and current aircraft/transport producers. The ranked list and mission occurrences remain G historical reconnaissance, not a verified current implementation inventory.

1. Scoped the obsolete speed-8, unresolved weapon/altitude and disabled-fire inventory to its research snapshot; later descriptor, aircraft and weapon implementations supersede it.

No original executable or reference runtime was run for this audit. Historical
validation totals retain their original dates; they are not this
audit's test results. Private captures, derived source and probe output remain
outside the tracked documentation.

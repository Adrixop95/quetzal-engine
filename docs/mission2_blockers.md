# Mission 2 blockers

Evidence date: 2026-11-14. **V** identifies a fact checked in private original
instructions or data. **G** identifies an independent engine policy or unresolved
mapping. Synthetic tests and clone playthroughs verify implementation behavior;
they do not establish original-game parity. No reference executable was run.
Private inputs, probes and logs remain under ignored `work/m2/`.

## Ferry targeting and damage

| Label | Evidence | Finding |
| --- | --- | --- |
| V | Ferry constructors `0x408410`, `0x4084d0`; vtable `0x48faa8` | The ferry is its own object class (kind getter `0x4083b0` returns 6). It has no Marine descriptor providing a positive HP pool. Its maximum-HP getter returns zero; fixed armour getters at `0x4083e0` also return zero. Zero armour is not proof of immunity. |
| V | `0x43e9f0`, `0x4166a0` | Attack eligibility checks the target's live byte, then weapon availability and firing constraints. Positive calculated damage is required for weapon selection. |
| V | `0x43ea30`, ferry vtable slot `0x124`; `0x408600` | The ferry inherits the ordinary explicit damage handler. It checks the live byte and subtracts damage; docking sets that byte. This does not support blanket damage immunity or invented positive HP. |
| V | `0x43ec60`, instruction `0x43ec92` | The direct type-field comparison against 84 skips a removal-list operation. It is not a target-selection exclusion or an armour rule. |
| G | Automatic assignment | The clone excludes type 84 from `nearest_enemy`. Whether the original's controller/scan population excludes ferries in every phase remains unresolved. This is a playable fallback, not a verified original immunity rule. Johnka is not excluded. |

Direct attacks and projectile damage can still destroy a ferry and its cargo.
The synthetic control boards a passenger while the sides are friendly, changes
them to enemies, verifies automatic survival, then explicitly attacks and verifies
both deaths. Scripted destruction remains possible. A live original targeting
control or a complete scan/controller membership audit is still required before
promoting the automatic exclusion to V. The requested original targeting rule is
therefore only partially established.

## Landing commands

V: `0x408eb0`, `0x4090ee`–`0x4093d9` searches expanding tile rings and asks each
passenger's placement predicate before transferring it to its controller. That
supports expanding the former two-tile search. It does not establish a player
right-click route planner or the clone's collision-disc rules.

G: Right-click land gives selected vessels a landing command before enemy picking
can convert the click to Attack. Other selected units retain normal context
orders. A loaded vessel chooses a reachable water tile adjacent to the clicked
land component, ranked by distance to the click, water route cost, row and column.
It sails there and then places cargo in boarding order onto free connected land,
ranked by distance to the click, row and column. The search extends six tiles
around the boat, retaining terrain-disc and object collision checks. This lets
passengers use safe tiles behind shore edges without placing their collision
discs in water. The six-tile bound and ranking remain G.

The request persists across ticks and participates in hashing and replay. Stop
or another sailing order cancels it. An impossible request preserves the current
sail and displays an English message. A blocked landing retains cargo and reports
that the shore is blocked; it can be retried. The command-card Unload button still
attempts disembarkation at the current shore. Replay semantics version is 8;
older replay saves are rejected explicitly.

Synthetic controls cover distant clicks, ordered unloading, occupied shores,
no adjacent shore, disconnected water, cancellation, landing over an enemy,
hash sensitivity and replay. The UI playtest runs boarding, sailing and landing
without `wait 3` workarounds.

## Playtest commands and click ordering

The playtest grammar is documented in
[`playtest.hpp`](../src/engine/app/playtest.hpp). It now includes:

```text
dump
dump 0
dump 0 84
dump -1 84
```

`dump [side] [type]` prints one line per living
object, including embarked passengers. Omitted filters or `-1` mean all values.
Each line contains object ID, type, side, tile coordinates, hit points, cargo
count and carrier ID. Dead objects and retired non-cargo objects are omitted.
Malformed filters fail the playtest instead of becoming zero through `atoi`.
This is a read-only diagnostic and does not advance the simulation by itself.
The synthetic UI process test checks filtered cargo/boat output and malformed
arguments.

G: Click commands now deliver their release through the UI before parsing the
next line. If that release queued a simulation command, a normal frame applies
it before the next script instruction. `rclick X Y` followed immediately by
`run N`, `dump` or an expectation therefore observes the order without a manual
wait. Clicks still occupy deterministic input frames and can advance simulation
time; they are not zero-tick transactions. The pause/resume UI test accounts for
the completed release frame. Keyboard, held-button and drag commands retain their
existing semantics.

## Bizon and economy evidence

V: Bizon is a Marine. `0x433db0` drops a chase target that is no longer visible
or exceeds 1152 pixels using the approximate norm at `0x461fe0`. The clone now
applies that retention limit to Marine attacks and excludes out-of-limit automatic
candidates. Synthetic controls distinguish a nearby land target from a distant
one. The clone has one Attack target standing in for the original's separate
chase and firing targets; that integration remains G. This is separate from a
full tactic assignment implementation.

G: The clone still selects nearest visible enemies with its existing mode leash;
original `0x451c30` / `0x452c70` uses tactic-level weighted assignment, kill-time
estimates and allied-damage penalties. The clone's shared flow fields, disc
avoidance and water mask also remain independent movement policies. Static
inspection does not verify chase timing or shoreline path equivalence. No Bizon
HP, speed, damage or range bonus/nerf was introduced.

Private clone probes use the unchanged mission, loaded rule tables and sight,
normal player commands, dummy SDL drivers, `timeout` and `unshare -rn`, with a
private game directory and isolated profile. They do not teleport units, edit
cash, disable enemies, force triggers or run the original runtime.

Initial controls before the chase-limit change:

- One ferry boarded five starting troops by tick 300, sailed from the starting
  coast, and reached the enemy bay near tile 38,22. All five were disembarked
  alive by tick 1100 and remained alive at tick 2400.
- The economy accepted barracks, farm and mine construction and completed them.
  It trained 15 footmen, then a separate fresh run trained 15 riflemen. Three
  ferries each boarded five trained units and reached the bay with no ferry loss.
- A single footman assault ended at tick 6803 with the attacking wave lost and
  the mission still running. A separate rifleman assault ended at tick 10103
  with the attacking wave lost and the mission still running. These are failed
  winning-route attempts, not proof of unwinnability.

After adding the chase limit, further fresh controls transported the original
mixed starting force and a separately trained 15-rifle group. Neither won by tick
10103. A reinforcement control trained 30 riflemen, shipped them in two
15-passenger waves on three ferries, and completed the second landing by tick
12004. Its grouped assault also lost its attacking force by tick 17004, with the
mission still running. These controls exercised both individual and shared group
orders; they do not identify target assignment as the sole cause of failure.

A different approach **won twice at tick 5149** in independent processes and
private installations, with the final engine library. This uses the new landing
planner to approach the enemy town directly instead of marching from the western
bay through its defenders:

1. Construct a barracks, farm and mine through normal town production/placement.
   Train 15 riflemen using the starting cash and subsequent income. All 15 are
   available by tick 3503; no starting combatants are needed for this assault.
2. Board five riflemen on each of the three available player ferries. At tick
   4103, all three carry five passengers.
3. Issue a land destination at the enemy town, tile 78,14. The planner selects
   reachable northern shore near tiles 73–74,8. By tick 5103 the boats have
   unloaded their passengers; all three ferries remain alive.
4. Issue a grouped attack-move toward the town. The unchanged mission's victory
   condition fires at tick 5149. Six of the 15 trained riflemen and all three
   ferries remain alive. The independent repetition reaches the same tick,
   outcome and survivor counts.

G: This proves an economy-funded winning route in the clone with its documented
fallback policies. It does not prove the route, target eligibility, combat losses
or timing against the original. No stat change was needed. The initial western
bay failures remain useful negative controls rather than evidence of an economy
or production blocker.

Construction is handled through the existing town work queue; these controls do
not demonstrate separate builder-unit behavior. Original builder scheduling,
full tactic assignment and movement parity remain unverified. A single winning
clone route does not establish campaign parity.

## Validation

Baseline and final Release builds succeeded with `AZTEC_GEN_DIR=`. The complete
69-entry CTest suite passed: 63 ran and six optional private-data/network checks
skipped. Separate `test_formats` and `test_engine` runs passed with the private
installation under dummy SDL and `unshare -rn`. All pre-commit gates passed for
tracked files and this new document separately.

The final loaded-ferry control repeated the five-passenger bay crossing and
unloading by tick 1100, with all five alive at tick 2400. A separate launch of the
normal `aztec_engine --headless` binary used an authored sailing order and
observations over 1200 ticks. Its ferry reached tile 38,22 alive, with zero script
errors and zero unsupported executions. The report's `timeout` status denotes
its bounded, still-running mission; the process completed normally. This latter
control used an empty ferry; the loaded control is the private simulation probe.
The economy victory uses a private headless simulation probe issuing ordinary
player commands through the same engine API, not a forced victory or edited
mission. Its repetition asserts `kVictory` and exits successfully. Original
targeting and pathfinding parity are not claimed.

# Unit automatic behaviour (task 127)

Status: **partial implementation; needs live run**. The independent fixes below
are complete, but the broader tactic/formation and campaign acceptance is open.
No original executable was launched and no private source or captures are included.
V means a fact checked in the private reference instructions/data; G means an
inference, approximation or engine policy. Tests establish engine contracts, not
reference parity. Function addresses identify research locations, not portable
interfaces or implementation recipes.

## Reference audit and comparison

| Area | Evidence and engine boundary |
| --- | --- |
| Idle acquisition | **V:** Tactic update `0x454820` scans expanded member bounds, visibility and nonfriendly live objects, with a random 15..23-tick scan interval. Global/local assignment (`0x451c30`, `0x452c70`) uses eligibility and kill-time weighting. **G:** The clone's nearest-enemy selection every four staggered ticks is still a substitute; its six-tile asset-free radius is not reference sight. |
| Attack modes and chase | **V:** Constructor `0x44f5c0` defaults to balanced (3). Direct/defensive/aggressive/balanced have group distances 4/8/8/6 tiles (`0x4516b0`), measured from tactic origin with the reference norm. Direct suppresses target following, defensive suppresses that branch during path state 3, and modes 2/3 allow combat updates on a path (`0x455ad0`, `0x455b10`). **G:** These are not four weapon ranges or four distances from an individual unit's acquisition position. The clone's weapon reach plus two tiles limits acquisition only; it is not an implemented chase leash. |
| Nearby ally defence | **V:** Nonzero damage notifies the victim's tactic before HP death processing (`0x43ea30`). Tactic notification `0x455570` can forward to attached tactics and distinguishes forced notification from ordinary scans. Side response `0x461a30` is another path. **G:** A universal radius around any nearby ally is not established. The clone has no equivalent complete notification/assignment graph; its AI town alert is a separate policy. |
| Workers and return to work | **V:** The inspected completed-building harvest path (`0x410010`, `0x410cc0`) uses building-owned accumulators. This does not establish an independent worker. **G:** Worker identity, work orders, interruption and return rules remain unestablished. The clone has building income and construction timers, not an evidenced mobile worker state machine. No worker travel or return timer was invented. See the task-103 mine audit. |
| Automatic repair | **V:** Town repairer modes, marks and per-tick credits are separate from cash (`0x4445c0`, `0x444730`, `0x444920`, `0x45d4d0`). Off-mode service can enqueue eligible damaged ordinary buildings. **G:** Nearby mobile service category/boundary parity remains unresolved. Existing `service_towns` and synthetic repair tests are retained; mobile units are not assigned a new repair order. See `rules_types.md`. |
| Retaliation | **V:** Damage notification is not an unconditional individual attack order. Notification rejects friendly sources, Defence-kind sources, and absent/inactive source tactics before storing the source side (`0x455570`). Forced notification can admit an enemy tactic in defensive mode; ordinary notification has a different mode gate. **G:** The clone now records the side for damage notifications, but exact target reassignment and attached/side defence remain pending. It does not blindly retaliate against every `last_hit_by` object. |
| Weak-unit flight | **V:** Local assignment `0x452c70` considers weapon eligibility, kill-time estimates and competing assignments. **G:** No universal low-HP fraction, flee distance, cooldown or worker escape rule was established in this audit. The clone has no such rule. Adding a guessed flee threshold would change mission outcomes without evidence. |
| Blocked movement | **V:** Member processing `0x455b10` maintains pending/active member state, uses tactic distance and tests candidate firing positions. Formation processing `0x450910` gathers occupied locations. **G:** The clone uses flow fields, destination slots and optional recovery rather than that member/path model. A blocked agent or a failed firing-position search is not evidence of a universal reference timeout. |
| Target loss | **V:** Marine think `0x433db0` clears dead/invalid target references and notifies tactic processing. Its pursuit reference and immediate firing reference are separate. The pursuit validation includes a visibility test and a 1152-pixel norm bound. **G:** That bound must not be applied as an anchor leash or generalized to all mobile classes. The clone combines these references. This patch stops stale pursuit when its combined target disappears, while preserving interrupted move, attack-move or follow orders. Hidden-target and reassignment timing still need measurements. |
| Group formation | **V:** Marine idle execution calls tactic member processing (`0x433db0`, `0x455a90`, `0x455b10`); member destination/route state is distinct from current step and tactic origin. **G:** Current clone formation settling and destination slots remain approximations. Startup walking cannot establish a random wander radius. See `idle_tactic_walk.md`; its pending task-95 correction is not present as a verified formation replacement here. |
| Script idle | **V:** `0x417d60` compares tactic state 0/1 as idle and returns false for a missing tactic. **G:** The clone uses member logical orders and pending script paths. These are not a complete mapping of tactic states. Cancelling a lost attack's movement fixes one case where logical idle previously coexisted with continued pursuit. |
| Script attacked | **V:** Condition `0x417dc0` calls tactic getter `0x408bd0`, which reads its stored notifying side. Constructor `0x44f5c0` initializes this to -1; notification `0x455570` stores the source object's side, before its ordinary/forced mode branch. The condition dispatch is separate from the action dispatch. **G:** Notification can also originate from a scan, so this is not simply a damage counter or a one-tick hit pulse. The clone's damage path is corrected below; scan-derived and attached notifications remain pending. |

The broader mode specification is in [rules_ai.md](rules_ai.md#tactic-attack-modes).
Repair evidence is in [rules_types.md](rules_types.md#repair-destroy-garrison-and-limits).
Worker/depletion limits are in [103-mine-audit.md](103-mine-audit.md).

## Independent changes

- **V contract / G adapter:** `tactic(N).attacked` now reads a stored notifying
  side, independent of object attribution. A positive direct, projectile or blast
  hit records that side for the victim's side-local tactic, subject to the
  notification gates above. Friendly damage still retains object attribution;
  it does not create this notification. Neutral sources pass the nonfriendly
  gate. Side zero is a valid result. Defence-kind sources do not pass this path.
- **G:** A world map keyed by side/tactic number represents that state, retaining
  the latest notification after the hit member dies or the source leaves. It is
  included in deterministic hashes and reconstructed by command replay. This
  avoids selecting the first surviving member's stale attacker ID. Complete
  tactic destruction/recreation, joining, gifting, scan notifications and import
  of original saved tactic state still need a lifecycle model and audited mapping.
  Merely retaining a numeric source tactic is the current active-tactic adapter;
  it does not reproduce the reference active flag.
- **G:** Lost attack targets now use the same cleanup as a stance change. Idle
  units stop their movement agent; interrupted movement is reissued toward the
  retained destination. This prevents continuing to a dead target while script
  conditions see idle. Explicit and automatic attacks share that cleanup.
- **G:** The misleading one/two-tile measurement claim in `nearest_enemy` was
  replaced with an explicit provisional-policy comment. No acquisition/leash
  constant was changed based on the cached startup samples.

No low-health fleeing, mobile worker state, repair travel order or universal
nearby-ally defence radius is claimed as implemented. Those depend on the
unresolved tactic/formation/observer work; the independent corrections above do
not depend on its completion. Original-save script execution remains disabled
by the existing import adapter. Original-save notification restoration is not
verified by replaying the engine's own command log.

## Existing private measurements

**V (cached report inspection):** `atk_215058`, `atk2_220146` and
`dn_213657_dense_l0` under `/home/adrox/aztec-re/work/parity` report
`blocked_observation_contract` for frame, phase, orders, queues and view. The
mission-2/3 dense reports record `experiment_failed`. Their diagnostic native
samples may still exist, but neither failed nor incomplete reports validate
logical orders, chase radii, retaliation or script semantics.

**V (previously reviewed observation):** The private startup samples discussed in
[idle_tactic_walk.md](idle_tactic_walk.md) distinguish member settling and idle
animation from autonomous wandering. **G:** No controlled, repeated per-mode
capture was found in the inspected sets. No public numeric fixture, private
stream, script body or screenshot was produced from those captures. These cached
runs cannot certify the new fixes or all fifteen campaign missions.

## Maintainer run sheet: needs live run

Capture on the Windows installation; Codex must not launch the original here.
Use two independent original sessions per case with the same initial state,
map, settings, attack mode, visibility, seed if observable, and input sequence.
Retain captures, map/script inputs, settings, binary/build identifiers and raw
observations privately. A missing logical-order/tactic observer is a dependency,
not permission to infer orders from animation. On a Linux reference host retain
the mandatory private installation copy, dummy SDL drivers, hard timeout and
`unshare -rn` setup from EXPERIMENTS.md. Namespace failure is a failed capture.

For each actor record its stable identity, class, side, tactic, primary HP,
position, logical order, target, member destination, tactic origin/state/mode,
notification side and script idle/attacked values before and after each event.
Keep current position, reserved destination and origin as distinct fields.
Align before-command/after-update phases and capture every tick around events;
wall-clock waits alone cannot identify a transition tick. Record unknown fields
as unavailable rather than filling them from clone expectations.

| Case | Controls and required observation |
| --- | --- |
| A: acquisition and leash | For all four modes, stationary melee/ranged units with one enemy entering and leaving visibility and weapon range. Move the enemy beyond the tactic-origin distance, then back. Compare first acquisition, pursuit, release and return positions; distinguish range from norm/distance. Repeat with an explicit attack and a path order. |
| B: allied defence | Hit one idle member, then a different tactic and an allied side. Vary ally distance, attachment and the defender's mode/path state. Include a visible harmless enemy with no damage as a scan control. Record which tactics are notified and which members are assigned, without presuming an assist radius. |
| C: work interruption | First establish whether the visible worker is an independent object or building animation. If independent, record its actual work identity/order, interrupt by move/attack/Stop and remove its work target. Record return versus idle, destination, and income activation. If no worker object is observed, report that boundary. |
| D: repair | Off/repair/destroy town modes, a damaged ordinary building, Defence and mobile unit, marks on/off, credits available/unavailable, service structure present/absent. Observe credit and HP transitions separately from orders and cash. |
| E: retaliation and attacked | Actual hits from hostile, neutral, allied and same-side sources, a Defence source and a source without an active tactic. Include zero damage/miss and a visible enemy that never hits. Query the script condition with source side deliberately different from object ID. Repeat with a second attacker side, death of the hit member, source death and one surviving teammate. Measure persistence and any reset; also test joins/gifts and tactic recreation. |
| F: weak-unit flight | Sweep primary HP with armed/unarmed classes, each mode, cooldown ready/busy, ally support and path/idle orders. Record whether any retreat is assigned, its trigger, target and endpoint. Do not equate an assignment change or collision sidestep with fleeing. |
| G: blocked/lost target | Wall, friendly congestion and no reachable firing tile; then remove, kill, embark or hide the target. Record order/agent cleanup, retry timing, retarget and resumption for idle, move, attack-move and follow. Keep blocked and arrived distinct. |
| H: group keeping | Sparse/dense groups, occupied origins, displaced member, turning path and following tactic. Record member routes/slots and formation marks through attack interruption and recovery. Reuse the task-95 dense startup procedure for missions 1-3 with an audited seed and draw schedule. |
| I: scripts and campaign | Record attacked/idle before acquisition, after notification, after target loss and during pending paths. Replay equivalent inputs in the clone and locate the first repeated divergence. Finally rerun missions 1-15 with reviewed winning routes; these unit cases do not establish campaign completion. |

**G acceptance procedure:** Only promote an unresolved rule after both original
captures agree and the observer/control audit passes. Implement it with invented
fixtures, then repeat the unchanged controls against the corrected clone. Report
unsupported fields and first divergence explicitly. Keep exact/tolerant visual
results separate from logical state. Full automatic-behaviour parity remains open.

## Verification

Baseline: Release configuration with `AZTEC_GEN_DIR=` and `GAME_DIR=`, full build,
CTest and pre-commit passed. CTest registered 69 checks: 63 passed and six skipped
(private-data and restricted UDP process checks). The reference was not run.

Synthetic regressions cover missing/dead/removed target cleanup, existing
move/follow resumption and stance gates, script notification side zero, side-local
identity, friendly/neutral gates, Defence and absent source tactic, zero damage,
member/source loss, state hashing, direct/projectile damage and command replay.
Post-change: the full Release build passed; all 63 runnable CTest checks passed
with the same six skips and no failures. Pre-commit passed on all tracked files
and separately on this new document; `git diff --check` passed. No synthetic
success upgrades the live-evidence status above. Changes remain uncommitted
because git metadata is read-only.

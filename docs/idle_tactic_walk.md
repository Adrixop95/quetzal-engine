# Idle tactic walk investigation (task 95)

Evidence labels (audit 2026-11-23): V = inspected source fact, with reference
or clone scope stated; M = measurement rechecked from retained captures or an
offline clone probe; G = inference, proposed procedure or uncorroborated
historical report. A clone V does not establish native parity. A marker covers
one paragraph/list item or the entire immediately following table. Unresolved
meanings remain G even inside a table whose codec structure is V.
See [label_audit.md](label_audit.md) for counts and evidence limits.

**G** (pending interpretation or measurement protocol). Status at task 95: **partial; engine correction and live acceptance pending**. The later [formation candidate](group_formation_rule.md) does not establish the native assignment rule. This work
adds diagnostic probes and a dense measurement workflow. It does not implement
an idle movement policy or change simulation hashes.

## Evidence inspected on 2026-11-07

**M** (retained reference samples; no new native run). The worktree started clean, without a partial patch or local task notes. The
supplied private analysis and cached native observations were available read-only.
The three `dn_213657_dense_l0*` reference captures contain ticks 1 through 24,
then 30, 36, 48 and 60. There is only one dense capture per mission in that set;
these counts are **single-capture observations**, not newly repeated results.
The user's repeated ticks 1/6/12 evidence remains separate.

Walking execution counts for the initial Marine cohort are:

**M** (retained reference samples; no new native run).

| Mission | Cohort | 1 | 2 | 3 | 4 | 5 | 6 | 9 | 12 | 18 | 24 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | 60 | 31 | 33 | 33 | 31 | 31 | 31 | 22 | 17 | 12 | 4 |
| 2 | 62 | 11 | 12 | 13 | 17 | 19 | 29 | 32 | 29 | 29 | 29 |
| 3 | 137 | 6 | 7 | 7 | 7 | 6 | 6 | 3 | 1 | 0 | 0 |

First observed walking ticks in those cohorts:

- **M** (retained reference samples; no new native run). Mission 1: 31 at tick 1, two at tick 2; 27 never walk through tick 24.
- **M** (retained reference samples; no new native run). Mission 2: 11 at tick 1, one at tick 2, one at tick 3, four at tick 4,
  two at tick 5, 11 at tick 6 and five at tick 7; 27 never walk through tick 24.
- **M** (retained reference samples; no new native run). Mission 3: six at tick 1, one at tick 2; 130 never walk through tick 24.

**M** (retained reference samples; no new native run). Tick 1 is already a walking observation, not the observed instant of destination
selection. Mission 1 has no walking units at ticks 48 and 60; mission 3 has none
at ticks 14 through 48. Mission 1's first walking sample includes a unit whose
reserved tile differs from the tactic anchor by (-6,-3). The anchor alone does
not establish a one/two-tile random destination rule. These observations suggest
startup settling as a candidate explanation; they do not prove it.

**M** (retained reference samples; no new native run). The cached mission-3 native capture exists even though its normalized bundle
failed with `spawn identity needs an audited creation observer`. The new tool
summarizes the **initial Marine cohort** directly from native samples. It refuses
missing ticks, duplicate identities/handles and disappearing cohort members.
It reports additional Marines separately. It does not resolve general spawn
identity, logical orders, frame alignment or whole-game parity.

## Inspected facts and unresolved decisions

Read-only inspection of the private executable and analysis establishes these
facts. No executable-derived implementation is included in the repository.

- **V** (reference arithmetic/structure). Marine idle execution calls the tactic's per-member processing path
  (`0x433db0`, `0x455a90`, `0x455b10`). A member record contains its own tile,
  active/pending flags and route bookkeeping. These differ from the tactic anchor
  and the Marine's current step destination. Formation processing (`0x450910`)
  marks units and gathers occupied locations. Path search and occupancy therefore
  need auditing before attributing a chosen destination to RNG.
- **V** (reference arithmetic/structure). Step setup (`0x433820`) targets the centre of a destination tile, computes the
  pixel displacement from the current fixed-point position and scales that
  displacement using descriptor d5 and the approximate norm (`0x461d50`,
  `0x461fe0`). The norm differs from Euclidean length. For example, a displacement
  of (0,-48) has norm 47: speed 8 gives velocity (0,-2091), truncating division
  toward zero. (-48,0) gives (-2091,0); (-71,-48) has norm 88 and gives
  (-1652,-1117). Thus the supplied velocities do not imply randomized speed.
  `Rules::load` already reads Marine speed from d5. Other mobile classes require
  separate layout and execution audits.
- **V** (reference arithmetic/structure). The Marine idle timer at +0x88 controls **idle animation**, not an established
  walk cooldown: it decrements during idle, resets to 60 plus a CRT random value
  modulo 200 when negative, and considers an idle animation when zero. Calling
  this timer a movement delay would conflate distinct behavior.
- **V** (reference arithmetic/structure). Mission loading (`0x42ec90`) seeds the CRT stream from the stored mission seed.
  The inspected ordinary startup path (`0x43c9b0`) can obtain that seed from
  `timeGetTime`. The CRT generator (`0x481070`) has thread-local state, with
  multiplier 214013, increment 2531011, 32-bit wrapping and output bits 16..30.
  Constructors, animation and other calls consume this stream. This is not
  evidence for a separate per-side walk generator, seed 1, or seed equal to the
  mission simulation tick. Actual seed/state and consumption order remain
  unmeasured in the cached observations.

**G** (pending interpretation or measurement protocol). There is no verified offset set, probability, movement cooldown, complete random
draw schedule or formation/occupancy rule yet. The independent engine's existing
movement policy cannot be replaced with a guessed random walk. Exact position
agreement for all three missions and the requested simulation regressions remain
pending these measurements. The generated synthetic measurement fixture uses
the recorded mission-1 count pattern and supplied signed velocities, with invented
identities/positions; it tests the summarizer, not engine movement.

## New observations and repeat procedure

**G** (pending interpretation or measurement protocol). The optional reference order probes now include `formation_marked`,
`idle_animation_timer`, `descriptor_speed_px` and a `member` record. Member lookup
audits the complete active/free pool chains and rejects duplicates, missing
members and short records. Failures remain explicit nested diagnostics and do
not suppress base scalar observations. The top-level diagnostic `mission_seed`
is the stored seed, **not current CRT random state**. Existing captures without
that field remain readable. Synthetic tests cover signed timers, distinct member
tiles, member route state, descriptor speed and malformed member pools.

**G** (proposed verification procedure). Prepare all three dense schedules in a fresh ignored directory:

```sh
python3 -I tools/idle_walk_measure.py prepare \
  --output work/parity/idle-walk/scenarios
```

**G** (pending interpretation or measurement protocol). Rebuild the reference observer inside this worktree as described in
[EXPERIMENTS.md](EXPERIMENTS.md#l0-pool-and-mission-startup-follow-up-2026-11-06-needs-live-run).
The supplied external runtime does not automatically include the new probes.
For each mission 1, 2 and 3, run two repetitions with independent outputs:

```sh
python3 -I tools/parity_run.py idle_walk_mission_1 --flat-output \
  --scenarios-dir work/parity/idle-walk/scenarios \
  --game /home/adrox/aztec-re/game \
  --runtime "$PWD/build_l0_reference/aztec_recomp" \
  --engine "$PWD/build/aztec_engine" --timeout 120 \
  --output work/parity/idle-walk/mission-1-a
# Repeat into mission-1-b, then repeat both runs for missions 2 and 3.
python3 -I tools/idle_walk_measure.py summarize \
  work/parity/idle-walk/mission-1-a/reference/observations.jsonl \
  work/parity/idle-walk/mission-1-b/reference/observations.jsonl \
  --output work/parity/idle-walk/mission-1-repeat.json
```

**G** (pending interpretation or measurement protocol). The summarizer compares full initial-cohort scalar/movement timelines after
replacing process handles with initial owner/type/position identities; aggregate
walking counts alone cannot pass repeatability. Exit codes: 0 equal observed
cohort timelines, 1 different timelines, 2 invalid/incomplete inputs. Outputs
must be new. It never creates reference expectations from clone output.
Retain and inspect `runners.json` first: a successful cohort summary does not
override runner or shared-contract failures.

**G** (pending interpretation or measurement protocol). The 2026-11-07 local attempt used the supplied runtime with a private game copy,
dummy SDL and `unshare -rn`. Reference setup failed before launch:
`Cannot open netlink socket: Operation not permitted`. The separately isolated
clone completed its 24-tick schedule. No new reference samples were obtained;
repeating the same denied setup would not establish repeatability. Private
attempt output remains under `work/parity/idle-walk/mission-1-attempt/`.

Before implementing movement, collect repeated dense reference runs with the
new probes, audit the exact formation/path/occupancy transitions for the first
step, and trace CRT state/draw order if that path consumes randomness. Preserve
clone-before samples. Then implement only the demonstrated rule, add actual
simulation velocity/trajectory regressions, compare all requested positions and
review every changed deterministic expectation against those captures.

## Label audit corrections (2026-11-23)

Evidence checked: Reference: 0x433db0, 0x433820, 0x455a90/0x455b10, 0x461d50/0x461fe0, 0x42ec90 and CRT 0x481070. Clone: `Rules::load`, `sim/path/`, `tools/idle_walk_measure.py` and optional observer fields. Retained dense startup samples reproduce the walking table and first-start counts; no new reference process was launched.

1. Scoped the investigation status to its historical snapshot and linked the later candidate.

No original executable or reference runtime was run for this audit. Historical
validation totals retain their original dates; they are not this
audit's test results. Private captures, derived source and probe output remain
outside the tracked documentation.

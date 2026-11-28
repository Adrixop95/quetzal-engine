# Missions 6–15 completion progress

Status: incomplete. This investigation does not establish ten completable
missions or original-game parity. Only independent-engine runs are counted.
Historical G-labelled route reports are investigation leads, not new victories.

## Aggregate results

The retained baseline and updated smoke sweeps each load and execute all ten
requested missions through the 720-tick smoke horizon without a process failure.
These passive checks send no player orders and are not winning routes.

Current scripted completion count: **0/10 missions reach victory**. All ten
have private objective-directed player scripts and launch attempts. Completed
routes include natural defeats and failures after troop losses; larger battles
also exhaust their wall budgets. Longer reserve-building attempts witness paid
production and income but still do not complete elimination. Interrupted
attempts and the historical winning report do not increase this count.

## Shared defect fixed

Normal automatic member assignment included ordinary buildings. Troops could
stop at weapon range and repeatedly fire at an indestructible hall instead of
continuing an attack-move into capture range. Private campaign observations
exposed this stall; the established exclusion is documented in
[rules_ai.md](rules_ai.md) and [rules_types.md](rules_types.md).

Automatic member selection now excludes ordinary buildings. Explicit focus
orders retain their structure damage path, and listed Defence members remain
eligible. No mission-specific exception, resource grant, health adjustment or
outcome override was added. Replay semantics advance to 17 because the change
alters simulation results.

Synthetic regressions cover protected and damageable ordinary buildings,
defender selection beside a nearer building, listed Defence eligibility,
explicit structure damage, and attack-move capture followed by a natural script
victory. Command-log replay must reproduce the resulting state. Visibility and
siege controls now distinguish ordinary building visibility and explicit focus
from automatic combat-member selection.

This fixes one shared defect class; it does not establish that each unsuccessful
campaign route has a diagnosed engine blocker.

## Evidence limits and open classes

All campaign scripts in the smoke inventory have supported action dispatch.
This does not establish correct controller lifetimes or execution of dormant
sites. Missing general side-plan execution remains an engine dependency, but
no supplied campaign site was attributed to that unsupported action.

Open investigations include sustained production and force preservation,
protected-unit journeys, ordered capture and rescue routes, and the final
conquest/diplomacy/vessel sequence. Empty selections after casualties, invalid
resource sites, exact-tile assertions on inaccessible destinations and exhausted
wall budgets are route or observation failures. They do not establish additional
engine defects. Path/controller recovery, AI opposition and special-ability
fidelity still need controlled evidence before further behavior changes.

Every live process uses dummy SDL video/audio inside `unshare -rn`, with a
private installation and user profile. Mission inputs, player scripts,
observations, captures and diagnostic replays remain in ignored
`work/missions-6-15/`. Interrupted streams without terminal reports are excluded
from completion evidence. No original executable or reference runtime was run.

## Verification

The Release build and full CTest suite pass: 79 tests pass and six optional
checks skip. The capture-to-victory test also passes independently after its
final strengthening. Separate private-copy format and engine data checks pass
inside the required namespace with dummy SDL. Pre-commit and whitespace gates
are run separately for tracked files and the new aggregate report. Native
Windows/macOS CI and original-game parity were not measured here.

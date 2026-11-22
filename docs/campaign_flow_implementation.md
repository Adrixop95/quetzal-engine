# Campaign flow implementation

Evidence review: 2026-11-22. **V** means a documented static reference fact or an
explicitly identified engine construction property; it does not mean live parity.
**G** marks an unresolved interpretation, implementation gap or unverified user
experience. Known reference facts behind a G implementation gap remain V.
The original executable and reference runtime were not run for this task.
All added test inputs are invented, with no original mission data or code.

Sources: [campaign loader and flow](campaign_loader_and_flow.md),
[campaign menus and saves](campaign_menus_saves.md),
[audio/save residuals](audio_saves_residual.md),
[script/descriptor leftovers](leftovers_scripts_descriptors.md) and
[formats](formats.md). Private campaign structure notes were read only;
no mission tables, assets, lifted code or decompiled source were copied here.

## Comparison and implemented corrections

| Area | Documented original | Engine implementation and evidence |
| --- | --- | --- |
| Mission order | V: fixed missions 1 through 15, in numeric order | V construction: the campaign browser iterates numeric names, rather than sorting filenames lexically; missing local files are omitted |
| Mission labels | V: Mission plus number, current row without status, earlier rows Complete, final row Victory! unless current | V construction: browser labels now follow the progress rule instead of map titles; status is independent of installed map titles |
| Unlocks | V: progress p exposes 1..p for p=1..14; p=0,15,16 exposes all fifteen | V construction: locked entries are now omitted; a later win unlocks all intervening missions, rather than only a predecessor/completion bit |
| Progress update | V: new profile p=1; victory writes min(16,max(mission+1,p)); defeat leaves p unchanged | V construction: a scalar replaces the completion mask; earlier replays cannot reduce progress. Writes occur once in the mission-loop tail, after the player leaves the engine result view |
| Briefing | V: campaign Play Mission reads .mit and uses the mission AVI; custom and load bypass this entry | V construction: existing Diary/Order/Objectives views remain. Objectives use the third .mit section rather than whichever section happens to be last. Editor Notes are no longer substituted for missing briefing/objectives. Mission AVI playback is restricted to a fresh campaign launch |
| Campaign movies | V: ordinary campaign victory and defeat have no win/lose movie; a victorious next progress of at least 16 selects final.avi | V construction: movie selection now follows these branches and executes at mission exit. A win with existing progress 16 also selects final.avi; the static branch supports this, but its visible replay behavior remains G |
| Custom result movies | V: Custom Mission state selects win.avi or lose.avi only for a terminal outcome | V construction: explicit custom worlds select the appropriate result movie; running missions select none. Movies depend on local files and ffmpeg |
| Script end conditions | V: victory/defeat writes a side outcome only while still playing; only the local side outcome is copied to the mission result | V construction, already present: first outcome wins, other sides cannot finish the local mission, and there is no universal force-elimination end condition. Added regressions exercise both human outcomes and an empty world without scripts |
| Script timing | V: globalTime and localTime use simulation ticks; localTime wraps at 7200; trigger/action timing matters | V construction: existing script implementation and synthetic timed-win gates retained. No per-mission end condition is hardcoded from private scripts |

The result overlay, retry and next-briefing shortcuts remain engine policies.
They are not established original controls. Progress writes no longer happen
merely because this overlay first appears. Direct diagnostic sessions without a
menu session, scripted playtests and imported original worlds do not award wins.

## Engine progress storage

V construction: `Saves/engine_campaign.txt` remains in the engine user directory,
selected by `AZTEC_USER_DIR` or the platform user path. It is separate from the
installation and original `.plr` files. Version 2 contains the header
`QUETZAL_CAMPAIGN_2`, followed by one integer 0..16 and a terminating newline.
Malformed, oversized or out-of-range files are refused without mutating the
loaded record. A failed load prevents the mission-exit writer from replacing the
existing file. Exclusive staging-directory creation and atomic replacement are
retained; stale `.pending` state refuses a write.

V construction: version 1 engine masks are read for compatibility. An empty mask
becomes progress 1; otherwise the highest completed bit becomes mission+1.
Saving writes version 2. This migration deliberately replaces sparse completion
history with contiguous unlocks. It does not alter an original profile or save.

V reference / G implementation gap: the original stores progress at offset 0x44
of a 300-byte named single-player `.plr` record, saves options in that record,
and stores slots under `Saves/<profile>`. The engine still has one default
campaign record, separate front-end preferences and engine replay slots. The
full original profile selector/lifecycle from task 77 is absent in this checkout;
this task completes the independent progression rules without claiming that
profile creation, selection, deletion or original-file interoperability exists.

## Open behavior and dependencies

| Label | Open item and present behavior | Required follow-up |
| --- | --- | --- |
| G | Automatic end/result screen: the inspected outcome store does not itself leave the reference loop. The engine freezes at its own result overlay | Identify modal/end-flag callers and observe the actual result UI; do not infer an automatic score screen from the outcome store |
| V stack arithmetic / G visible return | End Mission unwinds to the launcher: campaign briefing state 17, custom state 6, load state 4. Whether briefing then unwinds again is unresolved. Engine victory Continue jumps to the next briefing; defeat/Menu returns to Main; Retry restarts directly | Complete the task-77 front-end stack and observe the return chain before claiming matching navigation. No automatic successor launch is established by the original progress update |
| V reference / G missing profiles | Original progress and settings belong to the selected single-player profile, rather than one engine-wide default | Implement the missing profile selector and separate named engine storage, with kind/name matching and per-profile slots |
| V reference / G save continuation | Original saves retain the campaign mission number independently of the map basename and restore script latches. Engine AZS3 replay continuation is tested; imported original worlds deliberately cannot write progress | Finish original-save continuation and explicit campaign launch identity before enabling imported-save progression; tasks 94/V16 remain dependencies |
| G | Engine custom/campaign context uses explicit custom-world state plus bounded mission names, rather than the complete original menu stack or configured CampaignPath | Support separate campaign/custom roots and persist launcher identity; test identically named maps in both roots |
| G | Begin Campaign remains an engine convenience; no original constructor for string 140 has been found | Establish whether the original exposes this control |
| G | Exact briefing margins, result artwork, score content, movie timing/skip behavior and post-final screen are unmeasured | Obtain authorized original observations; no executable run was performed here |
| G | Real campaign reachability, simultaneous trigger cases, hero/transport objectives and save-to-win routes | Finish the existing script/transport and observer dependencies and audit ordinary player routes. Synthetic success does not establish any original mission completion |

The save residual document's earlier claim that no srand exists is superseded by
its dated correction: V, the mission loader seeds the simulation thread. This
change does not introduce a claim of original RNG, wind, audio or prototype-state
save parity. Those domains remain separate continuation dependencies.

## Synthetic validation

`campaign_ui` covers bounded mission parsing, objective sections, unlock/status
boundaries 0/1/7/14/15/16, noncontiguous legacy-mask migration, monotonic progress,
movie branches, malformed records and failed replacement preservation.
`campaign_flow` retains fifteen invented win/lose/retry UI routes and explicitly
checks that playtest outcomes cannot write progress. Additional bounded runs use
synthetic pre-ticks without the playtest guard to exercise the actual mission-loop
tail: victory persistence, earlier-win replay, defeat/running preservation and
refusal to overwrite an invalid profile. These diagnostic runs are construction
checks, not normal player campaign observations.

`script_unit` checks absence of an implicit end condition, local-only results,
first-outcome precedence and existing timer/enablement behavior.
`campaign_save_processes` checks fresh-process replay continuation on invented
missions. No test loads original assets, profiles or saves.

Baseline and final Release builds, CTest and pre-commit checks pass. The suite has
82 entries: 76 pass and six skip (five private-data checks and the restricted
network process check). No reference recording/replay, original campaign
completion, real-data headless run or native Windows validation is claimed.

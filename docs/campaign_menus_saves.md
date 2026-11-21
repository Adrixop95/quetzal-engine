# Campaign flow, player profiles, saves and front-end states (original Aztec.exe)

Read-only analysis of `work/decomp/all_full.c`, `work/gaps/dec_41eef0.c` and objdump of `game/Aztec.exe`.
V = read in code or data. G = inference not checked against a live run. Numbers are hex (decimal).
Related docs: `docs/script_conditions_flow.md` sec. 4, `docs/savegame.md`, `docs/formats.md` (User files, Video), `docs/frontend_screens.md`.

## 1. Campaign progression

| Item | Finding | Evidence |
|---|---|---|
| Mission list | 15 (0xf) fixed names `mission_1.mp` .. `mission_15.mp`, pointer table `0x4adfac[0..14]`. Path = registry `HKLM\Software\New Media Generation\The Aztec\CampaignPath` + `\` + name | V `0x43f620` (RegQueryValueExA `CampaignPath`, fmt `%s\%s` at `0x4ae148`) |
| Current mission number | global `0x4dd164` (1-based; 0 = not a campaign mission). Set when Play Mission starts: `atoi(strpbrk(basename, "123456789"))` of the selected file (`0x4aa364` = "123456789"; basename taken after the last `\`). The once-only end flag `0x4dd160` is cleared at the same time | V `0x419290`, stores at `0x419b03` / `0x419b19` |
| Reset | `0x4dd164 = 0` on entering state 1 (`0x41b790`) and state 3 (`0x41b710`), so custom and multiplayer games count as non-campaign | V |
| Progress field | profile `+0x44` (the current-profile copy is at `0x4cc5a4`). Getter `0x43f550`. Setter `0x43f560(n)` stores n and then rewrites the matching `.plr` file (300 (0x12c) bytes from `0x4cc560`). It matches on the name `0x4c47e0` (`0x429190`) **and** on flag byte == 0, so only single-player profiles are written | V `0x43f550`, `0x43f560` |
| Victory rule | at mission end the code checks `0x4dd164 != 0`, the local side's outcome (side `+0x140`) == 1 (won) and `0x4dd160 == 0`. It then sets `0x4dd160 = 1` and computes next = max(`0x4dd164`+1, progress). If next < 0x10 (16) it calls `0x43f560(next)` and plays **no** movie. Otherwise it calls `0x43f560(0x10)` and plays `final.avi` (`0x4ae93c`) | V tail of `0x4577b0` (`0x45889c`..`0x4588ba`) |
| Defeat / custom | when the mission is not a campaign mission or was not won, a movie plays only if `0x429020()` is true (top state is 6, Custom Mission) and outcome != 2 (2 = still playing): `win.avi` (`0x4ae934`) for outcome 1, otherwise `lose.avi` (`0x4ae928`). So a campaign defeat plays no movie and leaves progress unchanged | V `0x4577b0`; table `0x4aadc4` has only index 6 set |
| Movie playback | video dir `0x4c1a50` + file name are copied to `0x4dd168`, then `0x515560 = 1`, SetEvent `0x4c4d60`, wait on `0x4c4d5c` (handshake with the video thread) | V `0x4577b0` |
| Intro movie | the mission's `[Globals] AVI=` is played by `0x419110`, called from Play Mission (`0x419adc`) just before the mission number is stored. `intro.avi` at start-up: `0x42d300` | V (see formats.md Video) |
| Unlock rule | the campaign list (state 1) shows missions 1..p for progress p in 1..14. When p-1 falls outside 0..13 (p = 0, 15 or 16) all 15 missions are listed. Row label `"%s%d"` = string 119 "Mission " + number, plus a status: row p-1 (current) has an empty status, row 14 (0xe) shows "Victory!" (120), the others "Complete" (121) | V `0x43f620` (`iVar7 - 1U < 0xe`, fmt `0x4ae140`, ordinals via `0x4c4f40+0x1dc/0x1e0/0x1e4`) |
| New profile | progress starts at 1 (`+0x44 = 1`) | V `0x43f2e0` |
| Disc table | `0x4adfe8[mission-1]` = 1,1,1,1,1,1,1,1,1,2,2,2,1,2,2; compared with 1 at `0x419509` during the Play Mission file checks | V data; G: CD number (disc 1 or 2) |
| Difficulty | no campaign difficulty value is stored in the profile or passed by Play Mission; only `.mp` side metadata exists (see doc 124) | V no such field found; G that none exists anywhere else |
| Speed settings | game speed and scroll speed sit in the options block (profile `+0x4c` + `0xac` / `+0xa8`), so they follow the profile across missions | V `0x43f070` copies the block to `0x515430` |

Unknowns:
- Whether "Begin Campaign" (string 140) is a real button. No constructor references ordinal 140 (see sec. 4).
- What the list shows when p = 0.
- When `final.avi` plays. The threshold is next >= 16, so it also plays when the player wins any campaign mission after progress has already reached 16 (G, follows from max()).

## 2. Profile (`*.plr`), 300 (0x12c) bytes

| Offset | Meaning | Evidence |
|---|---|---|
| `+0x00` u8 | kind: 0 = single-player profile, non-zero = multiplayer profile. The two lists are built separately (`0x41b040` for 0, `0x41b0e0` otherwise). Create Player is disabled when the list for the current mode holds more than 9 entries | V `0x43ee40` |
| `+0x01` | name, NUL-terminated, up to 0x43 (67) bytes | V `0x43f2e0` |
| `+0x44` u32 | campaign progress (sec. 1) | V |
| `+0x48` | set to 0 at creation; meaning unknown | V `0x43f2e0` |
| `+0x4c` | 0xe0 (224) byte options block, a copy of `0x515430` (fields listed in formats.md) | V |
| in memory only | node of 0x230 (560) bytes: `+0x12c` file name `<timeGetTime>.plr`, `+0x22c` next pointer | V `0x43ee40` |

| Function | Role | Writes? |
|---|---|---|
| `0x43ee40` | enumerates `*.plr` in the game directory and rebuilds profile list `0x4cc68c` (count `0x4cc404`) | read only |
| `0x43f070(name, kind)` | selects a profile: copies it to `0x4cc560` and the options block to `0x515430`, then applies gamma (`0x478680`), audio groups (`0x474950` 1/2/4) and resolution. If kind != 0 it also calls `0x43a780`. Sets `0x4cc400 = 1` | no |
| `0x43f210()` | saves the current profile: refreshes the options block from `0x515430` and rewrites the file (name and kind must match) | yes; called by the options Back button `0x418be0` |
| `0x43f2e0(name, kind)` | creates a profile. Fails if name+kind already exists. Writes `%i.plr` (timeGetTime) with CREATE_NEW and creates directory `Saves\<name>` | yes |
| `0x43f420(name, kind)` | removes a profile: deletes the `.plr` file and calls RemoveDirectory on `Saves\<name>` | deletes |
| `0x43f560(n)` | sets progress and rewrites the file (single-player profiles only) | yes, at campaign victory |

Callers (V):
- Accept Player `0x418c40` and the row handler `0x41b170` call `0x43f070`, then push state 1 (single player) or 3 (multiplayer).
- Create `0x418cd0` calls `0x43f2e0`.
- Remove `0x418dc0` calls `0x43f420`.

Unknowns: the meaning of `+0x48`. The "unlock all missions" call `0x43f560(15)` in PLAN.md is not among the original callers; the only original callers found are in `0x4577b0`.

## 3. In-mission save and load

**Save (V)**
- Writer `0x4450e0(slot, description)` is called from the in-game save confirmation `0x41a3d0`, which takes the description from text field `0x4c4980`.
- The header includes `0x4dd164` (written at `0x44517e`), so a campaign mission loaded from a save still counts for progression.
- The stream grammar is documented in `docs/savegame.md`.
- The Save button `0x41a340` works on slot list `0x4c1b94` and sets `0x51555b = 1`.

**Load entry (V)**
- Load This `0x41a2e0` builds `single%i.sav` (`0x4aa390`) from the selected slot and calls `0x428f30(path)`, the same entry point Accept Mission uses.
- `0x428f30` stores the path in `0x4c4b58`, sets `0x4c4cc0 = 1` (game requested) and signals `0x4c4d6c`.
- `0x445570(profileDir, file)` changes directory to `Saves\<profile>`, opens the stream, runs `0x4455f0` and closes it.

**Read order in `0x4455f0` (V)**
1. Header: mode word, `0x4dd164`, mission path (256), description (64), SYSTEMTIME (16).
2. View `0x4625d0`, globals `0x43d3a0`, then `0x429080(path)`, which puts the mission path in `0x4c4b58`.
3. Pools, in order:
   - decorations `0x4c1978`
   - towns `0x5156f8` (bit 31 tag)
   - sides `0x4dd2f4`: count via `0x43d1d0`; array of 0x69c (1692) bytes; each side is loaded through vtable slot 0, the side loader that contains `0x446c41`
   - tactics `0x4d9b50` (switch on kind)
   - units, weapons, effects `0x4c1298`, transports `0x4c4ce0`, scripts `0x4cd698`, orders `0x4d9b88`

   Each pool keeps an id-to-object table (`0x4cd510`..`0x4cd538`) that the fix-ups use.

**Fix-up pass (V)** runs after all pools, in this order:
1. decorations, vtable `+0x54`
2. sides, `+0x08` for every side. This is `0x4473a0`: it resolves plan `+0x694` via `0x444f00`, and `+0x688` plus the lists at `+0x654` / `+0x660`.. via `0x444da0`.
3. towns `+0x144`
4. tactics `+0x144`
5. units `+0x54`
6. weapons `+0x54`
7. effects `+0x10`
8. transports `+0x54`
9. orders `+0x04`

After that come `0x443f00` (UI chain) and `0x418650` (daylight). The function returns success when the stream error byte `+4` == 0.

**Scripts (V):** `0x4455f0` has **no** fix-up loop for the scripts pool `0x4cd698`. Trigger state is restored only by the stream constructors (`0x446770`, `0x45f1c0`, `0x45f770`).

**Not traced (G):** `0x42ec90` calls `0x445570` and probably decides between loading a save and starting a new mission based on the requested path.

Unknowns: how `0x42ec90` treats `0x4dd164` for a `.mp` start; the entry point for `multi%i.sav`.

## 4. Front-end state machine

**Mechanism (V)**
- The state stack is `0x4c4a18[]` with top index `0x4c4a14`.
- Push: `0x428cd0(s)`. Replace top: `0x428d00(s)`. Back/pop: `0x428d20(0)`; at depth 0 it signals quit (`0x4c4d68`).
- `0x428e10` calls the enter hook `0x4aadf4[s]` and shows widget root `0x4c4994[s]`; the roots are built in `0x41eef0`.
- Per-state flag tables:
  - `0x4aadac`, multiplayer (`0x429000`): states 2, 3, 9, 10, 11, 12
  - `0x4aadc4`, custom mission (`0x429020`): state 6
  - `0x4aaddc`, campaign list (`0x429040`): state 1
- The main thread `0x458e40` pushes state 0 at start (at `0x458b41`).
- A button's click handler is vtable slot 4 of its class. Button constructors start at `0x427730`; each one's string ordinal = its `0x4c4f40` offset / 4.

| State | Enter hook | Meaning (G, from buttons and flags) |
|---|---|---|
| 0 | `0x41ee90` | Choose Player, single-player profiles |
| 1 | `0x41b790` (clears `0x4dd164`, builds list via `0x43f620`) | single-player menu with campaign list |
| 2 | `0x41ee90` | Choose Player, multiplayer profiles |
| 3 | `0x41b710` (clears `0x4dd164`) | multiplayer menu |
| 4 | `0x446460` | Load Game list (pushed by `0x419d80`) |
| 5 | `0x4465a0` | save list (G: in-game Save Game) |
| 6 | `0x41b870` | Custom Mission list |
| 7 | `0x41ea60` | Options |
| 8 | `0x41ec80` | sub-options (G) |
| 9 | `0x41b8d0` | multiplayer setup (pushed by `0x41a766`, `0x41aec6`) |
| 10, 11 | `0x41b810`, `0x41b000` | Create Game / joined game (pushed by `0x419e30`, `0x419e50`) |
| 12 | `0x41b000` | multiplayer; set by replace in `0x41a7ca` |
| 13 | `0x41ee70` | Credits |
| 14 | `0x41ea30` | unknown |
| 17 | `0x41ee60` | pushed by `0x419d49`; unknown |
| 18, 19, 20 | none | pushed by `0x41aa6d`, `0x41ad20`, `0x41ab8d` (G: dialogs) |
| 15, 16 | none | no push sites found |

| Button (ordinal) | Handler | Action (V) |
|---|---|---|
| Single Player (31) | `0x418b90` | replace top with 0 |
| MultiPlayer (32) | `0x418ba0` | replace top with 2 |
| Create Player (132) | `0x418cd0` | name dialog, then `0x43f2e0` |
| Remove Player (133) | `0x418dc0` | `0x43f420` |
| Credits (134) | `0x419d60` | push 13 |
| Accept Player (136) | `0x418c40` | `0x43f070`, then push 1 or 3 |
| Quit The Game (137) | `0x418bb0` | confirmation `0x429200`, then back (quits at depth 0) |
| Options (48) | `0x419d70` | push 7 |
| Load Game (138) | `0x419d80` | `0x429180(1)`, push 4 |
| Play Mission (139) | `0x419290` | disc and file checks, `0x419110` (mission AVI), store `0x4dd164`, start |
| Custom Mission (37) | `0x419e20` | push 6 |
| Accept Mission (155) | `0x41a2c0` | `0x428f30(selected path)` |
| Load This (156) | `0x41a2e0` | `0x428f30("single%i.sav")` |
| Save (157) | `0x41a340` / `0x419df0` | save selected slot / back |
| Continue Game (159) | `0x419df0` | back |
| Save Game (160) | `0x419da0` | `0x4465a0`, opens the save dialog |
| End Mission (161) | `0x419e00` | back, `0x428fa0`, back |
| Objectives (164) | `0x41a7f0` | objectives view |
| Back (141) | `0x418be0` / `0x418c00` | save profile `0x43f210` + back / back only |
| Ok (147) | `0x418c00` | back |
| Cancel (148) | `0x418c20` | `0x4296a0(1)`, back |
| Create (149) | `0x419e50` | create net game; push 11 on success |
| Start Game (150) | `0x41a270` | host start `0x43c9b0` |
| Create Game (151) | `0x419e30` | push 10 |
| Connect (153) | `0x41a740`, `0x41a790` | join |
| Disconnect (154) | `0x41a290` | leave |
| Screen resolution (142), Sampling rate (143), Acknowledgement (144), Speech style (145), Network protocol (146) | `0x419ea0`, `0x419f10`, `0x41a0c0`, `0x41a180`, `0x41a130` | cycle the option |
| Next/Prev (162/163) | `0x41a410`/`0x41a4a0`, `0x41a520`/`0x41a5b0`, `0x41a630`/`0x41a6c0` | page one of three lists |
| (no constructor found) | `0x41a830` | gamma slider: sets `0x5154e0`, calls `0x478680` |

Unknowns:
- Begin Campaign (140) has no button constructor. Either it is missing from this build or it is created by a different code pattern.
- The meaning of states 8, 14 and 15..20.
- The briefing screen (built at `0x424a9f`.., per frontend_screens.md) does not appear in the state table; it is probably a modal outside the stack.
- Which state the game returns to after a mission ends (involves `0x4c4cc0` / `0x429060` and `0x428ec0`).

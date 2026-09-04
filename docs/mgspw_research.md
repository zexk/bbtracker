# MGS Peace Walker Master Collection PC reverse-engineering reference

Reference for Peace Walker support in bbtracker. Values apply to Steam build
`24525201`. Everything under "Confirmed layout" was verified live against
quantified runs; everything still uncertain is kept in "Open items" and
"Retracted and corrected" so the same mistakes are not made twice.

## Target build

| Property | Value |
| --- | --- |
| Steam app / depot | `2492660` / `2492661` |
| Depot manifest | `1804199695039359147` |
| Executable | `mgspw/METAL GEAR SOLID PEACE WALKER.exe` |
| Product version | `1.2.0.0` |
| Architecture | PE32+ x86-64, ASLR, NX, preferred base `0x140000000` |
| Image size | `0x199e000` |
| Entry point | RVA `0x1964310` in `.bind` |

Steam rewrote PE metadata after first launch without changing any section. File
size/hash before launch were `18408520` and
`8dd0eaa5cc8d35e121612a52087399399578aeb62df1a5faff64388e7ec7a429`.
After launch size became `18408960`; repeated reads observed hashes
`52d2d18be2dd0a34c1c910deecec3abf205e35ce8f9e8a2d4eb98bba97453ba6`
and `c684d6fd44ed9262629ddc3e9d23fd4335335d5931b1ac2c52bfe6feb453cd1b`.
Section hashes below stayed identical. Whole-file hash is unsuitable as sole
build gate; use build ID plus section hashes.

| Section | RVA | Virtual/raw size | File offset | SHA-256 |
| --- | ---: | ---: | ---: | --- |
| `.text` | `0x1000` | `0x98b65c` | `0x400` | `49bb66dbc78ae81e159c0a849e3bc8acf3e8f0d831531e668c403e82c433f029` |
| `.rdata` | `0x98d000` | `0x50e24a` | `0x98bc00` | `280e835e7d8a9bda51af0eef5acb1a539499d7881e8535e52e81ebba2dae1e5c` |
| `.data` | `0xe9c000` | `0x1dfe00` | `0xe9a000` | `847e605450d5cb83782e1913b19cfa7a6d396aaba0177525e490fb48aeeb3b4b` |
| `.pdata` | `0x1886000` | `0x77454` | `0x1079e00` | `c02267dff37f1f36f93f87e0613b95f812a38c1416a7db02a679c0fbdba51625` |
| `_RDATA` | `0x18fe000` | `0xfc` | `0x10f1400` | `269596face4db8c46de4f31452acdf1afd0a39421b43fb3c36b56a2f04e919a5` |
| `.rsrc` | `0x18ff000` | `0x38508` | `0x10f1600` | `815b666e532c9a62f4fe1da7f58d420755574ab2dd7191a49643f78e1d70935c` |
| `.reloc` | `0x1938000` | `0x2b518` | `0x1129c00` | `35359003ac5aa664192be3e9882bce0ee3e60f9aad2dda2a54ead5e6e5b527f2` |
| `.bind` | `0x1964000` | `0x39248` | `0x1155200` | `69918a806fd5a745371f2fc1931ec8b42d33ff5536982ef0547ab4f1af84a8b6` |

## Executable protection

On-disk `.text` is encrypted; disassembly from RVA `0x1000` yields garbage.
The `.bind` bootstrap (entry `0x1964310`, protection stub at `0x19643D0`)
decrypts the whole code region in place before OEP handoff, and `.text` stays
plaintext for the rest of the process lifetime - a single runtime capture is
enough, and no anti-dump re-encryption was observed during play.

`.rdata`, `.data` and `.pdata` are readable on disk. `.pdata` holds 40,715
x64 `RUNTIME_FUNCTION` records covering `.text`, which recovers function
boundaries once a runtime dump exists. The PE debug directory is type `13`
(`IMAGE_DEBUG_TYPE_POGO`), so no PDB path ships. Graphics/audio imports are
Direct3D 11, DXGI, XAudio 2.9, WinMM and Media Foundation; Steam integration is
`steam_api64.dll`; there is no Direct3D 12 import.

## Runtime anchors

Resolved by AOB plus RIP displacement, no hard-coded addresses. VAs are at
image base `0x140000000`.

| Name | Pattern | Disp | Global |
| --- | --- | ---: | --- |
| `PW_SAVEROOT` | `48 8B 05 ?? ?? ?? ?? 48 05 3C BD 00 00 C3` | 3 | `0x140EA4860` |
| `PW_CHARARRAY` | `53 48 83 EC 20 48 8B 05 ?? ?? ?? ?? 48 63 D1 48 8B 0C D0` | 8 | - |
| `PW_MISSIONTIME` | `48 89 05 ?? ?? ?? ?? 41 0F BA E1 19` | 3 | - |
| `PW_MISSIONID` | `33 DB BE FF FF FF FF B9 FF FF FF 00 48 89 1D ?? ?? ?? ?? 8B EB 89 35 ?? ?? ?? ??` | 23 | `0x1414F0A00` |

`PW_SAVEROOT` holds a pointer to the save block; the matched site
(`0x140215D10`) is a getter returning `saveroot+0xBD3C`. `PW_MISSIONID` is the
current mission id, set to `-1` at mission start by `0x1401674B2` and then
loaded from script variable `0x49`; mirrors exist at `0x141140398`,
`0x14121F004` (`u16`, from name hash `0x8815F5`) and at `save+0x5244`,
`+0x1D6E4`, `+0x24BC0`.

Other globals worth keeping:

| Global | Role |
| --- | --- |
| `0x140F1B870` | player / game-state object (`+0x11BC` current weapon id, `+0x2384` a counter) |
| `0x14143B738` | mission definition table owner (`+0xB0` header, `+0xB8` rows) |
| `0x1414C72F0` | mission-record context for `0x140259C10(ctx, missionId)` clear queries |
| `0x141596A88` | `MGK_IAchievementSystem*` singleton |
| `0x14121DFE8` | Mother Base roster array, stride `0x28` |

## Save block layout

Offsets are from the block that `PW_SAVEROOT` points at. Little-endian.
Timers are 300 Hz ticks unless stated.

| Offset | Type | Meaning |
| ---: | --- | --- |
| `+0x22` | `u16` | mission-scoped counter, twinned with achievement id 11 |
| `+0x54` | `char[24]` | stage code (e.g. `w00s01a`, `my_outer`, `result`) |
| `+0x84` | `u32` | total play seconds |
| `+0x88` | `u32` | stage play, 300 Hz (mirrors `PW_MISSIONTIME+0x14`) |
| `+0x250`, `+0x264` | `u32` | best score of a score-attack mission (`8000`); mirrors at `+0x1FBCC`, `+0x1FBE0` |
| `+0x278` | `u32` | best score of another score-attack mission (`6000`); mirrors at `+0x2A0`, `+0x1FBF4`, `+0x1FC1C` |
| `+0x420..+0x45C` | `u16[]` | score config block: `9999`, `8000` x3, `1`, `6000` x3, `1`, `1000` x8, `100` |
| `+0x3C980` | `u32` | last results: run time (300 Hz) |
| `+0x3C9A8` | `u32` | last results: mission score |
| `+0x3CA20` | `u32` | last results: previous best time (drives the `NEW` tag) |
| `+0x29B4 + 4*id` | `u32` | per-mission best time, `0xFFFFFFFF` = none |
| `+0x32B4 + 2*id` | `u16` | per-mission rank: `0` = S, `1` = A, `2` = B, `3` = C, `0xFFFF` = never cleared |
| `+0x46F4 + 2*id` | `u16` | second rank array (`0x1440` after the first), all `0xFFFF` on a solo profile |
| `+0x5244` | `u32` | current mission id mirror |
| `+0x586C`, `+0x5874` | `u32` | last-mission staging twins (this run's time) |
| `+0x64EC` | `i32` | heroism, last-mission delta |
| `+0x64F4` | `i32` | heroism |
| `+0x656C` | `i32` | clear count; replays count |
| `+0xB4EC` | `u32` | narrow takedown counter feeding achievement id 10 (threshold 50) |
| `+0xB52C` | `u32` | GMP |
| `+0xBD3C` | record[] | weapon records, stride `0x1C` |
| `+0x18334` | - | titles block; earned slots read `07` |
| `+0x1C098` | `char[]` | ASCII codename of the soldier in use (e.g. `ALLIGATOR`) |

Stat descriptors also live in this block (observed around `+0x5200..+0x8000`),
but the table is reallocated between missions - always resolve by id, never by
absolute offset.

### Per-mission arrays

Both arrays and the best-time table are indexed by mission id and are written
on first clear **and** on improvement. Confirmed by watching a first clear
(id 4: rank `0xFFFF` -> `1`, time `0xFFFFFFFF` -> `218095` = 726.98 s) and a
rank improvement (id 2: rank `1` -> `0`, time `175470` -> `58635`).

The probe reads 272 entries; ids past the live list length read as zeros, so
only entries carrying a rank or a time are reported. The exact length is still
open (the list object at `0x14141C200` is null outside the mission-select UI).

`0x1401E72AC` walks the rank array to drive the mission-select
`noAlert`/`noKill` display: any rank with `rank & 0xFFFB` set clears the
aggregate `noAlert` flag, and anything but rank `1` clears `noKill`. That
screen's UI variables are `noAlert`, `noKill`, `numOuter`, `ClearOuter`
(`.rdata 0x140D209B8..`), read through the name-hash lookups `0x14011F8D0` ->
`0x1400A52E0` -> `0x1400A4A20`.

### Mission definitions

Runtime table at `0x14143B738` -> `+0xB8`, 380 rows, stride `0x24`:

| Offset | Type | Meaning |
| ---: | --- | --- |
| `+0x00` | `i16` | mission id (search key) |
| `+0x0A` | `i8` | difficulty rank, what `RANK.%d` formats via `0x140255AF0` |
| `+0x0D` | `i8` | category |
| `+0x13` | `i8` | sub-group |
| `+0x18` | `u32` | name hash (same 24-bit space as the `.PDT`/`.olang` filenames) |

`0x1402557A0(id)` finds a row; field getters sit at
`0x1402558E0..0x140255AF0` covering `+0x04`, `+0x08`, `+0x0C..0x13`, `+0x14`,
`+0x18`, `+0x1C`, `+0x1D` (five of them booleans at `+0x0D..+0x11`).

Category spread: cat 5 = ids 1-130, cat 4 = 131-219, cat 2 = 220-253,
cat 3 = 254-331, cat 1 = 332-338, cat 6 = 339-380.

Known ids:

| id | mission | stage |
| ---: | --- | --- |
| 1 | Main Op 1 | `w00s01a` |
| 2 | Main Op 2: Contact the Sandinista Comandante | `w01s04a` |
| 3 | Main Op 3: Pursue Amanda (inferred) | - |
| 4 | Main Op 4: Armored Vehicle Battle: LAV-Type G | - |
| 5 | Main Op 5: Rescue Chico | - |
| 36 | Extra Ops 005: Marksmanship Challenge | - |
| 37 | Extra Op, unidentified | - |
| 52 | Side Ops 10 | - |

**Mission id equals the Main Op number.** Ids 1, 2 and 4 match the published
Main Ops order (Main Op 4 is LAV-Type G, Main Op 2 is Sandinista
Comandante), so the whole Main Ops range can be labelled from the public
mission list without playing each one. Extra Ops use a different id range.

The probe prints the id and the stage string together, so this table grows as
missions are played.

## Stat descriptors

48-byte records, located by the `999999` bounds at both ends.

| Offset | Type | Meaning |
| ---: | --- | --- |
| `+0x00` | `u32` | bound, `999999` |
| `+0x10` | `u32` | stat id |
| `+0x18` | `i32` | this mission's tally - ticks live during play |
| `+0x20` | `i32` | career value - settles during the results tally |
| `+0x28` | `u32` | bound, `999999` |

Category matters: `0x042`/`0x442` ids keep a real tally at `+0x18`; `0x002`
ids leave junk there (large negative values), so filter to a sane range.

Confirmed ids, each pinned by runs with counted actions:

| id | meaning |
| --- | --- |
| `0x420008` | lethal takedowns (kills), total |
| `0x200E0` | per-type takedowns: assault rifle, lethal |
| `0x200E1` | per-type takedowns: sniper rifle, lethal |
| `0x200E4` | per-type takedowns: shotgun, lethal |
| `0x200E5` | per-type takedowns: rocket launcher, lethal |
| `0x200E6` | per-type takedowns: grenade, lethal |
| `0x2007C` | kills on enemies that never spotted the player |
| `0x420002` | alerts |
| `0x442002E` | non-lethal takedowns, total (body shots count, misses do not) |
| `0x200DF` | per-type takedowns: pistol, lethal |
| `0x200F9` | per-type takedowns: pistol, non-lethal |
| `0x20104` | per-type takedowns: CQC (non-lethal bank index 13), chokes and slams alike |
| `0x442007B` | CQC uses (career); a choke-then-slam sequence is one use |
| `0x2006B` | ineffective CQC (a slam that fails to stun) |
| `0x4420031` | headshots |
| `0x4420077` | heroism (equals `save+0x64F4`) |
| `0x2008E` | Fulton extractions: enemies |
| `0x2008F` | Fulton extractions: prisoners |
| `0x442011E` | missions cleared with no alerts |
| `0x442011F` | missions cleared with no kills |
| `0x442007B` | total CQC count (uses, not takedowns) |
| `0x44200DC` | missions cleared with no recovery items used |
| `0x4420030` | total hold-ups |
| `0x200ED`, `0x2002F` | non-headshot (body) kills |

The game keeps a takedown counter per weapon type - 11 of them, CQC and
stun rod included - plus lethal and non-lethal totals. `0x442002E` and
`0x420008` are those totals; the per-type counters live in the sparse block
`0x200DD..0x20110` plus strays like `0x2007C`, and a type reads `0` until it
is used, which is why only a handful are ever non-zero.

### Two banks, 26 slots each

A lethal pistol run put `0x200DF` on the board, and it fixes the block's
shape. The sparse range is `0x200DD..0x20110` - exactly 52 ids - and splits
into two 26-slot banks:

    lethal      base 0x200DD, index = weapon type
    non-lethal  base 0x200F7, same indexing

Pistol confirms the alignment: lethal `0x200DF` is index 2, non-lethal
`0x200F9` is `0x200F7 + 2`, the same index. Known indices:

| index | type | lethal | non-lethal |
| ---: | --- | --- | --- |
| 2 | pistol | `0x200DF` | `0x200F9` |
| 3 | assault rifle | `0x200E0` | `0x200FA` (predicted) |
| 4 | sniper rifle | `0x200E1` | `0x200FB` (predicted) |
| 7 | shotgun | `0x200E4` | `0x200FE` (predicted) |
| 8 | rocket launcher | `0x200E5` | - |
| 9 | grenade | `0x200E6` | `0x20100` (predicted) |
| 13 | CQC | - | `0x20104` |

The non-lethal predictions are untested; the Mosin Nagant (the only tranq
weapon besides the Mk22, dropped after Main Op 7) would settle them.

A CQC run of 2 chokes and 2 slams - one slam ineffective, since an enemy
must be choked a little before a slam actually stuns - moved `0x20104` by
`+3` and `0x442007B` by `+4`. That is takedowns against uses: the
ineffective slam counts as a use and earns no takedown. Index 13 is
therefore CQC as a weapon type, with chokes and stunning slams sharing it;
there is no lethal CQC in this game. `0x200F7`/`0x20106` stayed flat, so
those two belong to the LAV mission after all.

A follow-up run of two clean slams moved takedowns and uses by `+2` each and
left `0x2006B` alone, which pins `0x2006B` to ineffective CQC and shows a
choke-then-slam sequence counts as a single use.

Three are pinned by player-reported runs: a 7-takedown mission (6 pistol,
1 CQC) moved the total `+7` and `0x200F9` `+6`; three single-weapon
assault-rifle runs moved `0x200E0` `+3` each; a 3-kill shotgun run moved `0x200E4` off zero; and a 3-kill sniper run
moved `0x200E1` off zero.

A shotgun run (3 lethal takedowns) then corrected the earlier reading:
`0x200E4` came off zero by `+3` while `0x200E0` stayed put, so `0x200E0` is
the assault-rifle counter rather than a second copy of the kill total, and
`0x420008` alone is that total. Since the pistol counter sits at `0x200F9`
and the two lethal ones at `0x200E0`/`0x200E4`, the block looks like two
banks: `0x200Ex` lethal by type, `0x200Fx` non-lethal by type.

`0x2007C` moved `+3` on both the assault-rifle and shotgun runs but only
`+2` on the mixed 6-kill LAV run, so it spans weapon types - most likely
firearm kills as distinct from explosives and CQC.

A later CQC/stun run moved **only** `0x20104` (`5 -> 6`) and left
`0x442002E` untouched, so `0x442002E` is not a catch-all non-lethal total
and `0x20104` is the CQC/stun-side counter. That contradicts the LAV run
above unless CQC variants land in different counters, which is plausible:
CQC in this game can hold, slam or slit a throat. `0x200F7` and `0x20106`
stayed flat on the CQC run, so neither is CQC - both belong to the LAV
mission (vehicle destroyed, boss defeated).

Headshots are stored as one total, so the lethal/tranq split is a
subtraction:

    explosive kills       = grenade (0x200E6) + rocket (0x200E5)
    lethal headshot kills = kills (0x420008) - body kills (0x200ED) - explosive
    tranq headshots       = headshots (0x4420031) - lethal headshot kills

Explosive kills carry no hit location: a 3-kill rocket run left both the
headshot and the body-kill counters untouched, as did the grenade run, so
they have to be subtracted as well.

Pinned by three single-weapon runs on the same mission: 3 tranq headshots
moved `0x4420031` and `0x442002E` only; 3 lethal headshots moved
`0x4420031`, kills and `0x2007C`; 3 lethal **body** shots with that same
weapon left `0x4420031` flat and lifted `0x200ED` and `0x2002F` off zero
for the first time this profile. Career kills stood at 20 with `0x200ED` at
`0`, i.e. every kill before that run had been a headshot, which matches the
earlier 6-kill run moving `0x4420031` by 6.

`0x2007C` counts kills on enemies that never spotted the player. It took
`+3` from every clean single-weapon run regardless of weapon, `+2` from a
mixed 6-kill boss run, and `+2` from a 3-kill grenade run the player
described as two stealth kills followed by one after being found.

Heroism responds to both lethality and alerts: `+22` on a clean no-kill
no-alert clear, `+7` on the same mission with one alert, `+0` with kills.

Ids are data-driven. Only two code sites embed one as an immediate:
`0x14037B0B5` posts `0x420001..0x420004` through the event dispatcher
`0x140079210(ctx, id, valuePtr)` from an alert-state switch, and
`0x140190152` references `0x420021`.

## Timers

`PW_MISSIONTIME` layout, from a 25-minute trace (2540 samples):

| Offset | Type | Meaning |
| ---: | --- | --- |
| `+0x00` | `u64` | high-resolution total play clock at 300 Hz - **not** the mission timer |
| `+0x08` | `u64` | ticks 28-60/s depending on phase; role unknown |
| `+0x10` | `u32` | not a plain mirror of `+0x14`; diverges and can run backwards |
| `+0x14` | `u32` | equals `save+0x88` |

Per-mission time is the stage segment (total since the stage string changed).
Best times store 3.33 ms resolution while the UI floors to whole seconds, so
same-second improvements still override correctly - compare raw integers.

## Weapon records

Array at `save+0xBD3C`, stride `0x1C`:

| Offset | Type | Meaning |
| ---: | --- | --- |
| `+0x00` | `u32` | weapon id, 1-based (row `i` holds id `i+1`) |
| `+0x04` | `u32` | developed flag (`3` = developed) |
| `+0x08` | `u32` | `100` when developed |
| `+0x0C` | `u32` | stock quantity |
| `+0x14` | `u16` | XP within the current level; resets to 0 on level-up (level 1 ends at 6000, level 2 at 12000) |
| `+0x16` | `u8` | usage level (NV.1..3) |

Equipped weapon id at character `+0x14B8` matches the record id; id 3 is the
tranquilizer pistol. XP applies exactly as displayed and settles at mission end.

## Results screen

Captured live while the screen was up, for Extra Ops **005 Marksmanship
Challenge** (mission id 36). Screen showed: Heroism `+7` (total `380`),
Weapon Experience Mk.22 Mod.0 `+304` LV.2 NEXT `7560`, Clear Time NEW
`0:00:53`, Marksmanship Target Points `3300`, Clear Rank `A`.

| Field | Screen line | Verified |
| --- | --- | --- |
| `save+0x3C980` = `15930` | Clear Time `0:00:53` | 15930/300 = 53.10 s |
| `save+0x3CA20` = `17085` | drives the `NEW` tag | the previous best, 56.95 s |
| `save+0x3C9A8` = `3300` | Marksmanship Target Points | exact |
| `0x4420077` tally = `7` | Heroism `+7` | career went 373 -> 380 |
| weapon row id 3 xp `4440` | Mk.22 `+304` | 4136 + 304 |

Weapon XP thresholds follow from the `NEXT` line: level 2 ends at
**12000** (`4440` held + `7560` remaining), level 1 at **6000**. Same
arithmetic on an earlier run (`2261` held + `9739` remaining) agrees.

A second capture, on a story mission (Main Op 5, 6 pistol tranq takedowns
plus 1 CQC, 0 kills, 0 alerts, 0 near-deaths), fills in more of it:

| Field | Story run | Score run | Reading |
| --- | ---: | ---: | --- |
| `+0x3C980` | `61870` | `15930` | this run's time |
| `+0x3C9A8` | `0` | `3300` | score - zero on story missions |
| `+0x3CA00` | `7` | - | takedowns this run (6 pistol + 1 CQC) |
| `+0x3CA08` | `1` | `1` | matches the single CQC takedown |
| `+0x3CA20` | `9720000` | `17085` | previous best; `9720000` = 9:00:00 is the "no record" sentinel |
| `+0x3CA18` | `90` | `83` | moves between runs, meaning unknown |
| `+0x3CA10` | `456` | `456` | constant so far |

Story missions carry no score, so whatever drives their rank is not the
score field that score-attack missions use. Kills, alerts and near-deaths
were all zero on the captured story run, so the fields holding them are
still indistinguishable from the record's other zeros - that needs a
deliberately messy story run.

## Player character

`PW_CHARARRAY` -> `[0]` is the player object.

| Offset | Type | Meaning |
| ---: | --- | --- |
| `+0x11BE` | `u16` | health, `8000` = full; regenerates ~20/s while undamaged |
| `+0x14B8` | `u16` | equipped weapon id |

Health was pinned by two hits (`8000 -> 7420 -> 6429`) plus the refill
between samples. `+0x32C` dropped `1632 -> 1568` on the first hit and then
never moved again, so it is not health.

## Achievement system

50 achievements. All addresses are VAs.

| Item | Address | Role |
| --- | --- | --- |
| singleton | `0x141596A88` | `MGK_IAchievementSystem*` |
| vtable | `.rdata 0xE13EA8` | `MGK_SteamAchievement` |
| metadata table | `.data 0x14105C230` | 50 rows, stride `0xC` |
| predicate table | `.data 0x141596A90` | `bool(*)()` per id |
| registrar | `0x1400388D0` | installs 49 predicates at init |
| poll loop | `0x140038BF4` | pops queued ids, runs predicate, unlocks |
| `Notify(id)` | `0x140038CC0` | what gameplay code calls |

Metadata row at `0x14105C230 + 12*id`: `+0x0 u32` Steam number (`id + 2`),
`+0x4 u32` internal id, `+0x8 u8` enabled, `+0x9 u8` queued, `+0xA u8` unlocked,
`+0xB` pad. Steam key format is `ACH_QXS_%03d` (`.rdata 0xE13DF8`) of the Steam
number, so id 0 = `ACH_QXS_002` ... id 49 = `ACH_QXS_051`.

Methods (call sites use `+0x20`/`+0x28` because the object vptr sits 8 above
the dumped vtable):

| Address | Role |
| --- | --- |
| `0x14006B790` | acquires `ISteamUserStats` via `STEAMUSERSTATS_INTERFACE_VERSION012` |
| `0x14006BA30` | `Unlock(id)`: validate, skip if unlocked, dedupe, push to pending vector |
| `0x14006BB20` | `IsLocked(id)` |
| `0x14006BB80` | count of enabled and unlocked rows |
| `0x14006BC46` | flush: `SetAchievement(name)` per pending id (SDK vtable `+0x38`) |
| `0x14006BD0A` | refresh: `GetAchievement(name, &row[+0xA])` for all 50 (`+0x30`) |

Predicate map (`0x14002A4E0` is the shared stub - those ids are story unlocks
fired by direct `Notify`):

| id | Steam key | predicate | condition |
| ---: | --- | --- | --- |
| 0-5 | `ACH_QXS_002..007` | `0x140039010..060` | `0x140038E70(n)`: rank-array check over a small mission-id set |
| 6-9, 13-21, 39-41 | - | `0x14002A4E0` | stub |
| 10 | `ACH_QXS_012` | `0x140039B80` | `save+0xB4EC >= 50` |
| 11 | `ACH_QXS_013` | `0x1400392A0` | global `0x1415969F4 >= 50` |
| 12 | `ACH_QXS_014` | `0x1400392C0` | `player+0x2384 >= 100` |
| 22, 23 | `ACH_QXS_024/025` | `0x140039960`, `0x140039A40` | five mission ids each, all cleared (rank `<= 3`) |
| 24 | `ACH_QXS_026` | `0x1400398B0` | `0x1401C39F0() != 0` |
| 25 | `ACH_QXS_027` | `0x1400398E0` | any of five `0x1401B8Cxx()` flags |
| 26 | `ACH_QXS_028` | `0x1400397A0` | bit 26 of `0x1400FA8E0()` |
| 27 | `ACH_QXS_029` | `0x1400397C0` | 3 consecutive mission ids cleared |
| 28 | `ACH_QXS_030` | `0x140039670` | 10 mission ids (`0x191..0x19A` or `0x188..0x191`) cleared |
| 29, 30 | `ACH_QXS_031/032` | `0x140039920`, `0x140039940` | `save+0x33EE` / `+0x33F0` (`u16`) `== 0` |
| 32-35 | `ACH_QXS_034..037` | `0x140039370`, `0x1400393D0`, `0x1400393A0`, `0x1400392F0` | staff-count sums per group `>= 100` (`>= 300` for id 35) |
| 37, 38 | `ACH_QXS_039/040` | `0x1400395F0`, `0x1400394B0` | gated on ids `0x25`/`0x26` still locked |
| 42 | `ACH_QXS_044` | `0x140039840` | mission ids `0xE1..0xE8` all cleared |
| 43-48 | `ACH_QXS_045..050` | `0x140039070`, `0x1400390A0`, `0x140039430`, `0x1400390C0`, `0x140039140`, `0x1400391E0` | "clear them all" family over the 132-id list at `.rdata 0x140D8F5D0` |

Staff-count getters are `0x1400E3B60(group, 0)` and `0x1400E3A90(group, 0)`.
`save+0xB4EC` is incremented at `0x1405FCB73` in takedown handler
`0x1405FCB00`, which then calls `Notify(10)`; the handler gates on the hit's
body-part word (`obj+0x24`), the player's weapon (`player+0x11BC`, valid when
`< 9`) and a flag bit in `obj+0x18`.

`scripts/pwach.py` reproduces the metadata and predicate tables.

## Static anchors

For `.rdata`, `RVA = file offset + 0x1400`.

| RVA | String / role |
| ---: | --- |
| `0x98f5b0` | `MGSPW_WIN32` |
| `0x98fc58` | `[total_play_time]` |
| `0x990758` | `launcher MGSPW` |
| `0x9bd8f0` | `../mgspw_savedata_win` |
| `0xd209a8` | `noAlert` |
| `0xd209b0` | `noKill` |
| `0xd21890` | `mission_select` |
| `0xd23bf0` | `chart%02d_rank_01` |
| `0xd400d8` | `rank_up_%02d` |
| `0xd400f8` | `[SW]list_nokill_%02d` |
| `0xd40158` | `RANK.%d` |
| `0xe13dd0` | `STEAMUSERSTATS_INTERFACE_VERSION012` |
| `0xe13df8` | `ACH_QXS_%03d` |

MSVC RTTI is readable: 146 type descriptors including `WinStorage`,
`BaseStorage<WinStorage>`, `KeyConfigSsvIO`, engine resource/render classes and
the Steam callback types. Retained PSP paths (`ms0:/PSP/SAVEDATA/ULUS10509DAT`)
sit next to the PC save root, so port code kept much of the PSP terminology.

## Installed data and saves

Main directory holds the executable, `steam_api64.dll`,
`sdkencryptedappticket64.dll`, font XPRs and TXP resources. The launcher is a
separate Unity/IL2CPP app under `launcher/`; its `GameAssembly.dll` is not
gameplay code.

Data archives are named by the same 24-bit hash as the mission name field:
`MLG/disc0_rel/*.PDT|.DAT|.KEY`, `MLG/Text/*.olang`, `Text/*.txp`. All sampled
content is encrypted (entropy ~7.96 bits/byte), so mission and rank vocabulary
cannot be read off disk.

The per-user save directory holds `usersv` (exactly `0x1000` bytes,
first 256 bytes encrypted or obfuscated, no `MGSS` magic) and
`steam_autocloud.vdf`. The run save `STW00000092e301` (325,968 B) is fully
encrypted: no dword of any known live value appears anywhere in it. Disk-side
auditing is therefore not available; live diffing is the method.

## Tools

`nix develop .#re` provides `ghidra-bin` and `capstone`.

| Script | Purpose |
| --- | --- |
| `probe-mgspw-memory.py` | live snapshot, `--idmap` stat table, `--rate`/`--trace`, `--dump-text` |
| `pwdis.py` | overlays the `.pdata` function map on a runtime `.text` dump, disassembles, xrefs strings and immediates |
| `pwwatch.py` | records which save-block slots move across a session (how the live tallies were found) |
| `pwach.py` | dumps the achievement metadata and predicate map |
| `pwhash.py` | name-hash helper for the script-variable lookups |
| `find-mgspw-counter.py` | Cheat-Engine style snap/diff value scan (how `PW_MISSIONID` was found) |
| `scan-mgspw-strings.py`, `rtti-mgspw-ach.py` | string and RTTI enumeration |

Typical session:

```sh
python3 scripts/probe-mgspw-memory.py --dump-text /tmp/pw_text.bin
python3 scripts/pwdis.py --text /tmp/pw_text.bin --xref-string noKill
python3 scripts/pwwatch.py --seconds 1800 --out /tmp/pw_watch.json
```

## Test protocol

All tooling is read-only: the ASI only reads, the `/proc` scripts never write,
save handling is copy-out backups. Peace Walker autosaves after missions and
on-disk GMP matches live GMP, so live snapshots plus reported results-screen
numbers are a sound baseline method - no manual-save discipline needed.

Snapshot immediately before and after each reported mission. Sessions-apart
baselines let an unreported mission slip into the delta, which is exactly how
the retracted weapon-XP multiplier below was invented.

## In-game Mission Stats screen

The historic-data screen unlocks partway through the campaign and labels
several counters outright. Matched against a live read:

| Screen line | Value | Id |
| --- | ---: | --- |
| Missions Cleared with No Alerts | 23 | `0x442011E` |
| Missions Cleared with No Kills | 22 | `0x442011F` |
| Missions Cleared with No Recovery Items Used | 33 | `0x44200DC` |
| Total CQC Count | 23 | `0x442007B` |
| Fulton Extractions | 73 | `0x2008E` (71 enemies) + `0x2008F` (2 prisoners) |
| Total Hold-ups | 6 | `0x4420030` |
| Total Headshots | 115 | `0x4420031` |
| Heroism | 1029 | `0x4420077` |
| missions cleared | 39 | `save+0x656C` |

Two labels were wrong before this: `0x44200DC` is the no-recovery-item
clear count rather than a general clear counter, and `0x4420030` is
hold-ups rather than anything prisoner-related. The Fulton line also
confirms the enemy/prisoner split, since the screen shows their sum.

## Open items

- **Rank formula.** Rank is scored and the score path resolves everything
  through the script VM by name hash, so the thresholds live in the encrypted
  archives, not in code. Data points so far: mission 2 S at 195.45 s (0 kills,
  0 alerts), mission 2 A at 584.90 s, mission 4 A at 726.98 s (0 kills, alerts
  raised).
- **The `+0x278` / `+0x250` twins** look like per-mission best scores for
  score-attack missions, but that is one test short of confirmed. Every
  observation fits: they moved `4758` -> `6000` when a `6000` run scored S on
  Marksmanship Challenge, and they did **not** move on a later `3300` run on
  the same mission, which is exactly best-score behaviour. Confirm by beating
  `6000` there and watching `+0x278` follow. The `6000` value colliding with
  the Mk22 level-1 XP cost is a coincidence.
- **Score thresholds.** `3300` scored A and `6000` scored S on Marksmanship
  Challenge, so that mission's S line is at most `6000`. The `+0x420` config
  block (`9999`, `8000` x3, `6000` x3, `1000` x8, `100`) is the obvious place
  for per-rank or per-target values but has not been tied to a rank yet.
- **Career id `0x20023`** moved on only 2 of 5 runs (`+6000` on a first clear,
  `+4945` on a dirty replay), so it is not a plain cumulative score.
- **The remaining weapon types.** Identified: pistol non-lethal (`0x200F9`),
  assault rifle lethal (`0x200E0`), sniper lethal (`0x200E1`), shotgun lethal
  (`0x200E4`). The lethal bank is contiguous from `0x200E0`, so the gaps at
  `0x200E2`/`0x200E3` are two types not yet used (SMG is one - the profile
  owns none yet).
  Each type appears to have both a lethal and a non-lethal slot. Each unused type reads `0`, so a run using one
  Each unused type reads `0`, so a run using one type for 2-3 takedowns
  lights up exactly one slot. Next: SMG, sniper, LMG, rockets, grenades, plus
  the non-lethal slots for the types already found.
- (resolved) `0x2007C` is kills on unaware enemies - see the stat table.
- **`0x442002E` scope.** CQC takedowns never touch it (three runs now,
  including a story mission where 6 pistol takedowns moved both it and the
  pistol counter by exactly 6 while a CQC takedown moved neither), so it
  counts tranq-weapon takedowns rather than all non-lethal ones. The LAV
  run's `+7` against a pistol `+6` is most likely a miscounted seventh
  pistol takedown rather than CQC feeding the total.
- **`0x2006B`** is settled as ineffective CQC, not slam takedowns: it moved
  `+1` on the run with one failed slam and stayed flat through a run of two
  clean slams.

- **`0x2002F` vs `0x200ED`** both moved `+3` on the body-shot run and are so
  far indistinguishable.
- **`0x200F9`** tracked `0x442002E` for three runs, then diverged (`+6` vs
  `+7`), so the two are not the same counter.
- **`0x442007B`** moved `+1` on the one main-op clear and on nothing else -
  main-op or boss clear counter, single observation.
- **`0x200F7`, `0x20106`** each moved `+1` on the boss clear only.
- **`save+0x22` and global `0x1415969F4`** (achievement id 11, threshold 50)
  incremented on a dirty run and reset to `0` on the next mission, so they are
  mission-scoped; the event at `0x14017084E` is unidentified.
- **Alert tally.** The `0x420002` career value is confirmed, but that record's
  `+0x18` sequence did not line up with the reported alert counts, so only the
  career reading is trusted.
- **`char+0x32C`** fell once on damage and then stayed put; unidentified.
- **`save+0x9084` was never the S-rank count.** It read `3` while three S
  ranks were held, then `260` against 39 clears and 5 S ranks. S counts come
  from the per-mission rank array instead.
- **`save+0x130`** (reads `229`, static through Fulton uses) is not Fulton
  stock and remains unidentified. `save+0xB550` (`150` against a displayed
  food `151%`) and `save+0xB520` (`131079`) are likewise unexplained.
- **Rank array length** and the id-to-name mapping beyond ids 1, 2, 4, 52.
- **`usersv`** structure, and whether `KeyConfigSsvIO` is really its role.
- **11-char string near `save+0x140CF`** is not ranks (see below) and is still
  unidentified; it did not change across replays.

## Retracted and corrected

Kept deliberately: each of these was believed once and cost time.

- **Rank letters as ASCII near `save+0x140CF`** - withdrawn. PW ranks are only
  S/A/B/C, so `ECEDFGCCCCA` cannot be ranks, and the reported mission 5 = S
  contradicts position 5 = F.
- **Weapon-XP S-multiplier / first-S bonus** - retracted. The `+871` was two
  missions, an unreported one plus Side Ops 5; XP applies exactly as displayed
  in every clean sample.
- **`0x2008E` disproven as Fulton** - wrong, and now reversed. It is Fulton
  recoveries: `+1` on each of three separate 1-Fulton runs and `+8` on a main
  op where the results screen itself showed 8 extractions. The original
  "disproven" call rested on a profile number that counted something else.
- **`save+0x130` as Fulton stock** - disproven, static across Fulton uses.
- **`char+0x8A0` as player health** - disproven. It reads `393216000` as a
  dword, and the probe's `u16` view of it was a constant `0` in game.
- **`save+0x278` as the mission score** - withdrawn. Regular missions show no
  score on the results screen, so whatever these twins hold, it is not the
  number the rank is scored from. The overlay row was pulled back into
  Forensics under its raw offset.
- **`save+0xB4EC` as the Headshot Hero counter** - disproven. It reads `5`
  while career headshots are 71 and the achievement (threshold 50) is already
  unlocked; it is a narrower takedown counter.
- **"Nothing ticks live mid-mission except the clocks"** - corrected. The
  career value settles at the results tally, but the descriptor's `+0x18`
  tally ticks live: sampled every 2 s through a 6-kill run it stepped
  `0,1,0,1,2,4,5,6` while the career value made one `8 -> 14` jump at settle.
  The overlay no longer needs client-side baseline latching.
- **`save+0x32B4`/`+0x46F4` as item development levels** - corrected. They are
  the per-mission rank arrays; the achievement predicates that "check level
  `<= 3`" are checking that a mission is cleared with any rank.
- **`save+0x2A84` as a lone Side Ops 10 slot** - superseded. It is
  `0x29B4 + 4*52`, an entry in the per-mission best-time array.
- **`+0x1C1E4`** ruled out early as a non-monotonic staging transient, and an
  early "sleep ID" set (`0x442006E` and friends) was retracted as hex misreads
  of the same three ids.
- **Vehicle-boss escort counter at RVA `0x158CC48`** - the surrounding block
  turned out to be HUD/camera state written by the reset routine
  `0x140564260`, not a mission-stat accumulator.

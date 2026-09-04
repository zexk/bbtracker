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
| `PW_STATARRAY` | `48 83 EC 58 48 0F BF C1 48 8D 0C 80 48 8B 05 ?? ?? ?? ?? 0F 10 44 C8 10` | 15 | `0x14121DFE8` |

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
| `0x14121DFE8` | stat descriptor array pointer; `record = *ptr - 0x10 + (id & 0xFFFF) * 0x28` |

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
| `+0xB520` | `u32[]` | bitfield block; `0x1400F9810` sets bit `0x200` in the first two dwords |
| `+0x140CF..+0x140D9` | `u8[11]` | grade bytes, `0x40 \| value`; defaults `5 3 5 4 6 7 3 3 3 3 1` |
| `+0x18334` | `u8[226]` | comm-message state; `0` locked, `5` Heroism-gated and earned, `7` free (block sized `0x180`) |
| `+0x182E4` | nibbles | packed comm-message field, read `& 0xF` and `>> 4 & 0xF` |
| `+0x184B4` | record[18] | equipped comm messages, stride `0x18`, `{seq u32, id s16, flags u16}` |
| `+0x1C098` | `char[]` | ASCII codename of the soldier in use (e.g. `ALLIGATOR`) |

Stat descriptors also live in this block (observed around `+0x5200..+0x8000`),
but the table is reallocated between missions - always go through the array
pointer and the id index, never an absolute offset.

### Per-mission arrays

Both arrays and the best-time table are indexed by mission id and are written
on first clear **and** on improvement. Confirmed by watching a first clear
(id 4: rank `0xFFFF` -> `1`, time `0xFFFFFFFF` -> `218095` = 726.98 s) and a
rank improvement (id 2: rank `1` -> `0`, time `175470` -> `58635`).

**Rank and best time are independent bests.** On mission 52 the rank slot
held S while the best-time slot took `26.23 s` from a run that scored only
A - the game keeps the best of each separately, so the pair must not be
displayed as though it came from one run.

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
| 6 | Main Op 6: Pursue the Jungle Train | - |
| 7 | Main Op 7: Tank Battle: T-72U | - |
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

One flat array of `0x28`-byte records, indexed directly by the low 16 bits of
the stat id - the low bits *are* the slot, and the high bits (`0x002`, `0x042`,
`0x442`) are category. The game's own getter at `0x1400E3A90` does:

```
record = *(0x14121DFE8) - 0x10 + (id & 0xFFFF) * 0x28
```

and bounds-checks the index against `0x123`, so the table is 291 records. The
`-0x10` is because the global points at `record+0x10`; the getter's operand
carries the `+0x10` instead. Verified against eight known ids - pistol lethal
and non-lethal, CQC, kills, tranq total, heroism, headshots, no-alert clears -
each landing on its own record. `PW_STATARRAY` resolves the global by AOB on
the getter, which matches exactly once.

| Offset | Type | Meaning |
| ---: | --- | --- |
| `+0x00` | `u32` | bound, `999999` |
| `+0x10` | `u32` | stat id |
| `+0x18` | `i32` | this mission's tally - ticks live during play |
| `+0x20` | `i32` | career value - settles during the results tally |

The `999999` that appears `0x28` bytes after a record's own bound is the *next*
record's bound. Reading the pair as one record's frame is what made these look
like 48-byte records, and why the probe scanned for them until now.

Category matters: `0x042`/`0x442` ids keep a real tally at `+0x18`; `0x002`
ids leave junk there (large negative values), so filter to a sane range.

Confirmed ids, each pinned by runs with counted actions:

| id | meaning |
| --- | --- |
| `0x420008` | lethal takedowns (kills), total |
| `0x200E0` | per-type takedowns: assault rifle, lethal |
| `0x200E1` | per-type takedowns: sniper rifle, lethal |
| `0x200E2` | per-type takedowns: LMG, lethal |
| `0x200FB` | per-type takedowns: sniper rifle, non-lethal (Mosin) |
| `0x200E4` | per-type takedowns: shotgun, lethal |
| `0x200E5` | per-type takedowns: rocket launcher, lethal |
| `0x200E6` | per-type takedowns: grenade, lethal |
| `0x200E8` | per-type takedowns: placed explosive, lethal |
| `0x2007C` | kills on enemies that never spotted the player |
| `0x20023` | career damage taken, same 0-8000 scale as health |
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
| `0x44200DC` | missions cleared with no recovery items used |
| `0x4420030` | total hold-ups |
| `0x200ED`, `0x2002F` | non-headshot (body) kills |

The game keeps a takedown counter per weapon type - 11 of them, CQC and
stun rod included - plus lethal and non-lethal totals. `0x442002E` and
`0x420008` are those totals; the per-type counters live in the sparse block
`0x200DD..0x20110` plus strays like `0x2007C`, and a type reads `0` until it
is used, which is why only a handful are ever non-zero.

### Two banks, 26 slots each

The sparse range `0x200DD..0x20110` is exactly 52 ids and splits into two
26-slot banks that share one index:

    lethal      base 0x200DD, index = weapon type
    non-lethal  base 0x200F7, same indexing

| index | type | lethal | non-lethal |
| ---: | --- | --- | --- |
| 2 | pistol | `0x200DF` | `0x200F9` |
| 3 | assault rifle | `0x200E0` | `0x200FA` (predicted) |
| 4 | sniper rifle | `0x200E1` | `0x200FB` |
| 5 | LMG | `0x200E2` | `0x200FC` (predicted) |
| 7 | shotgun | `0x200E4` | `0x200FE` (predicted) |
| 8 | rocket launcher | `0x200E5` | - |
| 9 | grenade | `0x200E6` | `0x20100` (predicted) |
| 11 | placed explosive (C4) | `0x200E8` | `0x20102` (predicted) |
| 13 | CQC | - | `0x20104` |

**The model is confirmed by prediction.** `0x200FB` was written here as the
predicted sniper slot before the profile owned a Mosin Nagant; a 2-takedown
Mosin run later lifted exactly that address off zero. The remaining non-lethal
predictions follow from the same arithmetic.

CQC occupies index 13 with no lethal counterpart - there is no lethal CQC in
this game. `0x20104` counts CQC takedowns and `0x442007B` counts CQC uses: an
ineffective slam (an enemy must be choked a little first) is a use with no
takedown, and a choke-then-slam sequence is a single use. `0x2006B` counts
ineffective CQC alone.

Headshots are stored as one total, so the lethal/tranq split is a subtraction.
Explosive kills carry no hit location - neither a rocket run nor a grenade run
moved the headshot or body-kill counters - so they subtract out too:

    explosive kills       = grenade (0x200E6) + rocket (0x200E5)
    lethal headshot kills = kills (0x420008) - body kills (0x200ED) - explosive
    tranq headshots       = headshots (0x4420031) - lethal headshot kills

Heroism responds to both lethality and alerts: `+22` on a clean no-kill
no-alert clear, `+7` on the same mission with one alert, `+0` with kills.

Ids are data-driven. Only two code sites embed one as an immediate:
`0x14037B0B5` posts `0x420001..0x420004` through the event dispatcher
`0x140079210(ctx, id, valuePtr)` from an alert-state switch, and
`0x140190152` references `0x420021`.

Two loose ends here. `0x200F7` is the non-lethal bank base but behaves like a
mission counter, staying flat on CQC runs and moving on the LAV mission
alongside `0x20106`. And one CQC/stun run moved `0x20104` without moving
`0x442002E`, while an earlier LAV run moved both; CQC variants may land in
different counters.

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

## Archive encryption (broken)

The Master Collection wraps every shipped archive in an extra layer the PSP
and PS3 releases do not have, which is why Jayveer's
[Chrysalis](https://github.com/Jayveer/Chrysalis) fails on these files in
both `-psp` and `-ps3` modes (it also needs building 64-bit: the released
binary is 32-bit and the slot archive is 519 MB).

The layer is an **MT19937 keystream XOR seeded from a hash of the file
name** - no block cipher, which is why no AES or SHA tables appear anywhere
in the executable. Dmytro Bidlov's [Peace Walker Localization
Tool](https://github.com/LittleBitUA/PEACE-WALKER-LOCALIZATION-TOOL)
implements it (`src/pwcrypt.py`) along with the container formats, so there
is no reason to reimplement any of it.

With that, `MLG/disc0_rel/002aba34.DAT` + `.KEY` (the Master Collection's
renamed `SLOT.DAT`/`SLOT.KEY`) unpacks cleanly: 2137 of 2137 blocks, 1.3 GB,
no failures, and `slottext.export` pulls 10,074 strings out of it.

### Mission names

Mission titles live in SLOT element `002FF/8`, 182 entries. Anchored against
missions this profile actually cleared:

| element index | mission id | title |
| ---: | ---: | --- |
| 3 | 1 | OPENING/INVESTIGATE THE SUPPLY FACILITY |
| 4 | 2 | CONTACT THE SANDINISTA COMANDANTE |
| 5 | 3 | PURSUE AMANDA |
| 6 | 4 | ARMORED VEHICLE BATTLE: LAV-TYPE G |
| 7 | 5 | RESCUE CHICO |
| 8 | 6 | PURSUE THE JUNGLE TRAIN |
| 9 | 7 | TANK BATTLE: T-72U |
| 36 | 36 | [EXTRA OPS: 005] |
| 52 | 52 | [EXTRA OPS: 010] |

Main Ops sit at `index - 2` (five cleared missions confirm it) while Extra
Ops are index-aligned - id 36 is the Marksmanship Challenge whose results
screen read "[005]", and id 52 is the Fulton Recovery op. The two sections
therefore need separate rules; why the offset resets is not yet understood.

`scripts/pwtext.py` drives the extraction end to end.

### Rank thresholds and insignia tiers: searched, not found

Both are data-driven and neither turned up in the decrypted archive:

- element types across all 2137 blocks are `olang`, `mdb`, `txp`, `mtar`,
  `mtsq`, `sep`, `eft`, `la3`, `ypk`, `vrd`, `bin`, `mdpe`, `lt2`, `vcp`,
  `dcd`, `ohd`, `geom`, `vlm`, `eqp`, `nav`; the small parameter-ish ones
  (`bin`, `eqp`, `dcd`, `vlm`, `nav`, 409 elements) contain no 25/50/100
  tier pattern;
- the insignia block `001FC` holds only `olang` text plus `ypk`/`ohd`;
- insignia descriptions are templates - "over `$1` headshots (Heroism
  increases by `$2`)" - so the numbers live in a table elsewhere;
- the executable has no `insignia`, `emblem`, `medal` or `award` strings at
  all, and the one promising `.data` table at `0x1033300` is 5-dword records
  carrying typed ids (`0x20000200`-style), i.e. R&D or dialogue triggers.

The `.cnf` script data was the next lead - Chrysalis parses those files, and
its issue #6 asks the same question about `.slot` lines, so it is unsolved in
the community too. It does not survive contact with the Master Collection
data either:

- the 92 files the PDT catalogue names `scenerio.gcx` are DAR archives, not
  GCL bytecode (`0023_scenerio.gcx` opens on its first DAR row, `ICON0.png`),
  so the GCX decompiler cannot read them under either spelling;
- no `data.cnf` exists in any of the 142 QAR archives, and no `.cnf` member in
  the 837 files extracted from the scenario DARs. Chrysalis uses `data.cnf` as
  the old STAGEDAT stage index, which the PDT extraction has already passed;
- no extracted member holds the little-endian float `0.9` or `1.1`, so if the
  reported balance rule is exact it uses integer arithmetic or an indirect
  table.

The remaining anchor is runtime code, not another archive pass. The live
codename text at `save+0x1C098` does not help reach it: searching decrypted
`.text` for the displacement `0x1C098` yields only instruction-byte
coincidences, because the save pointer and field address are formed
indirectly.

### Comm-message database

The CO-OPS comm messages (ally radio lines) are a 226-record database. It was
mistaken for the codename and insignia tables for a while, so it is documented
here in full - and it is *not* where codename requirements live.

Records are `0x80` bytes:

| offset | type | meaning |
| ---: | --- | --- |
| `+0x04` | `u32` | sequential record id |
| `+0x08` | `s16` | config id, `50..289` |
| `+0x0A` | `u16` | behavior flags |
| `+0x0C..+0x14` | five `s16` | requirement tuple, one value per tier; axes unresolved |
| `+0x16` | `s16` | usually `35`; role unresolved |
| `+0x18` | `s16` | solo / cooperation selector (`0` or `1`) |
| `+0x1A` | `s16` | record family, `1..8` |
| `+0x1C` | `s32` | Heroism floor; `-1000000` means always available |
| `+0x20` | `char[]` | asset name, `v909_NNN_spr_sna_1` |
| `+0x40` | `char[]` | the message text, e.g. `GO! GO! GO!`, `Grenade!` |

Native access path:

| step | detail |
| --- | --- |
| owner global | `0x14143B738`, a pointer; null until the database loads |
| header | `owner+0x220`, record count as `s32` at `header+8` |
| rows | `owner+0x228`, record `i` at `rows + (i << 7)` |
| accessors | `0x140255E90` (record by index, bounds-checked), `0x140255ED0` (count) |

Save-side state, all one flat index space over the same 226 records:

| offset | meaning |
| --- | --- |
| `save+0x18334` | `u8[226]` state, in a block cleared to `0x180` bytes by `0x14015CEB0` |
| `save+0x182E4` | packed nibbles, read `& 0xF` and `>> 4 & 0xF` |
| `save+0x184B4` | 18 equipped slots, `{seq u32, id s16, flags u16}`, stride `0x18` |

State bits are `1` registered, `2` default-unlocked, `4` Heroism floor met, so
`7` is a free message, `5` a Heroism-gated one now earned, and `0` never
activated. `0x14039C110` maintains bit `4`: it walks all 226 records, skips
those without bit `1`, reads Heroism through `0x1400E3950` (stat id `0x77`),
and compares against `record+0x1C`.

None of these accessors appears in `.pdata` - they are leaf functions with no
unwind data. `--disasm` and `--find-imm` cannot see them, which is why early
searches for `0x18334` and `0x182E4` returned zero hits while the code using
those displacements sat in plain sight. Use `pwdis.py --raw VA` here, and read
a zero result from the `.pdata`-driven commands as "not covered", never as
"absent".

Codenames use the evaluator documented below. Localization gives their
descriptions at index `24 + codename_index`: `FOXHOUND` is index 13,
description 37, all weapon types / cooperation / non-lethal force;
`BUTTERFLY` is index 20, description 44, short-range / solo / non-lethal.

### Title system wiring

GCL scripts reach native code only through dispatch tables in PE `.data`. One
record is 16 bytes:

| offset | type | meaning |
| ---: | --- | --- |
| `+0x00` | `u32` | 24-bit script command hash |
| `+0x04` | `u32` | always `0` |
| `+0x08` | `u64` | native function VA |

Records run in tables of strictly ascending hash; `pwdis.py --cmd-tables`
recovers **80 tables, 28,075 records** from a stock exe. The hash is the only
stable name a command has, so this table is the entry point for any "what does
the script call here" question.

Read the stride as 8 bytes and every pairing shifts by one record while still
looking plausible; see "Retracted and corrected". Pairings that matter here:

| command hash | native function | role |
| --- | --- | --- |
| `0x26DF41` | `0x14039BC70` | activation: sets state bit `1` at one index |
| `0xDEC231` | `0x14039BC10` | bulk activation of `flags & 4` records, `state \|= 3` |
| `0xF3148A` | `0x14039C190` | writes the grade-byte defaults, below |
| `0x28A553` | `0x1400F9810` | unrelated: a save-flag setter |

`0x14039BC70` reads argument hash `0x000E2F` (`no`), finds the record whose
`s16 +0x08` id matches, and ORs state bit `1` at that index. The bound it
checks is `0x180`, matching the `0x180`-byte state block cleared by
`0x14015CEB0` - so the array is sized for 384 entries while only 226 are used.

`0x1400F9810` sets bit `0x200` in the dwords at `save+0xB520` and
`save+0xB524`.

**The activation command is not in the result script.** `STAGEDAT/0175_result.rlc`
(204,140 B) has zero aligned 32-bit hits for `0x26DF41`. Its one aligned hit,
at file offset `0x2AC7F`, is `0x28A553` - the save-flag setter. So the result
script sets a save flag, and nothing pins title activation to it.

### Grade bytes

`0x14039C190` (script command `0xF3148A`) writes a small block of defaults. It
first zeroes `save+0x140CE..+0x140DC` and the qword at `save+0x182E4`, then
ORs a value and the constant `0x40` into each byte:

| offset | `+0x140CF` | `D0` | `D1` | `D2` | `D3` | `D4` | `D5` | `D6` | `D7` | `D8` | `D9` |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| value | 5 | 3 | 5 | 4 | 6 | 7 | 3 | 3 | 3 | 3 | 1 |

Each byte is a packed field: low 6 bits carry the value, and a two-bit field
above them is read back with `sar 2` / `and 3` on `+0x140CE`. `save+0x182E4`
is a packed nibble array over the same subsystem, read by leaf getters at
`0x14039C530` (`>> 4 & 0xF`), `0x14039C550` (`& 0xF`) and `0x14039C570`
(`& 0xF` at `+0x182E6`).

Because every value is 1-7 with `0x40` set, the block reads as the ASCII
`ECEDFGCCCCA` - the "rank letters" string that sat unexplained for weeks. It
was never text.

## Codename system

The evaluator is `0x1401E7F40`. Its only direct caller is `0x1401E6560`, which
passes an output block at `object+0x56E4` and mission/result data at
`object+0x43D0`. The evaluator contains 24 direct calls to the codename granter,
one for each codename id.

| Piece | Address | Role |
| --- | --- | --- |
| ownership | `save+0x1BFF0 + id` | one byte per codename, id `1..24`; bit `0` owned, bit `1` seen, bits `2..4` grade |
| evaluator | `0x1401E7F40` | computes candidate grade for all 24 codenames and returns changed ids/grades |
| caller | `0x1401E6560` | runs evaluator and sends its output to the result UI |
| current grade | `0x1405448C0(id)` | returns `0` if unowned, otherwise grade `1..7` |
| granter | `0x140544B20(id, grade)` | sets ownership and raises stored grade; bounds-checked to 24 ids |
| owned count | `0x1405446D0` | counts owned codenames `1..24` |

Evaluator input table at `0x140D209F0` is four groups of twelve career-stat
ids. Same position in every group is one weapon class:

| Axis | Stat ids | Live-profile check |
| --- | --- | --- |
| kills | `0xDD..0xE8` | scattered lethal weapon totals |
| sleeps | `0xEA..0xF5` | handgun slot is `145`, matching tranq takedowns |
| stuns | `0xF7..0x102` | CQC slot is `28`, matching the observed CQC/stun total |
| incapacitations | `0x104..0x10F` | all zero on the research profile |

The evaluator sums all four axes per slot and makes a 12-bit mask containing
every slot tied for the largest total. Five normal weapon predicates consume
that mask:

| category | dominant-mask bits (zero based) |
| --- | --- |
| stun rod | `1` |
| CQC | `0` |
| explosive | `8, 9, 11` (`0x1B00`) |
| long range | `4` |
| medium range | `3, 5` (`0x28`) |
| short range | `2, 6, 7` (`0xC4`) |

Stun rod and CQC each have only a non-lethal title pair. Other four categories
each have lethal and non-lethal pairs. `0x140543E40(id, field)` proves labels:
its 24-way switch returns localization entity hashes, which join directly to
the name and description entities in `slot/001FC/3`.

Lethality split is exact. Let `K` be the sum of twelve kill counters and `N`
the sum of sleeps, stuns, and incapacitations. Non-lethal titles require
`N > 2*K`; lethal titles require `N <= 2*K`. Ties therefore count as lethal.

Each ordinary predicate has two title ids. First id below uses low camaraderie;
second uses high camaraderie. These are evaluator ids, not localization
indices:

| category | force | solo / cooperation title (id) |
| --- | --- | --- |
| stun rod | non-lethal | EEL (`24`) / FIREFLY (`8`) |
| CQC | non-lethal | BEAR (`2`) / KANGAROO (`14`) |
| explosive | lethal | ORCA (`16`) / PIRANHA (`17`) |
| explosive | non-lethal | OCTOPUS (`15`) / WHALE (`22`) |
| long range | lethal | HAWK (`12`) / RAVEN (`19`) |
| long range | non-lethal | SWALLOW (`21`) / GULL (`11`) |
| medium range | lethal | PUMA (`18`) / WOLF (`23`) |
| medium range | non-lethal | CAT (`5`) / DEER (`6`) |
| short range | lethal | SCORPION (`20`) / BEE (`3`) |
| short range | non-lethal | BUTTERFLY (`4`) / ANT (`1`) |

Four all-weapons pairs are HOUND (`13`) / DOBERMAN (`7`) for lethal and FOX
(`9`) / FOXHOUND (`10`) for non-lethal, again solo / cooperation. Their spread
predicate is exact and unlike old tracker guess. Game computes
`average = total / 11.0` and requires every
one of twelve slot totals to satisfy
`abs(slot - average) <= average * 0.1`. A zero-total profile passes this spread
test internally, but other grade conditions prevent a useful award.

Cooperation grade input is `result+0x24 / result+0x20` when denominator is
positive, otherwise zero. Ratio gates are `> 0.05`, `> 0.5`, and `>= 1.0`.
Result block is persistent inside Player Data object at `object+0x43D0`;
object pointer is `*0x14143A8D8`. Builder `0x1401E7210` reads script variable
`numMis`, stores `2*numMis - 27` at `+0x20`, walks mission records, and stores
counted records at `+0x24`. It initializes flags `+0x28/+0x29` true, clears
them according to stored rank values, then clears both unless counted records
reach required count. `+0x2A` marks completed builder result. Evaluator uses
`+0x28` for grade 5 and `+0x29` for grade 4. Probe reads these native fields;
it does not duplicate mission-record filtering.

Second axis is camaraderie aggregate at snapshot `+0x30`, built by
`0x140544D20`. It calls `0x140367FB0` for online table at
`*0x140EA4870 + 0x5008`, reads count at table start, and sums the dword at
`table + 0x110*(i+1)` for every entry. Ordinary low-camaraderie ids use
`<= 10k/50k/100k/200k/500k`;
high ids use strict `>` at same steps. Grade 4 and 5 additionally require
result flags `+0x29` and `+0x28`; grades 3-5 require cooperation ratio at least
1, grade 2 requires over 0.5, grade 1 over 0.05.

All-weapons grades add Heroism (`stat 0x77`) floors of `10k`, `50k`, `100k`,
`150k`, and `250k`. Low-camaraderie variants require camaraderie at or below
their matching normal threshold; high variants require it above. Evaluator
only calls `0x140544B20` when candidate grade exceeds stored grade, so grades
never decrease.

## Insignia system

There are **84 catalogued insignias**. The evaluator and save format contain
110 internal insignia ids, but the Player Data source feeding the Insignias
screen has a fixed 84 rows; its denominator comes directly from that row count.
The 26 omitted ids are:

`28..31`, `39..41`, `43`, `80..82`, `86..88`, and `96..107`.

These are not reserved slots: the evaluator contains grant paths, thresholds,
heroism awards and ownership bytes for them. They are hidden from the catalog.
Whether every hidden id remains obtainable in this port is unknown. Installed
English localization has labels for 95 ids and holes for 15, further showing
that the internal 110-id set is not the displayed catalog.

| Piece | Address | Role |
| --- | --- | --- |
| ownership | `save+0x1C009 + index` | one byte per insignia, index `1..110`; bit `0` owned, bit `1` seen |
| evaluator | `0x1401E9BB0` | reads the career counters and evaluates all 110 records; called through `0x1401E65B0` from `0x1401E4AA2` / `0x1401E53D0` |
| threshold lookup | `0x140543810(index, field)` | `field 0` = threshold, `field 1` = heroism award |
| already-owned | `0x1405448F0(index)` | non-zero means skip |
| granter | `0x140544B80(index)` | ORs the ownership bits, bounds-checked to `0x6D` |
| screen source | runtime Player Data records keyed by `0x2B0C75` | fixed 84-row list; each row maps through the table at `0x140F80600` to an internal id |
| screen predicate | `0x140544C00(obj, index)` | tests a 14-byte bitfield at `obj+0x22` of the UI object, `bit = index-1` |

The grant test is strict: `value > threshold`, which is why the descriptions
read "over `$1`" and why 100 headshots does not fire Headshot Master but 101
does.

Thresholds are data, not code. `0x140543810` builds a `110 x 3` dword table on
the stack from `.rdata` constants around `0x140D40DF0` and returns
`arr[(index-1)*3 + field]`. Reconstructing that table by emulating its stores
recovers every requirement; `scripts/pwinsig.py` does it.

Validated against a live profile: for the 84 records whose stat id is
recovered, ownership equals `career > threshold` in 83 cases. The four held on
the research profile line up exactly - no-alert clears `36 > 25`, no-recovery
`51 > 50`, headshots `145 > 100`, Fulton `104 > 100` - and the no-recovery one
was watched firing mid-session for `+500` heroism, `+522` total with the clear.

The one disagreement is index 53, stat `0x117`. The 84 stat-mapped records and
84 screen rows are different sets whose equal counts are coincidental.


## Installed data and saves

Main directory holds the executable, `steam_api64.dll`,
`sdkencryptedappticket64.dll`, font XPRs and TXP resources. The launcher is a
separate Unity/IL2CPP app under `launcher/`; its `GameAssembly.dll` is not
gameplay code.

Data archives are named by a 24-bit hash: `MLG/disc0_rel/*.PDT|.DAT|.KEY`,
`MLG/Text/*.olang`, `Text/*.txp`. They look like noise on disk (entropy ~7.96
bits/byte) but the encryption is broken - see "Archive encryption".

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
| `pwdis.py` | overlays the `.pdata` function map on a runtime `.text` dump, disassembles, xrefs strings and immediates; `--raw` for functions `.pdata` omits, `--xref-mem` for globals, `--cmd-tables` for the script-command dispatch map |
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

Script-command lookups need only the on-disk exe, no running game:

```sh
python3 scripts/pwdis.py --cmd-tables                     # all 28,075 records
python3 scripts/pwdis.py --cmd-tables --cmd-hash 0x26DF41 # who serves a hash
python3 scripts/pwdis.py --cmd-tables --cmd-ptr 0x14039BC70  # and the reverse
python3 scripts/pwdis.py --text /tmp/pw_text.bin --raw 0x14039C190 --raw-len 0x400
```

`--self-test` checks that the `0x26DF41 -> 0x14039BC70` pairing still
resolves, which is enough to catch a game update moving the tables.

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

Further screens (Player Overview, the rest of Mission Stats, VS Stats,
Insignias) add context rather than ids: Total Level of Camaraderie, all
CO-OP lines and every VS line read `0`, which plausibly accounts for a large
share of the ~180 descriptor ids that sit at zero. No screen lists per-weapon
takedowns, so the game keeps those internally and the run-per-weapon method
remains the only way to name them.

Insignia thresholds are strict: the no-alert insignia reads "over 25
missions without being spotted" and did **not** fire at 25, only at 26. The
same holds for Headshot Master, which sat unfired at exactly 100 headshots.

**Insignias award heroism**, so a run's heroism delta mixes the mission
award with any insignia crossed during it and cannot be read as a pure
mission result. "Headshot Master" grants `+500` for passing 100
headshots; a second insignia produced `522`, i.e. `22` for the clean
clear plus the award. `save+0x64EC` mixes both.

Insignia state is at `save+0x1C009 + index` - see "Insignia system". The
earlier hunt missed it because the byte that moved in the whole-block diff,
`save+0x18372`, is comm-message index 62 crossing its Heroism floor, and
because the search assumed an 84-entry array rather than 110 one-byte slots.

## Open items

- **Cheapest controlled codename transition on the current research profile.**
  Live values are 145 tranq-weapon takedowns, 26 CQC takedowns and 40 kills;
  handgun is already the dominant exact weapon type (142 non-lethal, 3
  lethal). Under the reported `sleep + stun + incapacitation - 2*kills`
  classifier, at least 46 additional lethal handgun takedowns would cross the
  known-input balance from positive to non-positive while leaving short range
  dominant, making the solo short-range lethal codename the cheapest likely
  new award. This is only a lower bound because the still-unmapped
  incapacitation input may be nonzero. Raising another weapon type above the
  handgun count costs at least 123 assault-rifle kills, 117 CQC takedowns or
  about 141 uses for an almost-unused type, so those are worse breakpoint
  triggers. FOXHOUND itself is not a practical trigger yet: Heroism is 2,036
  against the reported first-tier requirement above 10,000, before its CO-OPS
  Camaraderie gate.

- **Rank formula.** Thresholds live in the encrypted script data, not in
  code, so only observation constrains it. Story missions carry no score
  field at all (it reads `0`), so their letter comes from the run itself.
  Time dominates, with alerts costing a grade:

  | mission | time | alerts | rank |
  | ---: | ---: | ---: | --- |
  | 52 | ~32 s | 0 | S |
  | 52 | 26.23 s | some | A |
  | 52 | slow | 0 | C |
  | 2 | 195.45 s | 0 | S |
  | 2 | 584.90 s | ? | A |
  | 6 | 343.82 s | ? | A |
  | 7 | 345.93 s | ? | S |

  Missions 6 and 7 scored differently at nearly identical times, so the
  thresholds are per mission.
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
- **`0x20106`** moved `+1` on the LAV boss and `+1` on the T-72U tank battle,
  so armored-vehicle bosses defeated fits. `0x420080` came off zero by `+2`
  on the tank mission and is unexplained.
- **The unclaimed bank slots.** Index 6 is most likely SMG and 10 the other
  placed type (claymore/mine); this profile owns neither. Indices 0, 1 and
  12+ are unclaimed, as is the non-lethal bank beyond pistol, sniper and
  CQC. Each unused type reads `0`, so a run using one type for 2-3
  takedowns lights up exactly one slot.
- **`0x442002E` scope.** CQC takedowns never touch it (three runs now,
  including a story mission where 6 pistol takedowns moved both it and the
  pistol counter by exactly 6 while a CQC takedown moved neither), so it
  counts tranq-weapon takedowns rather than all non-lethal ones. The LAV
  run's `+7` against a pistol `+6` is most likely a miscounted seventh
  pistol takedown rather than CQC feeding the total.
- **`0x2002F` vs `0x200ED`** both moved `+3` on the body-shot run and are so
  far indistinguishable.
- **`save+0x22` and global `0x1415969F4`** (achievement id 11, threshold 50)
  incremented on a dirty run and reset to `0` on the next mission, so they are
  mission-scoped; the event at `0x14017084E` is unidentified.
- **Alert tally.** The `0x420002` career value is confirmed, but that record's
  `+0x18` sequence did not line up with the reported alert counts, so only the
  career reading is trusted.
- **`char+0x32C`** fell once on damage and then stayed put; unidentified.
- **`save+0x130`** (reads `229`, static through Fulton uses) is not Fulton
  stock and remains unidentified. `save+0xB550` (`150` against a displayed
  food `151%`) is likewise unexplained.
- **Rank array length** and the id-to-name mapping beyond ids 1, 2, 4, 52.
- **`usersv`** structure, and whether `KeyConfigSsvIO` is really its role.
- **Who writes the grade bytes during play.** `0x14039C190` only installs the
  defaults; nothing is yet known to update `save+0x140CE..+0x140DC` or the
  `+0x182E4` nibbles afterwards. A write watchpoint on that block across a
  mission result is the next trace, and it is the most direct lead left.
- **What consumes the `+0x0C..+0x14` requirement tuple.** The five `s16`
  values scale per tier, so something compares them against play statistics.
  Find that reader and the same shape probably explains codename scoring.
- **NEXT UNKNOWN — Codename grade presentation.** Probe now reads all evaluator inputs:
  48 career axes, camaraderie aggregate, result cooperation numerator and
  denominator, and grade-4/5 flags. Overlay still presents title only; add
  current/candidate grade display if useful.

## Retracted and corrected

Kept deliberately: each of these was believed once and cost time.

- **Rank letters as ASCII near `save+0x140CF`** - identified instead as grade
  bytes, see "Grade bytes". A packed field with a high bit set reads as
  plausible text for free.
- **`save+0x18334` as a codename array with insignias at `+0x1834C`** - it is
  one 226-entry comm-message array. Both systems index from zero, so the join
  looked clean, and every "insignia" reading landed 24 entries off: the
  controlled no-alert unlock that appeared to move insignia 38 actually moved
  message index 62 as career Heroism passed its floor of 1,000. Cost several
  sessions of watchpoints. Check a record's text field before trusting an
  index join between two zero-based tables.
- **FOXHOUND at 40,000 Heroism, BUTTERFLY at 6,000** - both retracted with the
  above; they are message-unlock floors. No codename requirement figure is
  known.
- **Title activation as script command `0x28A553`** - retracted. Dispatch
  records are 16 bytes, not 8, so an 8-byte read at `0x140EDAD88` paired
  `0x14039BC70` with the next record's hash. Activation is `0x26DF41`;
  `0x28A553` is a save-flag setter.
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
- **`save+0x9084` as the S-rank count** - disproven. It read `3` while three
  S ranks were held, then `260` against 39 clears and 5 S ranks. S counts come
  from the per-mission rank array.
- **Vehicle-boss escort counter at RVA `0x158CC48`** - the surrounding block
  turned out to be HUD/camera state written by the reset routine
  `0x140564260`, not a mission-stat accumulator.

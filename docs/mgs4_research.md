# MGS4 Master Collection PC reverse-engineering reference

Technical reference for MGS4 support in bbtracker. Values apply to Steam depot
build `24921893` unless stated otherwise.

## Target build

| Property | Value |
| --- | --- |
| Steam app / depot | `2492670` / `2492671` |
| Depot manifest | `1875973235293599215` |
| Executable | `MGS4/mgs4.exe` |
| SHA-256 | `9e8df67ea7f41e7f8306ce1a77584707209069b3c75389b3f00445efe459fe41` |
| Internal version | `Pela_[MPA]_x64_BGFX_0.0.3_Release_ww_[Code]a84606af_[DataNew]d06ab525_2026_0825` |
| PE timestamp | 2026-08-25 04:21:59 UTC |
| Architecture | PE32+ x86-64, ASLR, NX, preferred base `0x140000000` |

| Section | RVA | Virtual size | File offset | Notes |
| --- | ---: | ---: | ---: | --- |
| `.text` | `0x1000` | `0x165cf80` | `0x400` | Protected on disk; plaintext after bootstrap |
| `.rdata` | `0x165e000` | `0x4a1002` | `0x165d400` | Strings, RTTI, imports |
| `.data` | `0x1b00000` | `0x224fb238` | `0x1afe600` | Large zero-filled virtual tail |
| `.pdata` | `0x23ffc000` | `0x109614` | `0x1d6b800` | x64 unwind records |
| `.bind` | `0x24184000` | `0x39248` | `0x1ef2000` | Protection bootstrap and entry point |

PE entry point is RVA `0x24184310`. On-disk `.text` is unsuitable for normal
disassembly. Runtime capture after bootstrap produced coherent x86-64. Captured
virtual `.text` range `[base+0x1000, base+0x165df80)` has SHA-256
`82a452826735bca1f59cf0197c5f0eb4285188e92ca0cdaacb5245a2b2ad0bb2`.
Corresponding on-disk bytes hash to
`fbf8bc654bf323c44f0b35e05abf397c12639f70b2f4ee56ef2ff37616136faf`.

## Runtime integration

MGS4 loads Ultimate ASI Loader as `MGS4/winmm.dll`. Proton launch option:

```text
WINEDLLOVERRIDES="winmm=n,b" %command%
```

`bbtracker_mgs4.asi` supports Direct3D 11 and Direct3D 12. Direct3D 12 support
hooks DXGI `Present`/`ResizeBuffers` and
`ID3D12CommandQueue::ExecuteCommandLists`, captures direct command queue, and
uses per-backbuffer command allocators, render targets, and fences. Both
renderers were live-validated under Proton.

While MGS4 is running under Proton, set live Drebin-point balance with:

```sh
sudo python3 scripts/probe-mgs4-memory.py --set-drebin 100000000
```

Output reports `drebin_points` for verification.

## Live state pointer

```text
module base + 0x1C28B28 -> active linkvarbuf
```

| RVA | Meaning |
| ---: | --- |
| `0x1c28b28` | active `linkvarbuf` pointer |
| `0x1c28b38` | active `varbuf` pointer |
| `0x1c28b30`, `0x1c28b40` | snapshot pointers |

Active `varbuf` was `0x2800` bytes before active `linkvarbuf`. Snapshots retained
older progress, confirming `0x1c28b28` selects live state.

Probe accepts buffer only when difficulty is `20`, `30`, `35`, `40`, or `50`,
scenario progress is at most `291`, and stage is a seven-character campaign
code in families `s00` through `s05`, `s10`, `s20`, or `s30`. Menu stages,
standalone Mission Briefing stages (`r_sna*`), and unknown `sNN` families return
unavailable. Output is cleared before population to prevent stale values.

## `linkvarbuf` layout

All multibyte values are little-endian. Timers use 60 Hz ticks.

| Offset | Type | Meaning |
| ---: | --- | --- |
| `0x0000` | `uint32` | completed-playthrough count |
| `0x0004` | `uint16` | title-initialization field; domain unknown |
| `0x0006` | `uint16` | difficulty |
| `0x0034` | `char[7]` | stage code |
| `0x0054` | `uint32` | scenario progress |
| `0x0158` | `uint16` | continues |
| `0x0168` | `uint32` | total play time |
| `0x016e` | `uint16` | alert phases |
| `0x0178` | `uint16` | kills |
| `0x017a` | `uint16` | special-item-use bitmask: bit 0 Bandana, bit 1 Stealth Camouflage |
| `0x0180` | `uint16` | CQC uses; shared by BEAR and ASSASSIN |
| `0x0182` | `uint16` | headshots |
| `0x0184` | `uint16` | knife kills |
| `0x0186` | `uint16` | knife knockouts |
| `0x0188` | `uint16` | prone side rolls |
| `0x018a` | `uint16` | forward rolls |
| `0x018c` | `uint16` | Combat Highs |
| `0x018e` | `uint16` | weapon pickups |
| `0x0190` | `uint16` | item pickups |
| `0x0192` | `uint16` | hold-ups |
| `0x0194` | `uint16` | body searches |
| `0x0196` | `uint16` | praises |
| `0x0198` | `uint16` | items donated |
| `0x019a` | `uint16` | Syringe uses |
| `0x019c` | `uint16` | Scanning Plug uses |
| `0x019e` | `uint16` | Playboy pages turned |
| `0x01a0` | `uint16` | Emotion Magazine pages turned |
| `0x01a8` | `uint32` | crouch time |
| `0x01ac` | `uint32` | crawl time |
| `0x01b4` | `uint32` | wall-press time |
| `0x01b8`, `0x01bc` | `uint32` | box/drum timer components |
| `0x01c0` | `uint32` | spendable Drebin-point balance |
| `0x01c4` | `uint32` | unknown Drebin-related value; unchanged after spending 5,000,000 points |
| `0x01d4` | `uint16[95]` | weapon states, IDs `0..94` |
| `0x0350` | `uint16[68]` | separate inventory-related array |
| `0x0526` | `uint16[99]` | item states, IDs `0..98` |
| `0x0ae0` | `uint16` | recovery items used |
| `0x5a34` | `uint16` | flashbacks viewed; target `273` |

Native predicate aggregates:

```text
knife defeats       = u16[0x184] + u16[0x186]
pickups              = u16[0x18e] + u16[0x190]
Syringe/Plug uses    = u16[0x19a] + u16[0x19c]
magazine pages       = u16[0x19e] + u16[0x1a0]
box/drum time        = u32[0x1b8] + u32[0x1bc]
```

## Difficulty and progress

| Raw | Display name | Tracker tier |
| ---: | --- | --- |
| `20` | Liquid Easy | Very Easy |
| `30` | Naked Normal | Easy |
| `35` | Solid Normal | Normal |
| `40` | Big Boss Hard | Hard |
| `50` | The Boss Extreme | Extreme |

| Progress | Act |
| ---: | --- |
| `0..50` | Act 1 |
| `51..100` | Act 2 |
| `101..162` | Act 3 |
| `163..222` | Act 4 |
| `223..261` | Act 5 |
| `262..291` | ending/results |

## Stage codes

Names come from shipped `common/stage/select/scenerio.gcx`. Combined names mean
one internal stage covers multiple visible locations.

| Code | Area |
| --- | --- |
| `s00a00l` | Prologue Cemetery |
| `s00a10l` | Ending Cemetery |
| `s01a00l`, `s01a05l` | Middle East Infiltration |
| `s01a10l` | Red Zone |
| `s01a20l` | Militia Safehouse |
| `s01a30l` | Urban Ruins |
| `s01a40l` | Advent Palace |
| `s01a50l`, `s01a55l` | Crescent Meridian |
| `s01a57l` | Millennium Park |
| `s01a60l` | Liquid's Encampment |
| `s02a10l` | Cove Valley Village |
| `s02a20l`, `s02a25l` | Power Station |
| `s02a30l` | Confinement Facility |
| `s02a40l` | Vista Mansion |
| `s02a50l` | Research Lab |
| `s02a60l` | Mountain Trail / Riverside |
| `s02a70l` | Vamp Ambush |
| `s02a73l`, `s02a75l`, `s02a78l` | Stryker Escape |
| `s02a80l` | High Woodlands Highway |
| `s02a85l` | Marketplace Entrance |
| `s02a90l` | Marketplace |
| `s02a95l` | Marketplace Plaza |
| `s03a00l` | Eastern Europe Station |
| `s03a10l`, `s03a15l` | Midtown: Resistance Tail |
| `s03a16l` | Midtown: Canals |
| `s03a20l` | Midtown: Plaza |
| `s03a25l` | Midtown: North Sector |
| `s03a30l` | Church Courtyard |
| `s03a35l`, `s03a40l`, `s03a60l` | Motorcycle Chase |
| `s03a50l` | Raging Raven Ambush |
| `s03a65l`, `s03a70l` | Echo's Beacon |
| `s03a90l` | Volta River |
| `s04a05l` | Metal Gear Solid Flashback |
| `s04a10l` | Snowfield / Heliport / Tank Hangar |
| `s04a20l` | Nuclear Warhead Storage Building |
| `s04a30l` | Snowfield / Communications Tower |
| `s04a40l` | Blast Furnace / Casting Facility |
| `s04a50l` | Underground Base |
| `s04a60l` | Underground Supply Tunnel |
| `s04a65l` | REX Escape |
| `s04a68l` | Port Area |
| `s04a70l` | Port Area: REX vs. RAY |
| `s04a75l` | Outer Haven Arrival |
| `s05a10l` | Ship Bow |
| `s05a20l` | Command Center / Missile Hangar |
| `s05a30l` | Microwave Corridor |
| `s05a40l` | GW |
| `s05a45l` | Liquid Ocelot: Prelude |
| `s05a50l` | Liquid Ocelot |
| `s05a55l` | Liquid Ocelot: Aftermath |
| `s10a10l` | Nomad Mission Briefing |
| `s10a20l` | Nomad: South America Briefing |
| `s10a30l` | Nomad: Eastern Europe Briefing |
| `s10a40l` | Nomad: Shadow Moses Briefing |
| `s20a00l` | USS Missouri |
| `s20a10l` | USS Missouri vs. Outer Haven |
| `s20a20l` | Campbell's Room |
| `s30a00l` | Wedding |
| `s30a10l` | Hospital |

Live-validated codes: `s01a10l`, `s01a20l`, `s01a30l`, `s02a10l`,
`s02a20l`, `s02a25l`. Other names are exact selector entries.

## Emblem predicates

Final-result script `ww/stage/stage00/s00a10l/cache/00180720.gcx`, procedure
`122`, evaluates all 40 emblems in priority order.

| # | Emblem | Predicate |
| ---: | --- | --- |
| 1 | BIG BOSS | Extreme; alerts/kills/continues/recovery `=0`; time `<=5h`; no special item |
| 2 | FOX HOUND | Hard+; alerts `<=3`; kills/continues/recovery `=0`; time `<=5.5h`; no special item |
| 3 | FOX | Solid Normal+; alerts `<=5`; kills/continues/recovery `=0`; time `<=6h`; no special item |
| 4 | HOUND | Naked Normal+; alerts `<=10`; kills/continues/recovery `=0`; time `<=6.5h`; no special item |
| 5 | MANTIS | alerts/continues/recovery `=0`; time `<=5h` |
| 6 | WOLF | continues/recovery `=0` |
| 7 | RAVEN | time `<=5h` |
| 8 | OCTOPUS | alerts `=0` |
| 9 | BEAR | CQC uses `>=100` |
| 10 | EAGLE | headshots `>=150` |
| 11 | ASSASSIN | knife defeats `>=50`; CQC uses `>=50`; alerts `<=25` |
| 12 | PIGEON | kills `=0` |
| 13 | BLUE BIRD | items donated `>=50` |
| 14 | HAWK | praises `>=25` |
| 15 | LITTLE GRAY | 69 counted weapons excluding optional ID 11 |
| 16 | ANT | body searches `>=50` |
| 17 | GIBBON | hold-ups `>=50` |
| 18 | TORTOISE | box/drum time `>=60m` |
| 19 | RABBIT | magazine pages `>=100` |
| 20 | BEE | Syringe/Scanning Plug uses `>=50` |
| 21 | GECKO | wall-press time `>=60m` |
| 22 | SCARAB | prone side rolls `>=100` |
| 23 | FROG | forward rolls `>=200` |
| 24 | INCH WORM | crawl time `>=60m` |
| 25 | LOBSTER | crouch time `>=150m` |
| 26 | HYENA | weapon/item pickups `>=400` |
| 27 | HOG | Combat Highs `>=10` |
| 28 | PIG | recovery items `>=40` |
| 29 | COW | alerts `>=100` |
| 30 | CROCODILE | kills `>=400` |
| 31 | GIANT PANDA | time `>=30h` |
| 32–39 | regular grid | alerts split `75`; kills `250`; continues `25` |
| 40 | CHICKEN | alerts `>=150`; kills `>=500`; continues/recovery `>=50`; time `>=35h` |

Regular grid low means `<=75` alerts, `<=250` kills, `<=25` continues; high
means `>`. Rows are SCORPION/TARANTULA/CENTIPEDE/SPIDER for low alerts and
JAGUAR/PANTHER/LEOPARD/PUMA for high alerts, with kill and continue high/low
forming remaining axes. Game awards all matches and displays highest priority.

## Weapon count and native commands

| GCX hash | Preferred VA | Role |
| ---: | ---: | --- |
| `0x030e20` | `0x140064640` | query weapon state |
| `0x619034` | `0x1400645e0` | query item state |
| `0x3ec885` | `0x1400646f0` | set item/weapon state |
| `0x6a5831` | `0x140069db0` | count acquired weapon types |
| `0x5b316e` | `0x14005c780` | `NewSaveVariable` |
| `0x0cda58` | `0x1414bba50` | grant emblem/title |
| `0xdcab44` | `0x1414bb960` | grant result reward |
| `0x3db09f` | `0x140094040` | title-initialization query |

Weapon query supports IDs `0..94`; item query supports `0..98`. Weapon count
scans IDs `1..73` and counts states `1` or `2`. LITTLE GRAY requires 69 counted
weapons excluding ID 11 (1911 Custom): 69 without it, or 70 including it.
Tracker skips ID 11 and tests `>=69`.

## Bonus DP

Difficulty multiplier `m`: Liquid Easy `1`, Naked/Solid Normal `2`, Big Boss
Hard `5`, The Boss Extreme `10`.

- play time: `15,000m` through exactly 5h; then decreasing `10,000m` through
  12.5h; then zero;
- continues: `10,000m` at zero; decreasing from `5,000m` for `1..25`;
- alerts: `20,000m` at zero; decreasing from `10,000m` for `1..25`;
- kills: `20,000m` at zero; decreasing from `10,000m` for `1..50`;
- recovery items: same as continues;
- weapons: `10,000m` at `>=69`; positive lower counts scale from `5,000m`
  with denominator 68;
- flashbacks: `10,000m` at `>=273`; positive lower counts scale from `5,000m`
  with denominator 272;
- special items: `5,000m` only when `u16[0x17a] == 0`.

Interpolation uses integer division, rounds down to 10 DP, and clamps positive
sub-10 values to 10. Exactly 5h awards `15,000m`; next tick enters `10,000m`
branch. Maximum is `100,000m`, or 1,000,000 DP on Extreme.

## Fast-load and stage-selector micro-mod

```text
--stage VALUE -> fastLoad.stage
```

Bare `--stage` is ignored. Registered codes load directly. `--stage select`
black-screens because selector GCX ships but retail executable lacks selector
stage-init registration. Steam launcher does not forward trailing arguments.

`micromods/mgs4-stage-select/` installs reversible launcher shim plus separate
`mgs4_stage_selector.asi`. F6 opens named picker; restart writes
`Launcher/mgs4-stage-selector.ini` and relaunches game. Selector shares no code
or state with bbtracker.

Shipped `config/mgs4.dev.ecf` XOR key is
`MGS4ConfigFileSecureKey@2024`. Key index for byte `i`:

```text
(i + floor(i / 28)) mod 28
```

## Save formats

Root: `mgs4_savedata_win/<SteamID>/mgs4/`. Run slots use
`BLJM67001G<CREATION_EPOCH_HEX>`; global save uses `BLJM67001S`.

### `MGS4.SAV` and `MGS4SYS.SAV`

Remove final LF, preserve final eight-byte plaintext footer, and XOR body byte
`i` with key byte `i % 79`:

```text
kjdyeAiwoGsklcmfu93lwsENf7845ghw523if0ul7Pkj0hn9ejwksSVE8twf03te623DA842rc4oiQL
```

| File | Body | Footer |
| --- | ---: | --- |
| `MGS4.SAV` | `0x8344` | `XPQT3Q5\0` |
| `MGS4SYS.SAV` | `0x1f8` | `XQQT5Q3\0` |

`MGS4SYS.SAV` is system state, not run statistics. Decoded run-save word at
`0x8340` appears to be integrity data; algorithm remains unknown. Do not write
modified saves.

### `METADATA.SAV`

Plain `0x80` bytes plus LF:

| Offset | Type | Meaning |
| ---: | --- | --- |
| `0x00` | `uint32[9]` | 24-bit hashed stage/resource IDs |
| `0x24` | `uint32` | version, observed `1` |
| `0x28` | `uint32` | Unix save timestamp |
| `0x2c` | `uint32` | body length `0x8344` |
| `0x30` | `uint32` | difficulty |
| `0x34` | `uint32` | whole-second play time |
| `0x38` | `uint32` | unresolved |
| `0x3c` | `uint32` | Drebin points |
| `0x40` | `uint32` | save-menu progress resource index |
| `0x44` | `uint32` | scenario progress copy |
| `0x48` | `byte[56]` | zero/reserved |

### Launcher `usersv`

One `0x1000`-byte record plus LF:

| Offset | Size | Meaning |
| ---: | ---: | --- |
| `0x000` | 4 | `MGSS` |
| `0x004` | 2 | CRC-16/IBM |
| `0x006` | 6 | reserved |
| `0x00c` | 4 | plaintext PRNG seed |
| `0x010` | `0x400` | 256 signed `int32` settings |
| `0x410` | `0xbf0` | zero padding |

CRC polynomial `0xa001`, initial zero, no final XOR, covers `[0x010,0x1000)`.
Fixed PRNG table gives `0x800`-byte keystream period. Read-only decode:

```text
P[i] = C[i] ^ C[0x804+i]  for 0x000 <= i < 0x00c
P[i] = C[i] ^ C[0x800+i]  for 0x010 <= i < 0x410
```

## VPAK archives

Version-3 header:

| Offset | Type | Meaning |
| ---: | --- | --- |
| `0x00` | `char[4]` | `VPAK` |
| `0x04` | `uint16` | version `3` |
| `0x06` | `uint16` | flags, observed `0`/`1` |
| `0x08` | `uint32` | entry count |
| `0x0c` | `uint32` | trailing index size |

Index starts at `file_size-index_size`. Entry fields: `uint32` path size,
`uint16` path flags (`0` plain, `0x100` LZ4), UTF-8 path, zero `uint64`,
decompressed `uint64` size, stored `uint64` size, absolute `uint64` data offset,
`uint32` chunk size, `uint32` chunk count, zero `uint64`, then cumulative
`uint64[chunk_count-1]` compressed ends. Compressed chunks are raw LZ4 blocks.
Layout validated across 16 archives and 80,979 entries.

## GCX format

71 files decompile in MGS4 mode with [Jayveer/Gcx](https://github.com/Jayveer/Gcx).
Framing: Unix timestamp, `uint32` procedure offsets ending in `0xffffffff`, then
20-byte block header:

| Offset | Meaning |
| ---: | --- |
| `0x00` | procedure-block offset |
| `0x04` | resource-table offset |
| `0x08` | string-resource data offset |
| `0x0c` | string-resource end/procedure start |
| `0x10` | string-resource cipher seed; zero means plain |

Resource count is `(string_offset-table_offset)/4`. Table word low 24 bits are
resource offset; high byte has type flags. Final resource ends at procedure
offset. Cipher per byte with uint32 wrap:

```text
seed = seed * 0x7D2B89DD + 0xCF9
byte ^= (seed >> 15) & 0xFF
```

High-value scripts:

| Path | Content |
| --- | --- |
| `common/stage/select/scenerio.gcx` | stage catalog and debug setters |
| `ww/stage/stage00/s00a10l/cache/00180720.gcx` | emblems and Bonus DP |
| title scripts | resets, Act anchors, DLC setup |

Jayveer/Gcx 1.0 exporter reads past final resource. For result GCX, resource
`13508` has 18 valid bytes; larger output tail is process-memory leakage.

## Achievements

Main executable owns `ACH_001..ACH_033`; launcher owns `ACH_034` screenplay
achievement. Launcher screenplay has 309 English pages and 219 Japanese pages;
completing either language suffices. Persistent key is `ScenarioSave`.

Main executable RTTI identifies callbacks owned by
`SteamAchievementsManager@achieve@virtuos`. GCX reward IDs passed to command
`0xdcab44` are internal unlocks, not Steam IDs.

## Static anchors

`.rdata` ASCII; RVA equals file offset plus `0xc00`:

| File offset | RVA | String |
| ---: | ---: | --- |
| `0x18d6f10` | `0x18d7b10` | `LIQUID EASY` |
| `0x18d6f20` | `0x18d7b20` | `NAKED NORMAL` |
| `0x18d6f30` | `0x18d7b30` | `SOLID NORMAL` |
| `0x18d6f40` | `0x18d7b40` | `BIG BOSS HARD` |
| `0x18d6f50` | `0x18d7b50` | `THE BOSS EXTREME` |
| `0x18e0548` | `0x18e1148` | `Kill Count : %d` |
| `0x18e0560` | `0x18e1160` | `Alert Count : %d` |
| `0x18e0578` | `0x18e1178` | `Continue Count : %d` |
| `0x18e05a8` | `0x18e11a8` | `Total Play Time : %02d:%02d:%02d` |

CodeView PDB path:

```text
E:\Bola\source-code\out\x64_BGFX_Steam\source\main\Release\mgs4.pdb
```

## Repository tools

```sh
python3 scripts/inspect-mgs4-save.py /path/to/MGS4.SAV
python3 scripts/inspect-mgs4-save.py --self-test
python3 scripts/probe-mgs4-memory.py
python3 scripts/probe-mgs4-memory.py --dump-text /tmp/mgs4-text.bin
python3 scripts/probe-mgs4-memory.py --self-test
python3 scripts/list-vpak.py /path/to/archive.pak
```

Tools are read-only.

## Validation and unknowns

Live-validated: ASI loading, D3D11/D3D12, active pointer, stage changes,
difficulty, core rank counters, non-grey feat counters, several areas, direct
fast-load, and gamepad compatibility.

Static/script/save-validated without positive live sample: nonzero special-item
field, flashback overlay value, and individual identities of paired aggregates.

Open items:

- stable signature/version gate for pointer after executable updates;
- exact action taxonomy behind CQC counter `0x180`;
- box/drum component identities;
- run-save integrity algorithm;
- native trigger sites for Steam achievements;
- whether emblem predicates run only at results or maintain cached state.

None blocks current tracker. All 40 emblem predicates and every required live
aggregate are implemented for target build.

## References

- [Jayveer/Gcx](https://github.com/Jayveer/Gcx)
- [Perfare/Il2CppDumper](https://github.com/Perfare/Il2CppDumper)
- [RPCS3 update-2.00 encryption issue](https://github.com/RPCS3/rpcs3/issues/16748)
- [RPCS3 community patches](https://github.com/illusionyy/rpcs3-game-patch/blob/main/patch.yml)
- [BLUS30109 save-editor codes](https://www.save-editor.com/tools_original/wse_ps3_save_editor_code_title.html?id=BLUS30109)

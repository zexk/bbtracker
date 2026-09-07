# Metal Gear: Ghost Babel research notes

Reverse-engineering notes for the Ghost Babel build shipped in Master
Collection Vol. 2 Bonus Content. Addresses under "ROM" are offsets in the
decrypted 4 MiB Game Boy Color ROM unless written as Game Boy addresses.
Host VAs use the preferred image base `0x140000000`; live heap addresses are
examples from one run and are not signatures.

## Target build

- Steam AppID: `3036720`
- Process: `MGS MC2 Bonus Content.exe`
- Observed PE timestamp: 2026-07-28 08:52:51 UTC
- Executable SHA-256:
  `ab2eb668545297e9a00dd048e4a7dc758c4f9f104ba2c37bc77eb0d627fd8fca`
- x86-64 PE, preferred image base `0x140000000`, image size `0x1552000`
- `.text`: RVA `0x1000`, size `0x1D292C`
- PDB path retained in the binary:
  `C:\jenkins\workspace\GBtokage\steam\m2engage\bin\WIN32.D3D11.X64.STEAM\Release\m2engage.pdb`
- `windata/alldata.bin` and `windata/alldata.psb.m` begin with `mdf\0` and
  contain the packed application data. The ROM is not present as plaintext
  in either file.

The host is M2's `m2engage` emulator. Its strings expose Squirrel bindings
including `getBootTimer`, `getRamValue`, `GetEXRamU8`,
`CopySram2StructArray`, and save-state/replay functions.

## ROM

The standard 48-byte Nintendo logo locates decrypted ROM copies in live
memory. One run contained a complete contiguous copy at `0x0EBF0040`.

- Size: `0x400000` (4 MiB)
- Runtime dump SHA-256:
  `ac2c91a973aedcc25de894da1776e2867e4cd4b8ac53207103f79aa92246edd0`
- Header title: `METALGEARGB`
- Manufacturer: `BMSP`
- Game Boy Color only (`0xC0`)
- MBC5 + RAM + battery (`0x1B`)
- SRAM size: `0x2000` (8 KiB)
- Region: non-Japanese
- Header checksum validates (`0x0D`)
- Global checksum does not validate: stored `0x9974`, computed `0xD74B`.
  Treat this as a patched Master Collection ROM, not a bad dump.

Do not commit ROM or memory dumps.

## Host objects and SRAM

Breaking on the native callback registered as `getBootTimer` at
`0x140016FB0` captures the M2 system object in `rcx`. Confirmed fields:

| System offset | Meaning |
| ---: | --- |
| `+0x158` | pointer read by `getBootTimer`; first `u32` is boot timer |
| `+0x1B0` | emulator manager pointer |
| `+0x1F0` | SRAM descriptor-array pointer |

`CopySram2StructArray` is registered with native callback `0x140013CF0`.
Its worker at `0x140013DB0` walks four SRAM descriptors, stride `0x28`:

| Descriptor offset | Type | Meaning |
| ---: | --- | --- |
| `+0x00` | `char[8]` | tag, first entry is `STATRAM0` |
| `+0x0C` | `u32` | compressed size |
| `+0x10` | `u32` | decompressed size |
| `+0x18` | pointer | zlib-compressed data |

In the observed run, `STATRAM0` compressed 8 KiB of zero SRAM to 31 bytes.
This confirms the descriptor path but gives no save-layout evidence yet.

`GetEXRamSize`/`GetEXRamU8` do not expose Game Boy WRAM. Their callbacks
read an emulator-manager-local extra buffer at `manager+0xF8`, with size at
`manager+0x10F8`; observed size was `0x1000` and contents were zero.

## Rank evaluator

Bank 1 contains the end-game rank evaluator at CPU/ROM address `0x5ED8`.
Difficulty byte `C4E7` selects one 44-byte threshold record through the
pointer table at `0x5FCD`:

| Difficulty | Value | Record |
| --- | ---: | ---: |
| Easy | `0` | `0x6059` |
| Normal | `1` | `0x602D` |
| Hard | `2` | `0x6001` |
| Very Hard | `3` | `0x5FD5` |

Five run values feed the evaluator. All are career totals, accumulated
across stages by the merge routine at `0x5D8B`:

| Metric | Game Boy address | Width | Meaning | Evidence |
| ---: | ---: | --- | --- | --- |
| 1 | `DF4A` | `u16` | times found (alerts) | per-stage `C4EE` bumped by the alert path at bank `09:0x4178`, deduped through flag `D01F`; weighted 20 in the stage score |
| 2 | `DF4C` | `u16` | kills | per-stage `C4F0` bumped at bank `09:0x415D`; weighted 5 in the stage score |
| 3 | `DF4E` | `u16` | rations used | per-stage `C4F2` bumped at bank `08:0x6BE0` and `0D:0x4953`, both immediately after restoring 0x18 health into `C5E3`; weighted 2 in the stage score |
| 4 | `DE08` | `u8` compared as `u16` | saves | incremented once per save at bank `50:0x592C`, mirrored to `C44A` |
| 5 | `DF55` | 3-byte base-60 time, low byte first | cumulative time | seconds, minutes, hours |

Metric 5 is cumulative time: code at `0x5DDB` normalizes its two low bytes
at 60 and caps the high byte at 99. Per-stage values at `C4F9..C4FB` are
merged into `DF55..DF57` by `0x5D8B`.

The merge routine runs with `SVBK = 6`, so the whole career block lives in
WRAM bank 6. It adds the three `u16` counters with a 999 cap, adds a
`u32` counter (`C4F4` into `DF50`) with an `FFFFFFFF` cap, skips the
per-stage frame byte, then folds the time. The end screen at `0x5E7C`
prints them in the order time, metric 1, metric 2, metric 3, then calls the
evaluator; saves never appear on that screen.

Each metric becomes category 0-4: category increments for every threshold
less than or equal to the run value. Thresholds are:

| Difficulty | Metric 1 | Metric 2 | Metric 3 | Metric 4 | Time |
| --- | --- | --- | --- | --- | --- |
| Easy | 6, 26, 61, 121 | 25, 60, 120, 200 | 2, 2, 2, 34 | 80, 80, 80, 80 | 1:30:01, 2:00:01, 10:00:00, 10:00:00 |
| Normal | same | same | 2, 2, 2, 33 | same | same |
| Hard | same | same | 2, 2, 2, 21 | same | same |
| Very Hard | same | same | 2, 2, 2, 12 | same | same |

Evaluator returns rank index 0-11. Name table at `0x60DE` contains 12 names
per difficulty:

| Index | Easy | Normal | Hard | Very Hard |
| ---: | --- | --- | --- | --- |
| 0 | HOUND | DOBERMAN | FOX | BIG BOSS |
| 1 | CHICKEN | MOUSE | RABBIT | OSTRICH |
| 2 | SPARROW | PIGEON | SWALLOW | FALCON |
| 3 | CICADA | MYNA | PARROT | PEACOCK |
| 4 | PIRANHA | SHARK | JAWS | ORCA |
| 5 | PIG | ELEPHANT | MAMMOTH | WHALE |
| 6 | SNAIL | TURTLE | KOALA | SLOTH |
| 7 | SPIDER | TARANTURA | CENTIPIDE | SCORPION |
| 8 | MONGOOSE | HYENA | JACKAL | COYOTE |
| 9 | PUMA | LEOPARD | PANTHER | JAGUAR |
| 10 | BEAVER | BAT | MOLE | CLOW |
| 11 | CHAMELEON | IGUANA | ALLIGATOR | CROCODILE |

`CLOW` is the exact ROM spelling.

### Selection logic

`0x5F4C..0x5FB1`, exact. Categories live in HRAM: `FF8B` found, `FF8C`
kills, `FF8D` rations, `FF8E` saves, `FF8F` time.

```text
if found == 0 && kills == 0 && rations == 0 && time < 2:  return 0
if kills == 4 && rations == 4 && saves == 4 && time == 4:  return 1
if time == 0:                                              return 2
if found   == 4:                                           return 3
if kills   == 4:                                           return 4
if rations == 4:                                           return 5
if time    == 4:                                           return 6
row = found ? found - 1 : 0        // 0..2
col = kills ? kills - 1 : 0        // 0..2
return table_5FC4[3 * row + col]
```

Note the two asymmetries: the top-rank gate ignores saves, and the `== 4`
ladder never tests saves either, so `DE08` only ever reaches the result
through the rank-1 clause.

`table_5FC4` is 9 bytes, `07 08 09 0A 08 08 0A 08 0B`, rows by found
category, columns by kill category:

| found \ kills | 0-1 | 2 | 3 |
| --- | ---: | ---: | ---: |
| 0-1 | 7 | 8 | 9 |
| 2 | 10 | 8 | 8 |
| 3 | 10 | 8 | 11 |

The community table circulated on the wiki has this grid transposed for the
Jaguar/Puma row; the ROM pairs index 9 with low found and high kills, not
the other way round.

A debug editor at `0x629D` exposes all 11 raw bytes (`DF4A..DF4F`, `DE08`,
`DF55..DF57`, `C4E7`) and can alter the selected byte with directional
inputs. Useful for testing every rank branch.

## Per-stage rank

Bank `0x70` scores the stage just finished. `0x5CF9` computes the score,
`0x5D72` bands it, and the caller at `0x5B94` stores the band in `C503`.

```text
if C4FB != 0:                       // one hour or more
    score = 0x7FFF                  // forced Terrible
else:
    seconds = C4FA * 60 + C4F9
    delta   = seconds - tgt_time[stage]        // signed
    score   = delta * k[stage] + found * 20 + kills * 5 + rations * 2
```

The multiply at `0x5DA6` sign-corrects by negating around the unsigned
multiply, so a stage finished under target subtracts.

| Band | Score | Value in `C503` |
| --- | --- | ---: |
| Excellent | < 0 | 0 |
| Great | 0-99 | 1 |
| Good | 100-349 | 2 |
| Poor | 350-499 | 3 |
| Terrible | >= 500 | 4 |

`tgt_time` is 13 `u16` entries at `0x5DCE` in bank `0x70`, seconds:

| Stage (1-based) | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 | 13 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| tgtTime | 55 | 150 | 420 | 420 | 720 | 240 | 80 | 720 | 360 | 120 | 840 | 240 | 60 |

`k` is a switch on the stage id at `0x5DA6`, not a table:

| Stage id `C46C` | Stage | k |
| ---: | ---: | ---: |
| `0`, `9`, `11` | 1, 10, 12 | 2 |
| `6` | 7 | 5 |
| `12` | 13 | 20 |
| all others | | 1 |

Stage 13 is `k = 20`, not the 12 the community table lists; `0x5DC6`
computes `x*4 + x*16`.

After banding, `0x5B9A` gates the best-record update on `C4E7 == 1`, then
walks a per-stage record of `{seconds, minutes, hours, rank}` and sets
`C504 = 0x80` when the stage rank beats the stored one.

## RAM map

Everything in `C0xx..C6xx` is unbanked. Everything in `DExx..DFxx` needs
`SVBK = 6`.

| Address | Width | Meaning |
| ---: | --- | --- |
| `C0AA` | 3 | screen/state jump vector, the autosplitter's anchor |
| `C0F3` | `u8` | mode; low nibble 0 story, 2 special missions, high nibble the special-mission group |
| `C46C` | `u8` | stage id, 0-based |
| `C44A` | `u8` | mirror of the save count |
| `C4E7` | `u8` | difficulty, 0 Easy .. 3 Very Hard |
| `C4EE` | `u16` | stage times found, capped 999 |
| `C4F0` | `u16` | stage kills, capped 999 |
| `C4F2` | `u16` | stage rations, capped 9999 |
| `C4F4` | `u24` | stage counter bumped every 8th frame at bank `08:0x7BA7`, purpose unknown, merged into `DF50` but unused by either rank |
| `C4F8` | `u8` | stage frames, 0-59, incremented at bank `00:0x0B25` |
| `C4F9` | `u8` | stage seconds |
| `C4FA` | `u8` | stage minutes |
| `C4FB` | `u8` | stage hours |
| `C503` | `u8` | last stage rank, 0 Excellent .. 4 Terrible |
| `C504` | `u8` | `0x80` when the last stage set a record |
| `D01F` | `u8` | alert-counted flag, blocks double-counting one alert |
| `DE08` | `u8` | career saves |
| `DF40` | 4 | stage timer snapshot copied from `C4F8` at bank `01:0x7089` |
| `DF4A` | `u16` | career times found |
| `DF4C` | `u16` | career kills |
| `DF4E` | `u16` | career rations |
| `DF50` | `u32` | career total of `C4F4` |
| `DF55` | 3 | career time, seconds/minutes/hours |

`0x4A3C` resets a run: it zeroes `DF4A..DF57` and `C4EE..C4FB`, both 14
bytes, plus `C0F3` and `C46C`. That pair of clears is the cleanest
start-of-run signature the probe can watch for.

## Tooling

`scripts/gbdis.py` is a minimal SM83 disassembler with two extra modes:
`scan` finds every `ld [nn],a` / `ld a,[nn]` / `ld hl,nn` naming a given
address across all 256 banks, and `dis` disassembles a file range at a
given CPU base. Point it at a decrypted ROM dump; the dump is not in the
repo.

```sh
scripts/gbdis.py rom.gbc dis 5ED8 5F4C 5ED8
scripts/gbdis.py rom.gbc scan C4EE C4F0 C4F2
```

## Next targets

1. Resolve Game Boy WRAM backing storage from a stable host signature. This
   is the only remaining blocker for a probe; every field above is a WRAM
   offset with no way to reach it yet.
2. Confirm the values above against a live run, especially the difficulty
   record layout and the stage-clear moment when `C503` is written.
3. Identify `C4F4` and decide whether special missions reuse `C46C` or the
   `C0F3` high nibble for their target times.
4. Find the continue counter, if one exists; neither rank reads it.

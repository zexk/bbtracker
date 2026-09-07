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

Five run values feed the evaluator:

| Metric | Game Boy address | Width |
| ---: | ---: | ---: |
| 1 | `DF4A` | `u16` |
| 2 | `DF4C` | `u16` |
| 3 | `DF4E` | `u16` |
| 4 | `DE08` | `u8` compared as `u16` |
| 5 | `DF55` | 3-byte base-60 time, low byte first |

Semantics of metrics 1-4 remain unconfirmed. Metric 5 is cumulative time:
code at `0x5DDB` normalizes its two low bytes at 60 and caps the high byte at
99. Per-stage values at `C4F9..C4FB` are merged into `DF55..DF57` by
`0x5D8B`.

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

Rank selection logic at `0x5F4C` has confirmed special cases but still needs
clean pseudocode and metric names. A debug editor at `0x629D` exposes all
11 raw bytes (`DF4A..DF4F`, `DE08`, `DF55..DF57`, `C4E7`) and can alter the
selected byte with directional inputs. Useful for testing every rank branch.

## Next targets

1. Resolve Game Boy WRAM backing storage from a stable host signature.
2. Name metrics 1-4 by tracing writes to per-stage counters `C4EE..C4F3` and
   global counter `DE08`.
3. Translate `0x5F4C..0x5FB1` into exact rank-selection pseudocode.
4. Map mission/stage id, live timer, alerts, kills, continues, saves, and
   completion state for overlay use.

# MGS Peace Walker Master Collection PC reverse-engineering reference

Preliminary static reference for Peace Walker support in bbtracker. Values apply
to Steam build `24525201`; runtime state and gameplay counters remain unverified.

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

On-disk `.text` is encrypted or transformed: disassembly from RVA `0x1000`
immediately produces privileged instructions, invalid opcodes, and impossible
branches. Entry-point code in `.bind` is coherent x86-64 and enters a protection
bootstrap at RVA `0x19643d0`. Normal static analysis therefore needs a runtime
`.text` capture after bootstrap, as with MGS4.

CONFIRMED (2026-09-03): the loader decrypts `.text` in place at process start.
A live capture via `probe-mgspw-memory.py --dump-text` (reads the module's PE
`SizeOfCode`/`BaseOfCode` region from `/proc/pid/mem`) yields valid x86-64 from
the very first bytes (`48 8D 0D ... E9 ...` = `lea rcx,[rip+...]; jmp ...`),
whereas the on-disk bytes at the same RVA are garbage. So the decryption is a
whole-region transform applied by the `.bind` bootstrap before OEP handoff;
`.text` is plaintext for the entire post-bootstrap lifetime and is directly
readable from live memory. No anti-dump re-encryption observed during play.

Imports and readable `.rdata` remain usable. Imported graphics/audio APIs are
Direct3D 11, DXGI, XAudio 2.9, WinMM, and Media Foundation. Steam integration
uses `steam_api64.dll`; no Direct3D 12 import exists in main executable.

Protection does not erase structural metadata. `.pdata` contains 40,711 x64
runtime-function records spanning `.text`; these recover function boundaries
and unwind metadata after a runtime dump. PE debug directory is type `13`
(`IMAGE_DEBUG_TYPE_POGO`), not CodeView, so shipped image contains no PDB path.

## Static anchors

For `.rdata`, `RVA = file offset + 0x1400`.

| File offset | RVA | String / role |
| ---: | ---: | --- |
| `0x98e1b0` | `0x98f5b0` | `MGSPW_WIN32` |
| `0x98e848` | `0x98fc48` | `%02d:%02d:%02d` |
| `0x98e858` | `0x98fc58` | `[total_play_time]` |
| `0x98e878` | `0x98fc78` | `nht_savemenu : No Save Data` |
| `0x98f358` | `0x990758` | `launcher MGSPW` |
| `0x9bc4f0` | `0x9bd8f0` | `../mgspw_savedata_win` |
| `0xd1f5a8` | `0xd209a8` | `noAlert` |
| `0xd1f5b0` | `0xd209b0` | `noKill` |
| `0xd20490` | `0xd21890` | `mission_select` |
| `0xd227f0` | `0xd23bf0` | `chart%02d_rank_01` |
| `0xd3ecd8` | `0xd400d8` | `rank_up_%02d` |
| `0xd3ece8` | `0xd400e8` | `rank_down_%02d` |
| `0xd3ecf8` | `0xd400f8` | `[SW]list_nokill_%02d` |
| `0xd3ed58` | `0xd40158` | `RANK.%d` |

MSVC RTTI remains readable: 146 type descriptors include `WinStorage`,
`BaseStorage<WinStorage>`, `KeyConfigSsvIO`, engine resource/render classes, and
Steam callback types. `WinStorage` and save-menu strings provide good class and
call-site anchors once `.text` is captured. Retained lambda type hashes may also
help transfer names between builds whose RTTI layout stays stable.

Strings show retained PSP paths under `ms0:/PSP/SAVEDATA/ULUS10509DAT` plus PC
save root `../mgspw_savedata_win`. This suggests port code preserves much of PSP
save/game terminology, but does not prove identical structures or offsets.

## Installed data

Main directory contains executable, `steam_api64.dll`,
`sdkencryptedappticket64.dll`, font XPRs, and TXP text/loading resources. Launcher
is separate Unity/IL2CPP application under `launcher/`; its `GameAssembly.dll`
is not gameplay executable.

Observed PC per-user save directory contains `usersv` at exactly `0x1000` bytes
and `steam_autocloud.vdf`. Save content has not yet been decoded. User-specific
Steam ID is intentionally omitted. `usersv` begins with no readable magic and
its first 256 bytes appear encrypted or obfuscated; unlike MGS4 launcher
`usersv`, it is not immediately identifiable as an `MGSS` record. Static strings
expose `KeyConfigSsvIO`, making settings/key-configuration storage one plausible
role, not a confirmed interpretation.

## Remaining static frontier

Done: `.pdata` parsed into a function map (40,715 records) and overlaid on a
runtime `.text` dump (see "Live disassembly toolchain"); `.rdata` RTTI and
rank/codename string anchors enumerated and xref'd. Still open statically:

- inspect resource string tables and TXP/OLANG indices for mission, rank, and
  result vocabulary;
- analyze `usersv` through controlled save diffs and PSP-format comparison;
- catalog imports, Steam interfaces, achievement identifiers, and writable
  globals in `.data`.

## Live disassembly toolchain (operational)

`nix develop .#re` provides `ghidra-bin` 12.1.2 and `capstone` 5.0.9.
`scripts/pwdis.py` overlays the plaintext `.pdata` RUNTIME_FUNCTION map
(40,715 functions) onto a runtime-decrypted `.text` dump and disassembles
with capstone (exact-immediate operand match + RIP-relative xref resolution).

- Self-test (no game needed): `python3 scripts/pwdis.py --self-test`.
- Record what moves in the save block across a play session (this is how the
  live per-mission tallies were found):
  `python3 scripts/pwwatch.py --seconds 1800 --out /tmp/pw_watch.json`
  (SIGTERM writes the report early; it rebaselines if the block moves).
- Capture decrypted code (game running):
  `python3 scripts/probe-mgspw-memory.py --dump-text /tmp/pw_text.bin`
- Then:
  `python3 scripts/pwdis.py --text /tmp/pw_text.bin --find-imm 0x2008E`
  `python3 scripts/pwdis.py --text /tmp/pw_text.bin --xref-string noKill`
  `python3 scripts/pwdis.py --text /tmp/pw_text.bin --disasm 0x1401e744f`

Findings from the first live dump (pid-snapshot, image base `0x140000000`):

- `.text` is plaintext in memory for the whole post-bootstrap lifetime
  (see "Executable protection"); a single `--dump-text` suffices.
- `noAlert`/`noKill` strings resolve to one flag-init function
  `0x1401e744f` (clears flag bytes at `rbx+0x43F8`/`+0x43F9`, then loads
  save values into a struct at `+0x43FC`..`+0x4408` via string-keyed
  lookup helpers `0x14011f8d0`/`0x1400a52e0`/`0x1400a4a20`).
- `RANK.%d` -> `0x14051d600`, `0x1405212c0`; `rank_up_%02d` ->
  `0x14051e006`, `0x14052211a`; `chart%02d_rank_01` -> `0x14029758a`;
  `[total_play_time]` -> `0x140081220`.
- Stat id `0x2008E` has ZERO exact-immediate uses in code: it is a pure
  data-table id, not a compiled-in constant. Its meaning must come from
  the save-table data layout, not from an xref.

## Next analysis

- trace the rank/codename functions (`0x14051d600`, `0x1405212c0`,
  `0x14029758a`) to extract the FOXHOUND requirement inputs;
- follow the `noAlert`/`noKill` struct (`rbx+0x43F8` region) to find where
  mission flags feed codename evaluation;
- import the runtime dump into Ghidra at image base `0x140000000` for
  decompilation once the rank functions are mapped by capstone;
- diff controlled `usersv` samples before assuming any field layout.

## Probe (FOXHOUND bootstrap)

Minimal read-only probe in `src/games/mgspw/` plus
`scripts/probe-mgspw-memory.py`. Resolves three community anchors via
update-resilient AOB + RIP displacement (no hard-coded addresses):

- `PW_SAVEROOT`: `48 8B 05 ?? ?? ?? ?? 48 05 3C BD 00 00 C3` (disp+3)
- `PW_CHARARRAY`: `53 48 83 EC 20 48 8B 05 ?? ?? ?? ?? 48 63 D1 48 8B 0C D0` (disp+8)
- `PW_MISSIONTIME`: `48 89 05 ?? ?? ?? ?? 41 0F BA E1 19` (disp+3)

`--dump-text OUT` additionally captures the full decrypted `.text` (module
`BaseOfCode`..`+SizeOfCode`) for the disassembly toolchain in
"Live disassembly toolchain".

Live display: full `qword` mission timer (dec + hex + `M:SS.mmm` as ms +
`raw/60`, `raw/30`, `raw/1000`), secondary dword at `+0x14`, stage string
at save `+0x54`, total/stage play at `+0x84/+0x88`, player HP/weapon, first
16 weapon-record usage counters (`+0xBD3C`, stride `0x1C`, use `+0x14`).

Headshots-by-category and sleep/stun/incap lifetime mapping remain
UNVERIFIED; overlay marks them as such until the save-category scan
(`root+0x524C`) lands.

Live timer layout (25min `--trace`, 2540 samples over result/lobby/
mission-menu/Mother Base phases):

- `PW_MISSIONTIME+0x00` qword: high-res TOTAL play clock at 300Hz
  (`raw/300` tracks the total-play dword within ~1s in every phase:
  lobby 223/0.75, results 298/0.99). Despite the name it is NOT the
  mission timer. Per-mission time = the stage segment (total-since-
  stage-change; the overlay's big clock) or the area timer.
  Best model: frame-bound counter (~300fps cap).
- `+0x08` qword ticks ~28-60/s depending on phase (role unknown).
- `+0x10` dword: NOT a plain mirror of `+0x14` (diverges; even runs
  backwards within some phases — needs in-mission capture).
- `+0x14` dword always equals save `+0x88` (true mirror).
- save `+0x84` dword ticks ~0.6-1.0/s (pauses in some phases): total
  play seconds.
- save `+0x88` dword tracks `+0x14`.

Same-second best-time overrides stay resolvable (raw integer compare is
exact whatever the unit); the overlay shows `raw/300` as `M:SS.mmm`.

## Weapon records and mission-state block (live)

`scripts/probe-mgspw-memory.py --rate SECONDS` snapshots twice and reports
per-field deltas/rates. Note: under Proton the decrypted code lives in
anonymous mappings, so the script derives its scan range from the module PE
headers (`BaseOfCode`/`SizeOfCode`), not from maps pathnames (the exe path
also contains spaces, which breaks naive split-based matching).

Weapon array at save `+0xBD3C`, stride `0x1C` (28 bytes). Observed layout:

| Offset | Type | Meaning |
| ---: | --- | --- |
| `+0x00` | `u32` | weapon id, 1-based (row `i` holds id `i+1`) |
| `+0x04` | `u32` | developed flag (`3` = developed) |
| `+0x08` | `u32` | `100` when developed |
| `+0x0C` | `u32` | stock quantity |
| `+0x14` | `u16` | weapon XP within the current level: RESETS to 0
  on level-up (id3 NV.1->NV.2: `5792`+mission -> `0`, so NV.1 spans
  0-6000). Applies exactly displayed, settled at mission end. |
| `+0x16` | `u8` | usage level (NV.1..3) |
| `+0x16` | `u8` | usage level (NV.1..3) |

Equipped weapon id at character `+0x14B8` equals the record id, so the
overlay labels weapon rows as `idNN`. Confirmed: id3 = tranq/silenced
pistol (equipped id read `3` while tranq equipped).

## Mission best times and ranks (live)

Best times are stored in 300Hz ticks, displayed floored to whole
seconds. Correction: the "mission 10" session (A, 0:00:55, heroism +7,
pistol XP +345) was a Side Ops 10 replay at 55.18s (`16555`) that did
NOT beat the standing best `15755` = 52.52s (display 0:00:52) — which is
why the lone slot at save `+0x2A84` stayed frozen: it IS the Side Ops 10
per-mission best slot. Same-second override mechanism: display floors to
whole seconds while storage keeps 3.33ms resolution (a 55.05s replay
stores `16515 < 16555` and overrides despite the identical display).
Twin adjacent slots at save `+0x586C` stage the LAST mission
(`16555` then, `23385` = 77.95s after Side Ops 5 at displayed 0:01:17),
not a per-mission table. CONFIRMED update-on-beat: a 32.58s Side Ops 10
S-replay (display 0:00:32, floor once more) moved its slot `+0x2A84`
from `15755` to `9775` and the staging twins to `(9775, 9775)`; the
earlier 55.18s A-replay left `15755` untouched. S-count stayed `3`
because the mission already held S (unique-mission semantics).
Heroism `+22` repeated a third run in a row (167->189); id3 XP applied
exactly displayed (+348).
A process restart reloaded identical probe-visible state (heroism 167,
GMP 31341, clears 11, S-count 3, Fulton 208, id3 XP 5444, last-best
23385), confirming autosave restores everything the probe reads.
WITHDRAWN (was: rank letters as ASCII near `+0x140CF`): PW mission ranks
are only S/A/B/C, so `ECEDFGCCCCA` cannot be ranks, and reported mission 5
= S contradicts position 5 = F. The 11-char null-terminated string near
`+0x140CF` is something else, still unidentified; it did not change across
replays. Mission-index mapping of the time slots is still open: validate
by watching which slots move on the next clear of a new (not replayed)
mission.

## Per-mission results table (CONFIRMED live)

Three parallel per-mission arrays in the save block, indexed by **mission
id** (same id space as the mission definition table, see below):

| Array | Offset | Type | Meaning |
| --- | --- | --- | --- |
| rank | `save+0x32B4 + 2*id` | `u16` | `0` = S, higher = worse (`1` = A seen), `0xFFFF` = never cleared |
| rank2 | `save+0x46F4 + 2*id` | `u16` | parallel array, all `0xFFFF` on a solo-only profile (co-op/Extra Ops?) |
| best time | `save+0x29B4 + 4*id` | `u32` | 300 Hz ticks, `0xFFFFFFFF` = none |

Live confirmation on the current profile (`clears` 20 counts replays,
`s_count` 3 counts unique missions):

| id | rank | best time |
| ---: | ---: | --- |
| 1 | 1 | 423.87 s |
| 2 | 1 | 584.90 s |
| 3 | 0 | 258.73 s |
| 36 | 0 | 56.95 s |
| 52 | 0 | 32.58 s |
| 162 | 3 | none |

Six unique missions cleared, exactly three with rank `0` - matching the
confirmed S-rank count of 3, which is what identifies `0` as S. Mission id
52 is the Side Ops 10 slot whose best time the earlier work tracked at
`save+0x2A84` (`0x29B4 + 4*52 = 0x2A84`), so the single-slot finding was
this array all along; the twins at `save+0x586C` remain the last-mission
staging pair.

`scripts/probe-mgspw-memory.py` reports these under `foxhound.missions`.
Entries past the live list length read as zeros, so the probe only reports
ids that carry a rank or a time; the exact array length is still open (the
list object at `0x14141C200` is null outside the mission-select UI, so its
count could not be read from Mother Base).

Mission definitions live in a separate runtime table (`0x14143B738` ->
`+0xB8` array, 380 records, stride `0x24`): `+0x00 i16` id, `+0x0A i8`
difficulty rank (what `RANK.%d` formats via `0x140255AF0`), plus a family
of field getters at `0x1402558E0..0x140255AF0` covering `+0x04`, `+0x08`,
`+0x0C..0x13`, `+0x14`, `+0x18`, `+0x1C`, `+0x1D`. Five of those are
booleans (`+0x0D..+0x11`) - candidate per-mission objective flags.

`0x1401E72AC` walks the rank array to drive the mission-select
`noAlert`/`noKill` display: a mission with `rank & 0xFFFB` set clears the
aggregate `noAlert` flag, and anything but rank `1` clears `noKill`. The
four UI variables the same screen loads by name are `noAlert`, `noKill`,
`numOuter`, `ClearOuter` (`.rdata 0x140D209B8..`), read through the
name-hash lookups `0x14011F8D0`/`0x1400A52E0`/`0x1400A4A20`.

## Controlled run: Side Ops 10, S rank (quantified)

Reported by the player: Side Ops 10, clear time `0:00:50`, rank S, kills 0,
alerts 0, enemies near death 0, heroism `+22`, Mk22 XP `+280`. Actual
actions: 3 tranq takedowns (2 of them headshots), 1 Fulton extraction, no
continues. Pre/post descriptor snapshots (`--idmap`) moved exactly these:

| id | before -> after | delta | reading |
| --- | --- | ---: | --- |
| `0x4420031` | 71 -> 73 | +2 | headshots (2 headshot takedowns) |
| `0x442002E`, `0x200F9` | 72 -> 75 | +3 | tranq takedowns |
| `0x4420077` | 309 -> 331 | +22 | heroism (matches `save+0x64F4`) |
| `0x2008E` | 34 -> 35 | +1 | Fulton candidate, now 2/2 one-Fulton runs |
| `0x44200DC` | 16 -> 17 | +1 | clean-clear counter (unidentified) |
| `0x442011E` | 14 -> 15 | +1 | clean-clear counter (unidentified) |
| `0x442011F` | 13 -> 14 | +1 | clean-clear counter (unidentified) |

Kill ids (`0x420008`, `0x2007C`, `0x200E0`) stayed put on the 0-kill run,
as expected. Heroism now has a confirmed descriptor id, so the whole
"category `0x442`" family is the profile-stat set: value at `+0x20`,
last-mission delta at `+0x18` (heroism read `22`, tranq `3`, headshots `2`).

The three `+1` counters all moved on a run that was simultaneously 0-kill,
0-alert and S-rank, so they cannot be separated yet. With 21 total clears
their values (17 / 15 / 14) fit a no-kill / no-alert / both-of-those
ordering. Next run should be deliberately dirty (kill once, or trip an
alert) to split them.

Save-side confirmation from the same run: `clears` 20 -> 21, GMP `+347`,
weapon id3 XP `+280` exactly as displayed, staging twins `save+0x586C`
both `15165` = 50.55 s (the run's time). The per-mission arrays did not
move: mission 52 already held S and a better time (`9775` = 32.58 s), so
neither the rank nor the best-time slot updated - which is exactly the
expected behaviour and further confirms the mapping.

## Controlled run 2: Side Ops 10, dirty (6 kills, 1 alert, 1 Fulton, 0 tranq)

| id | before -> after | delta | reading |
| --- | --- | ---: | --- |
| `0x420008`, `0x200E0` | 8 -> 14 | +6 | kills (confirmed again) |
| `0x2007C` | 8 -> 10 | +2 | NOT plain kills - moved only 2 on a 6-kill run |
| `0x4420031` | 73 -> 79 | +6 | headshot reading now doubtful: +2 on run 1's 2 headshots, +6 here |
| `0x420002` | 2 -> 3 | +1 | alerts (1 alert this run, static on the 0-alert run) |
| `0x2008E` | 35 -> 36 | +1 | Fulton candidate, 3/3 one-Fulton runs |
| `0x44200DC` | 17 -> 18 | +1 | increments on dirty runs too: a clear counter, not a clean-clear one |
| `0x442011E`, `0x442011F` | static | 0 | THE clean-clear pair: both moved on the clean S run, neither here |
| `0x20023` | 4962 -> 9907 | +4945 | unidentified; static across the clean run |
| `save+0x22`, global `0x1415969F4` | +1 each | +1 | achievement id-11 counter (threshold 50); static on the clean run |

Heroism did not move at all (`heroism_delta` 22 -> 0), matching the
"heroism collapses with lethality" note. `clears` 21 -> 22, GMP `+192`,
staging twins `36800` = 122.67 s, and mission 52's rank/best-time slots
again stayed put (S already held, slower run).

Run 3 (below) resolved the first two of these: `0x442011E` is the no-alert
counter, `0x442011F` the no-kill counter, and `0x4420031` is headshots.
`0x2007C` and `0x20023` remain unidentified - both moved only on the
6-kill run, so they are kill-linked.

## Stat descriptor layout and live per-mission tallies (CONFIRMED)

Record is 48 bytes; the probe already keyed on the `999999` bounds, but the
middle fields are now pinned by live reads across two quantified runs:

| Offset | Type | Meaning |
| ---: | --- | --- |
| `+0x00` | `u32` | bound, `999999` |
| `+0x10` | `u32` | stat id |
| `+0x18` | `i32` | **this mission's tally** - ticks live during play |
| `+0x20` | `i32` | career value - settles at the results tally |
| `+0x28` | `u32` | bound, `999999` |

CORRECTION to "Settle model": the career value settles at results, but the
`+0x18` tally is live. The kill record's tally was sampled every 2 s through
run 2 and stepped `0,1,0,1,2,4,5,6` (the leading reset is mission start)
while the career value made a single `8 -> 14` jump at settle. That is the
missing piece for live requirement tracking: no client-side baseline
latching is needed, the game keeps the per-mission counters itself.

Category matters: `0x042`/`0x442` records use `+0x18`; `0x002` records leave
junk there (large negative values), so filter on a sane range.

Confirmed ids so far:

Three quantified Side Ops 10 runs settle the table:

| run | kills | alerts | tranq (headshots) | Fulton | heroism |
| ---: | ---: | ---: | --- | ---: | ---: |
| 1 | 0 | 0 | 3 (2) | 1 | +22 (S rank) |
| 2 | 6 | 1 | 0 | 1 | +0 |
| 3 | 0 | 1 | 3 (2, 1 body, 1 miss) | 1 | +7 |

| id | meaning | evidence |
| --- | --- | --- |
| `0x420008`, `0x200E0` | kills | +6 on run 2, static on runs 1 and 3 |
| `0x420002` | alerts | +1 on runs 2 and 3, static on run 1 |
| `0x442002E`, `0x200F9` | tranq takedowns | +3 on runs 1 and 3 (body shots count, misses do not) |
| `0x4420031` | headshots | +2 on both 2-headshot runs; run 2's +6 means all six kills were headshots |
| `0x4420077` | heroism | +22 / +0 / +7, matches `save+0x64F4` exactly |
| `0x2008E` | **Fulton recoveries** | +1 on each of four separate 1-Fulton runs |
| `0x442011E` | **no-alert clears** | +1 only on run 1 (the one run without an alert) |
| `0x442011F` | **no-kill clears** | +1 on runs 1 and 3, static on the 6-kill run |
| `0x44200DC` | clear counter | +1 on every clear (19 vs `clears` 23, so it counts a subset - Side Ops?) |
| `0x2007C` | unknown | static on runs 1 and 3, +2 on the 6-kill run |
| `0x20023` | unknown | +4945 on the 6-kill run only; kill-linked (damage?) |

Heroism is alert-sensitive as well as kill-sensitive: +22 clean, +7 with one
alert on an otherwise identical no-kill run, +0 with kills.

`save+0x22` (the achievement id-11 counter) went `0 -> 1` on run 2 and back
to `0` on run 3, so it is mission-scoped, not a career total.

Ids are mostly data-driven: only two code sites embed one as an immediate.
`0x14037B0B5` posts ids `0x420001..0x420004` through the generic event
dispatcher `0x140079210(ctx, id, valuePtr)` from a switch on an alert-state
case value, which is consistent with `0x420002` being the alert counter and
its neighbours being the other alert-phase counters. The alert record's
`+0x18` did not line up with the reported alert counts across the two runs,
so only the career alert reading is confirmed.

`scripts/probe-mgspw-memory.py` reports non-zero tallies under
`foxhound.mission_tally`; the ASI probe fills `pw_m_kills`,
`pw_m_alerts`, `pw_m_tranq`, `pw_m_headshots` and the overlay's Current tab
shows them (falling back to the old segment deltas when a descriptor is
unresolved).

## First-clear run: mission id 4 (write path confirmed)

Player cleared a never-before-cleared main mission, "Armored Vehicle
Battle: LAV-Type G": clear time `12:06`, 0 kills, 0 near-deaths, cutscene
interaction bonus 10/10, LAW rank `+518`. The save-block recorder caught
the exact writes:

- `save+0x32BC` (`0x32B4 + 2*4`): `0xFFFF` -> `0x0001` - the rank slot for
  mission id 4, written as A;
- `save+0x29C4` (`0x29B4 + 4*4`): `0xFFFFFFFF` -> `218095` = 726.98 s,
  matching the displayed `12:06`;
- `clears` 23 -> 24, staging twins -> `218095`.

So **mission id 4 = Armored Vehicle Battle: LAV-Type G**, the rank scale is
`0` = S, `1` = A, `2` = B, `3` = C, and both per-mission arrays are written
directly by mission id. A clean 12-minute boss clear scores A, which is the
first non-S data point on record.

Descriptor deltas from the same run:

| id | delta | note |
| --- | ---: | --- |
| `0x4420077` | +13 | heroism (matches `heroism_delta`) |
| `0x442002E` | +7 | tranq takedowns |
| `0x200F9` | +6 | **diverges from `0x442002E`** - the two are not the same counter |
| `0x4420031` | +3 | headshots |
| `0x2008E` | +8 | Fulton (8 extractions in a 12-minute main op) |
| `0x442011F` | +1 | no-kill clears (0 kills) |
| `0x442011E` | 0 | no-alert clears - the boss fight alerted, as expected |
| `0x442007B` | +1 | first movement in four runs: main-op/boss clear counter? |
| `0x44200DC` | +1 | every clear |
| `0x20023` | +6000 | equals the mission score |
| `0x200F7`, `0x20106` | +1 | one-shot events (boss defeated / vehicle destroyed?) |

`0x20023` is the career score total, not damage: the same run wrote `6000`
into the four score twins at `save+0x278`, `+0x2A0`, `+0x1FBF4`, `+0x1FC1C`
(previous value `8000`), and the career id moved by exactly that. It stayed
flat on the two Side Ops replays that did not beat a standing best, so the
award rule for replays is still open.

Live tallies ticked again through this run: the tranq record's `+0x18`
stepped `0,2,3,4,5,6,7` and the headshot record's `0,1,2,3`, each landing on
the value the results screen showed.

## Rank improvement observed: mission id 2 (S)

Player re-cleared "Sandinista Comandante" with S. The arrays moved exactly
as the model predicts:

| field | before | after |
| --- | --- | --- |
| rank (`save+0x32B8`) | `1` (A) | `0` (S) |
| best time (`save+0x29BC`) | `175470` = 584.90 s | `58635` = 195.45 s |

So **mission id 2 = Sandinista Comandante**, and a rank slot is overwritten
on improvement, not only on first clear.

Run deltas: heroism `+22` (the clean-run value again), tranq `+12`,
headshots `+10`, Fulton `+9`, `clears` +1, and **both** clean-clear
counters moved (`0x442011E` no-alert and `0x442011F` no-kill), so the run
had zero kills and zero alerts.

Two S/A data points now sit against each other:

| mission | rank | time | kills | alerts |
| --- | --- | ---: | ---: | ---: |
| 2 Sandinista Comandante | S | 195.45 s | 0 | 0 |
| 2 Sandinista Comandante | A | 584.90 s | ? | ? |
| 4 LAV-Type G | A | 726.98 s | 0 | had alerts |

Consistent with the player's read that time, kills and alerts all feed the
score.

Score anomaly: the four score twins (`save+0x278`, `+0x2A0`, `+0x1FBF4`,
`+0x1FC1C`) still read `6000` after this S run - the same value the A-rank
LAV clear left - and the career score id `0x20023` did not move at all.
`0x20023` has now moved on exactly two runs (a first clear `+6000` and a
dirty Side Ops replay `+4945`) and stayed flat on three others, so
"cumulative score" is not the whole story; the twins may be a capped
per-mission points value rather than the rank input.

## Heroism candidate

Single dword `123` at save `+0x64F4` with `min=-999999 max=999999` bounds
inline and the last mission delta `+7` in the adjacent record
(`+0x64EC`): matches reported Heroism `123` after a `+7` mission.
(Hits for `123` at `+0xCA94`/`+0xEE2c` are record ids in sequential
id arrays, stride `0x1C`/`0x18`, not the scalar.)
CONFIRMED: later read `145` with last-delta `22` (`123+22=145`).
Heroism dword = save `+0x64F4`, last-mission delta = save `+0x64EC`.
Side Ops 5 (+22 again, total `167`) re-confirmed the delta field.

## Economy and marksmanship (live, Side Ops 5: S, 6000pts)

- GMP dword = save `+0xB52C` (`31341`, single hit). Neighbor `+0xB520`
  = `131079` unidentified (lifetime GMP? camaraderie?).
- Food `151%`: candidates `+0xCDA4`/`+0xF0CC` (=151); `+0xB550` reads
  150. Unresolved.
- Marksmanship points twinned twice: `+0x278`/`+0x2A0` and
  `+0x1FBF4`/`+0x1FC1C` (`4758` -> `6000`), plus a second twinned pair
  `+0x250`/`+0x264` (`3936` -> `8000`, meaning unknown — second score
  category?).
- Weapon XP applies exactly displayed in all 3 cases (345 A, 483 S,
  348 S-replay). RETRACTED (were: S-multiplier, first-S bonus): the
  `+871` was TWO missions — an unreported +22-heroism mission (+388
  pistol XP) slipped through the baseline gap, then Side Ops 5 (+483).
  Lesson: snapshot live memory immediately before/after each reported
  mission, never sessions apart.
- Save `+0x656C` CONFIRMED as global clear count (replays count: a
  Side Ops 10 replay bumped `11`->`12`). Shown unqualified. Note for
  codename math: this is total clears, not unique-mission clear %.
- Save `+0x9084` CONFIRMED as S-rank count, refined to UNIQUE missions:
  re-clearing Side Ops 10 with S left it at `3` (that mission already
  held S; the earlier `2`->`3` was Side Ops 5's first S). Shown
  unqualified.

## FOXHOUND lifetime inputs (confirmed by quantified missions)

- Headshots id `0x4420031` (career 65): +1 on exactly-1-headshot run;
  prior +5 was 3 lethal-heads + 2 tranq-heads; static on 0-headshot
  body mission. Achievement at 50 crossed earlier (award-at-results
  timing explains the late toast).
- Kills ids `0x420008`, `0x2007C`, `0x200E0` (career 9): +3 on 3-kill
  run, each with mission-scope 0->3 copies. Static on no-kill runs.
- Tranq takedowns ids `0x442002E`, `0x200F9` (career ~69): +2 on
  2-tranq run; +3 on 3-tranq missions.
- id `0x2008E` CONFIRMED as career Fulton recoveries (the earlier
  "disproven" call was wrong): +1 on each of three separate 1-Fulton
  runs, then +8 on a main op where the in-game results screen itself
  showed 8 extractions. `+0x130` (229, static through Fulton uses) is
  NOT Fulton stock — still an unidentified stale value.
- Static fulton hunt is a dead end: `.rdata` fulton strings are only
  animation/voice assets (`sna_fulton`, `0806fulton_selfFULTON`,
  `fulton_strt`), not stat keys — the numeric-id table has no
  plaintext names. Save file `STW00000092e301` (325968 B) is fully
  encrypted (entropy ~7.7 bits/byte in all blocks; no dword of
  heroism 309 / GMP 34764 / kills 9 / clears 20 / 0x2008E=34
  appears anywhere). Next fulton step needs the game running:
  read the profile's Fulton Recoveries number and `--idmap` scan for
  a matching id, or diff pre/post a quantified N-fulton mission.
- Heroism collapses with lethality: +22 on clean tranq missions (x8),
  +3 on 3-kill missions (x2). Possibly +1/kill vs +22 clean — needs a
  different-kill-count mission.
- Overlay shows HS/KILLS/TRANQ/Fulton plus provisional lethal score
  (TQ-2K); stun/incap still unmapped (tranq total stands in).

- Method notes: `+0x1C1E4` ruled out early (non-monotonic staging
  transient). An earlier "sleep ID" set (`0x442006E` etc.) retracted as
  hex misreads of the same three IDs. Table reallocates between
  missions: track by descriptor ID with per-mission pre/post
  snapshots (`--idmap`), never absolute offsets. Tracer note:
  heroism/GMP settle at the result->lobby boundary; practice sorties
  can end with zero stat movement.
- Flavor: staging holds ASCII `ALLIGATOR` at save `+0x1C098` — the
  codename of the currently-used soldier (player character).

Titles block at save `+0x18334` confirmed: earned slots read `07`.

Vehicle-boss escort counter resolved live at RVA `0x158CC48` (value `0` in
menu); sibling reset stores at `0x158CBF8`/`0x158CBFC`. The surrounding
mission-state block reads all-zero outside missions, consistent with the
reset routine zeroing it on mission start. The indexed mission-stat
accumulator (`add [rcx+rax*4],ebx`) presumably targets this block during
missions; capture it live (plus results-screen kills/alerts) to anchor
stat indices including any headshot counter.

Current evidence supports binary identification and analysis entry points only.
No gameplay offset, counter meaning, rank predicate, or injection path is yet
claimed.

## Static RE: Steam achievement path (Headshot Hero)

Achievement: **Headshot Hero** — "Take down 50 enemies with a headshot"
(Exophase id 3788500). "Take down" wording matters for counting
discipline (wounding headshots may not count). Toast fired mid-mission,
so the unlock check reads a LIVE counter, not results settlement —
unless the toast lagged; Steam unlock timestamp vs tracer timeline will
decide. Related cumulative counters exist for method reuse: 50 CQC KOs,
100 rolls in one mission (rolls = ideal controlled test: exact N, no
enemies, no menus).

Binary anchors (on-disk, `.rdata` readable, `.text` encrypted):
- own classes `MGK_SteamAchievement` / `MGK_IAchievementSystem`
  (Virtuos-style, cf. MGS4's achievement manager);
- `STEAMUSERSTATS_INTERFACE_VERSION012`;
- `MGK_SteamAchievement` vtable RVA `0xe13ea8`, 16 methods at RVAs
  `0x6b800,0x6b980,0x6b990,0x6b9e0,0x6ba10,0x6ba30,0x6bb20,0x6bb80,
  0x6be70,0x6c030,0x6c280,0x2ddf0(x2)` (+2 rdata-resident).
- Only flat `steam_api64.dll` imports (Init/RunCallbacks/ContextInit/
  FindOrCreateUserInterface…); UserStats goes through the interface
  vtable, whose layout is public SDK knowledge.

Live status (DONE): runtime `.text` dump captured (see "Live disassembly
toolchain"); rank/codename string anchors xref'd to functions.
DONE: the whole achievement path is mapped from the runtime `.text` dump.
See "Achievement system (mapped)" below. Scripts:
`scripts/scan-mgspw-strings.py`, `scripts/rtti-mgspw-ach.py`. Toolchain
replaced by `scripts/pwdis.py`.

## Achievement system (mapped)

All addresses are VAs at image base `0x140000000`, recovered from the
runtime `.text` dump plus on-disk `.rdata`/`.data`.

Save-struct base pointer: **`0x140EA4860`** (qword global holding the
profile/save block). This is the same global the probe resolves as
`PW_SAVEROOT` (its AOB site is `0x140215D10`, a getter returning
`saveroot+0xBD3C`, the weapon array). Every achievement predicate reads
its counters through this pointer, so all `save+OFF` offsets below are in
the same space as the previously confirmed ones (GMP `+0xB52C`, heroism
`+0x64F4`, weapons `+0xBD3C`).

Other singletons seen in predicates: `0x140F1B870` (player/game-state
object; `+0x11BC` current weapon id, `+0x2384` a >=100-threshold counter),
`0x1414C72F0` (mission-record object queried by `0x140259C10(ctx, id)`).

### Objects

| Item | Address | Role |
| --- | --- | --- |
| `MGK_IAchievementSystem*` singleton | `0x141596A88` | all game-side calls go through it |
| `MGK_SteamAchievement` vtable | `.rdata 0xE13EA8` | slots below are relative to it |
| achievement metadata table | `.data 0x14105C230` | 50 rows, stride `0xC` |
| predicate function table | `.data 0x141596A90` | 50 slots, `bool(*)()` indexed by id |
| predicate registrar | `0x1400388D0` | writes 49 predicates at init |
| poll loop | `0x140038BF4` | pops queued ids, runs predicate, unlocks |
| queue push `Notify(id)` | `0x140038CC0` | what gameplay code calls |

`scripts/pwach.py` reproduces both tables (`--self-test` needs only the
exe; `--text DUMP` adds the id -> predicate column).

Metadata row (12 bytes) at `0x14105C230 + 12*id`:

| Offset | Type | Meaning |
| ---: | --- | --- |
| `+0x0` | `u32` | Steam number (`= id + 2`) |
| `+0x4` | `u32` | internal id (`0..49`) |
| `+0x8` | `u8` | enabled |
| `+0x9` | `u8` | queued/pending-store flag |
| `+0xA` | `u8` | unlocked (mirrored from Steam) |
| `+0xB` | `u8` | pad |

Steam API key format is `ACH_QXS_%03d` (`.rdata 0xE13DF8`) formatted with
the Steam number, so **internal id N = `ACH_QXS_%03d` of N+2**: id 0 =
`ACH_QXS_002` ... id 49 = `ACH_QXS_051`.

Class methods (vtable offsets are from `0xE13EA8`; the object's own vptr
is 8 higher, which is why call sites use `+0x20`/`+0x28`):

| Address | Role |
| --- | --- |
| `0x14006B790` | acquires `ISteamUserStats` via `STEAMUSERSTATS_INTERFACE_VERSION012` |
| `0x14006BA30` | `Unlock(id)`: validate, skip if unlocked, dedupe, push to pending vector `obj+0x40..0x48` |
| `0x14006BB20` | `IsLocked(id)` (gate used by predicates/poll loop) |
| `0x14006BB80` | count of enabled+unlocked rows |
| `0x14006BC46` | flush: `SetAchievement(name)` per pending id (`ISteamUserStats` vtable `+0x38`) |
| `0x14006BD0A` | refresh: `GetAchievement(name,&row[+0xA])` for all 50 (`+0x30`) |

### Predicate map

Poll loop reads `[0x141596A90 + 8*id]`. `0x14002A4E0` is the shared
"never true here" stub - those ids are story/progress achievements
unlocked by direct `Notify(id)` calls, not by a polled predicate.

| id | Steam key | predicate | condition |
| ---: | --- | --- | --- |
| 0-5 | `ACH_QXS_002..007` | `0x140039010..060` | `0x140038E70(n)`, n=0..5: item-level check over a small id set (`save+0x32B4`/`+0x46F4`) |
| 6-9,13-21,39-41 | - | `0x14002A4E0` | stub (direct `Notify`) |
| 10 | `ACH_QXS_012` | `0x140039B80` | `save+0xB4EC >= 50` |
| 11 | `ACH_QXS_013` | `0x1400392A0` | global `0x1415969F4 >= 50` |
| 12 | `ACH_QXS_014` | `0x1400392C0` | `player+0x2384 >= 100` (`0x1400EE3A0`) |
| 22 | `ACH_QXS_024` | `0x140039960` | 5 item ids all at dev level <= 3 |
| 23 | `ACH_QXS_025` | `0x140039A40` | same family, different id set |
| 24 | `ACH_QXS_026` | `0x1400398B0` | `0x1401C39F0() != 0` |
| 25 | `ACH_QXS_027` | `0x1400398E0` | any of five `0x1401B8Cxx()` flags |
| 26 | `ACH_QXS_028` | `0x1400397A0` | bit 26 of `0x1400FA8E0()` |
| 27 | `ACH_QXS_029` | `0x1400397C0` | 3 consecutive mission ids all cleared (`0x18C..` or `0x182..`) |
| 28 | `ACH_QXS_030` | `0x140039670` | all 10 of mission ids `0x191..0x19A` (or `0x188..0x191`) cleared |
| 29 | `ACH_QXS_031` | `0x140039920` | `save+0x33EE (u16) == 0` |
| 30 | `ACH_QXS_032` | `0x140039940` | `save+0x33F0 (u16) == 0` |
| 31 | `ACH_QXS_033` | `0x140039B50` | not yet read |
| 32 | `ACH_QXS_034` | `0x140039370` | staff-count sum for group `0x30` >= 100 |
| 33 | `ACH_QXS_035` | `0x1400393D0` | groups `0x2D`+`0x2E` sum >= 100 |
| 34 | `ACH_QXS_036` | `0x1400393A0` | groups `8`/`7` sum >= 100 |
| 35 | `ACH_QXS_037` | `0x1400392F0` | groups `0x8E..0x90` sum >= 300 |
| 36 | `ACH_QXS_038` | `0x140039BD0` | not yet read |
| 37 | `ACH_QXS_039` | `0x1400395F0` | gated on id `0x25` still locked |
| 38 | `ACH_QXS_040` | `0x1400394B0` | gated on id `0x26` still locked; scans a live list for entity kinds `0x55/0x56/0x57` |
| 42 | `ACH_QXS_044` | `0x140039840` | ids `0xE1..0xE8` all developed (level <= 3) |
| 43-48 | `ACH_QXS_045..050` | `0x140039070`, `0x1400390A0`, `0x140039430`, `0x1400390C0`, `0x140039140`, `0x1400391E0` | "develop everything" family over the 132-entry id list at `.rdata 0x140D8F5D0` |

Staff-count getters are `0x1400E3B60(group,0)` and `0x1400E3A90(group,0)`,
both indexing the roster array at `0x14121DFE8` (stride `0x28`).

### New save-struct fields

| Offset | Type | Evidence |
| ---: | --- | --- |
| `+0x22` | `u16` | incremented at `0x140170BC3` together with the id-11 counter |
| `+0x32B4` | `u16[]` | development level per item id (`0xFFFF` = absent); paired array |
| `+0x46F4` | `u16[]` | second level array, `0x1440` after the first; predicates take the max/min pair carefully |
| `+0x33EE`, `+0x33F0` | `u16` | item ids `0x9D`/`0x9E` in the first array (id 29/30 want them `== 0`) |
| `+0xB4EC` | `u32` | career counter, threshold 50 for id 10 |

`save+0xB4EC` is incremented at `0x1405FCB73` inside the takedown handler
`0x1405FCB00`, which then calls `Notify(10)` directly. The handler checks
the hit's body-part word (`obj+0x24`) against the value derived from the
player object (`player+0x11BC`, valid when `< 9`) and a flag bit in
`obj+0x18` before counting - i.e. this is a *specific* takedown counter,
not the general stat-table headshot id `0x4420031`.

DISPROVEN (live read, this profile): `save+0xB4EC` is **not** the
Headshot Hero counter - it reads `5` while career headshots are 71 and
the achievement (threshold 50) is already unlocked. It is some narrower
takedown counter still at 5/50, gated by the body-part word and the
weapon check in `0x1405FCB00`. The "Settle model" note therefore stands
as written.

Live counter values on this profile: `save+0xB4EC` = 5, `save+0x22` = 0,
global `0x1415969F4` = 2, `player+0x2384` = 0 (the last two are 0 outside
a mission, so they are mission- or session-scoped). Unlocked achievement
ids read from the metadata table: 0, 6, 31 (`ACH_QXS_002`, `008`, `033`).

### Follow-ups this opens

- confirm `save+0xB4EC` semantics live (above), and diff `save+0x22`
  against a counted repetition of whatever event `0x14017084E` handles;
- the "develop everything" predicates give the full item-id list
  (`0x140D8F5D0`, 132 entries): dumping it plus the two level arrays
  yields a complete development-progress view for the overlay;
- mission-clear queries via `0x140259C10(0x1414C72F0, missionId)` give a
  clean per-mission clear test, which is what the still-open mission-index
  mapping of the best-time slots needs;
- the Fulton counter is still unlocated: no predicate references it, so
  the id-map diff during a live N-Fulton mission remains the plan.

## Settle model (corrected: tally at results, not live)

Headshot Hero toasted during the results tally, not mid-mission: the
pre-toast snapshot (stage still w01s03a, results open) shows action
careers already settled (tranq id `0x442002E` at exactly 50) while
heroism still pending (189, X=0). So: action careers tally DURING the
results screen, heroism/XP/GMP settle at lobby exit, and nothing ticks
live mid-mission except the 300Hz clocks. The achievement hooks the
TRANQ counter (50.00 at toast minute; headshots sat at 48) — dev quirk
or loose "take down" logic. X semantics split: current-tally-so-far
for action stats, results-lump for heroism (0 until lobby).
Overlay derives a SORTIE view client-side by latching career baselines
at stage-string changes; deltas land at results tally (actions) or
lobby exit (heroism/XP/GMP). True live mission stats would need the
mission-state indexed-accumulator block mapped — open future work.

## Test protocol: baselines vs autosaves

All probe tooling is read-only (ASI only reads; `/proc` scripts never
write; save handling is copy-out backups). Correction to an earlier
worry: on-disk GMP matches live GMP exactly, i.e. Peace Walker autosaves
after missions and disk tracks live. Live `/proc` snapshots plus
reported results-screen numbers are therefore a sound baseline method;
no manual-save discipline needed. Reference backup: `/tmp/pw-point0/`.
Live baselines (`/tmp/pw-save-before.bin`) are labeled with the heroism
they hold. STW content itself is obfuscated (no live value appears as a
plain dword), so disk-side auditing is not available; live diffing is
the method.

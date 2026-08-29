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

Useful work remains without gameplay runtime access:

- parse all `.pdata` records into a Ghidra function map before overlaying dumped
  code;
- reconstruct RTTI class hierarchy, vtables, and object-locator addresses from
  `.rdata`;
- inspect resource string tables and TXP/OLANG indices for mission, rank, and
  result vocabulary;
- reverse `.bind` enough to identify decrypt completion and original-entry-point
  handoff, enabling a deterministic dump hook;
- analyze `usersv` through controlled save diffs and PSP-format comparison;
- catalog imports, Steam interfaces, achievement identifiers, and writable
  globals in `.data`.

Direct xrefs and gameplay logic remain blocked only by encrypted on-disk
`.text`; metadata and data-format analysis do not.

## Next analysis

- dump post-bootstrap `.text` and record its hash;
- import runtime dump into Ghidra at image base `0x140000000`;
- find references to `[total_play_time]`, `noAlert`, `noKill`, and rank strings;
- identify live game-state root and validate it across mission/menu transitions;
- diff controlled `usersv` samples before assuming any field layout;
- determine rank/codename counters and rules from code, then validate live.

Current evidence supports binary identification and analysis entry points only.
No gameplay offset, counter meaning, rank predicate, or injection path is yet
claimed.

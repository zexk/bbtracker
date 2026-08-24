# MG1/MG2 probe research

Target build inspected: Steam app `2131680`, October 13, 2025 binaries.

## Runtime

`METAL GEAR.exe` starts either game with `-mgst mg1` or `-mgst mg2` and loads
`mg1.dll` or `mg2.dll`. Both game DLLs are native x86-64 PE modules. Runtime
state is module-relative, so probes should wait for the selected DLL and reject
unreadable or implausible fields.

## MG2 rank state

MG2 end-screen rank evaluation starts at `mg2.dll+0x256BA`. Its counter getters
resolve to this contiguous block:

| RVA | Value |
| --- | --- |
| `0x45790` | raw game timer |
| `0x45794` | rations used |
| `0x45798` | humans killed |
| `0x4579C` | alerts |
| `0x457A0` | special-item use |
| `0x457A8` | continues |

Difficulty lives at offset `0x88` in the object reached through the pointer at
`mg2.dll+0x46DE0`.

Rank routine divides raw timer by 60. Result uses quarter-second units: its Big
Boss boundary is `0x627` (1575 quarter-seconds, 1:45:00). Full time tiers are:

| Result index | Boundary | Real time |
| --- | --- | --- |
| 9/10 | `< 0x627` | `< 1:45:00` |
| 7 | `< 0x8CA` | `< 2:30:00` |
| 6 | `< 0xE10` | `< 4:00:00` |
| 5 | `< 0x1C20` | `< 8:00:00` |
| 4 | `< 0x2A30` | `< 12:00:00` |
| 3 | `< 0x34BC` | `< 15:00:00` |
| 2 | `< 0x3F48` | `< 18:00:00` |
| 1 | `< 0x5460` | `< 24:00:00` |
| 0 | otherwise | `>= 24:00:00` |

Top-rank checks, in game order:

- humans killed: at most 10 for top tier, then at most 5 for Big Boss/Fox
- continues: 0
- alerts: at most 6
- rations: 0
- special-item use: 0
- difficulty field selects Big Boss versus Fox

## MG1 rank state

MG1 end-screen rendering starts at `mg1.dll+0x202E0`; rank evaluation starts at
`mg1.dll+0x20DB8`. State getter `mg1.dll+0x15100` returns the static block at
`mg1.dll+0x2F5E0`.

| RVA | State offset | Value |
| --- | --- | --- |
| `0x2F6A4` | `0xC4` | difficulty |
| `0x2F768` | `0x188` | raw game timer |
| `0x2F76C` | `0x18C` | rations used |
| `0x2F770` | `0x190` | humans killed |
| `0x2F774` | `0x194` | alerts |
| `0x2F778` | `0x198` | special-item use |
| `0x2F780` | `0x1A0` | continues |

Big Boss/Fox checks encoded by the evaluator:

- raw timer below `0xAFC8` (45,000 ticks at 15 Hz, `0:50:00`)
- continues: 0
- alerts: at most 8 on Original; alternate difficulty path allows 9
- humans killed: 0
- rations: at most 1
- special-item use: 0
- difficulty selects Big Boss versus Fox

The evaluator first permits at most 3 kills before entering its best-time rank
branch, then applies the strict zero-kill check for Big Boss/Fox.

Raw timer boundaries are:

| Boundary | Real time |
| --- | --- |
| `0xAFC8` | `0:50:00` |
| `0x13C68` | `1:30:00` |
| `0x1A5E0` | `2:00:00` |
| `0x34BC0` | `4:00:00` |
| `0x69780` | `8:00:00` |
| `0x9E340` | `12:00:00` |
| `0xD2F00` | `16:00:00` |
| `0x107AC0` | `20:00:00` |

`mg1.dll+0x2E5FC`, published online as health, is not health. Code clears it on
state transitions and tests it while handling player death. Do not use it as a
health anchor.

## Implementation shape

One ASI target can support both games: wait for `mg1.dll` or `mg2.dll`, select
matching probe and rules, and keep overlay unavailable until selected module and
fields pass validation. Existing D3D11 overlay path is reusable.

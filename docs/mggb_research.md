# Metal Gear: Ghost Babel rank research

Pre-implementation research for Ghost Babel (MGGB) support in bbtracker.
No Vol.2 binary inspected yet; all rules below come from community
documentation and need live-memory verification.

Scope decided with maintainer: PC (Master Collection Vol.2) only, both
projections (live stage rank + end-game codename), story 13 stages first.
Special-mission and VR-mission ranks look structurally similar
(same score inputs per attempt) and are deferred, not excluded.

## Rank system

Two layers, both trackable.

### Per-stage rank (13 story stages)

Each stage awards Excellent / Great / Good / Poor / Terrible from a
performance score. Primary source is the Perfect Guide via the Fandom
`Codename (gameplay)` Ghost Babel section:

```text
Score = (time - tgtTime) * k + killed * 5 + found * 20 + rations * 2
If time >= 1 hour, score = 0x7fff (forced Terrible)
```

| Rank | Excellent | Great | Good | Poor | Terrible |
| --- | --- | --- | --- | --- | --- |
| Score | < 0 | 0-99 | 100-349 | 350-499 | > 499 |

Per-stage constants (`tgtTime` presumed seconds; verify against guide):

| Stage | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 | 13 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| tgtTime | 55 | 150 | 420 | 420 | 720 | 240 | 80 | 720 | 360 | 120 | 840 | 240 | 60 |
| k | 2 | 1 | 1 | 1 | 1 | 1 | 5 | 1 | 1 | 2 | 1 | 2 | 12 |

Live projection needs four inputs per attempt: stage time, kills,
times found (alerts), rations used. Costs are linear, so remaining
allowances for each rank boundary can be shown exactly like other
bbtracker titles.

### End-game codename (after all 13 stages)

Displayed after the ending. Difficulty selects the name column:
Very Hard / Hard / Normal / Easy (e.g. Big Boss / Fox / Doberman /
Hound). Threshold semantics (`<`, ranges, `--` = ignored) follow the
Fandom table taken from the Perfect Guide:

| Very Hard | Hard | Normal | Easy | Found | Killed | Rations VH/H/N/E | Saves | Time |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Big Boss | Fox | Doberman | Hound | < 5 | < 24 | < 1 (all) | -- | < 2:00 |
| Falcon | Swallow | Pigeon | Sparrow | -- | -- | -- | -- | < 1:30 |
| Scorpion x3 rows | Centipede | Tarantula | Spider | 6-25 / 26-60 / 61-120 | 25-59 / 60-199 / 60-119 | -- | -- | -- |
| Coyote | Jackal | Hyena | Mongoose | 26-60 | 25-59 | -- | -- | -- |
| Jaguar | Panther | Leopard | Puma | 61-120 | 25-59 | -- | -- | -- |
| Crow | Mole | Bat | Beaver | 6-25 | 60-199 | -- | -- | -- |
| Crocodile | Alligator | Iguana | Chameleon | 61-120 | 120-199 | -- | -- | -- |
| Peacock | Parrot | Myna | Cicada | > 121 | -- | -- | -- | > 1:30 |
| Orca | Jaws | Shark | Piranha | < 121 | > 201 | -- | -- | > 1:30 |
| Whale | Mammoth | Elephant | Pig | < 121 | < 201 | > 7 / > 15 / > 25 / > 26 | -- | > 1:30 |
| Sloth | Koala | Turtle | Snail | < 121 | < 201 | < 7 / < 15 / < 25 / < 26 | -- | > 10:00 |
| Ostrich | Rabbit | Mouse | Chicken | > 121 | > 201 | > 7 / > 15 / > 25 / > 26 | > 80 | > 10:00 |

Boss Survival mode also awards a codename; no criteria table found.

## Resources surveyed

- speedrun.com/mggb: 77 followers, 192 runs, 52 players. Full-game plus
  41 level boards (per-stage Any% and Excellent, plus a Special-mission
  subset). 2 guides (Nikita Skip, Stage 03 Water Skip), 6 forums
  (Big Boss Routing, Special Missions breakdown, Leaderboard Changes).
  4 resources; the live one is NickRPGreen's autosplitter, the rest are
  legacy Excel timing sheets (iLL_Pazzo, Furry2) and casinocoin splits.
- Metal Gear Speedrunners wiki: Ghost Babel hub, per-stage routes for
  Easy / Normal / Hard / Very Hard / Big Boss, `mggb_ranks` page
  (canonical thresholds; fetch 526-blocked on 2026-09-07, retry or ask
  in Discord), frame-data and Stage-09 pillar-RNG pages.
- RetroAchievements Ghost Babel set: documents best-possible rank per
  Special mission (e.g. S01 Excellent/Great/Excellent, S02M03 max
  Great). Proves some Excellents are impossible by design and gives
  ground truth for max-rank expectations. Stage 09 Mission 02 runner
  note: sub-8-min zero-stat run still only Great.
- LP Archive playthrough and GameFAQs guides: Special-mission
  objectives list (time limits, emblem hunts, no-spotted, no-idle-bomb,
  weapon restrictions). Special missions reuse stage maps with
  modifiers, which is why their rank inputs should mirror story stages.
- Master Collection Vol.2 news (PlywoodCow, 2026-09): Vol.2 ships
  edited PAL + JPN ROMs, Versus Mode removed, title normalized to
  `Metal Gear: Ghost Babel`, codec macro menus (RTA-only concern),
  Rewind banned. Board stays unified with GBC/cart pending IGT and lag
  comparison.

## Autosplitter (NickRPGreen/Metal-Gear-Ghost-Babel-Autosplitter)

v3.1 supports `MGS MC2 Bonus Content` plus GSE, BGB/BGB64, and
Gambatte_speedrun through one signature scan (no emu-help-v3). Behavior:

- Start on gaining control of Snake
  (`Screen == 227 && Result == 0 && LvlFrames == 1`).
- Split when stage timer block resets (`LvlFrames` old > 0, new == 0),
  i.e. exiting the Stage Complete screen.
- Reset on emulator reset or return to main menu; IGT accumulated from
  per-stage timers so loads, codecs, menus, and lag are excluded.
- Watchers relative to the `MARK` signature: Screen -0x26E,
  Result +0x11A, LvlFrames +0x1E0 (int overlapping Secs/Mins/Hours
  +0x1E1-3), Life +0x2CB. These are GBC RAM mirrors; no rank counters.

The README Desirables explicitly request VAR views of rank-affecting
variables plus a projected-rank view. That is the gap bbtracker fills;
the MARK anchor plus the old GBC addresses (`0xC0AA` screen, `0xC4F8`
level timer) are the starting point for the Vol.2 probe.

## Implementation notes

- New probe likely lives under `src/games/mggb/`, rules under
  `src/common/codename/mggb`, following the `mg12` shape: wait for the
  Vol.2 bonus-content module, select Ghost Babel, gate on an explicit
  active-run state (not timer movement or stale counters).
- Needed live fields: stage id, stage time, kills, founds, rations,
  saves, total time, difficulty. Saves/time already matter for the
  codename layer even though the stage formula ignores saves.
- Special/VR ranking is deferred until story projection is verified,
  but budget for it: same score inputs per attempt, different
  tgtTime/k or fixed objectives per mission.

## Validation and unknowns

- Confirm `tgtTime` unit (seconds), `found` (alerts vs spots), `killed`
  (humans only vs all), and whether continues/special-items matter.
- Confirm rank-boundary inclusivity and the saves-column `--` semantics.
- Locate Vol.2 playlist/ROM-select state: edited PAL vs JPN ROM must
  not slip past difficulty-gated codename rows unnoticed.
- Re-check `mggb_ranks` wiki once reachable; verify Perfect Guide p.23
  against live Stage Complete screens.
- Death, Game Over/Exit, pause, and ending behavior for the visibility
  gate still need live testing. Failed probes must leave the overlay
  running with `no active ranked run` and never write game memory.

Sources: Fandom `Codename (gameplay)` Ghost Babel section (citing the
Perfect Guide on archive.org), speedrun.com/mggb resources/guides/news,
`NickRPGreen/Metal-Gear-Ghost-Babel-Autosplitter` README and ASL v3.1,
MGSR Ghost Babel hub, RetroAchievements Ghost Babel set.

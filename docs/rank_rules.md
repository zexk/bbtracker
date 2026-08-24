# MGS3 rank rule sources

Rules live in code as data (`src/common/codename/rules_mgs3.cpp`). This file records
where each threshold came from and which values still need in-game verification against
the Master Collection build (our "golden child").

## Primary source

muni_shinobu's MGS3 codename chart: https://www.tentenpro.com/muni_shinobu/mgs3/codename.html
- Precedence order as printed on the page (higher wins): BEST -> WORST -> SPECIAL -> REGULAR.
- Difficulty-tiered families map members as: [VeryEasy+Easy]=member0, Normal=member1,
  Hard=member2, Extreme=member3.

## Transcribed thresholds

Elite ladder ("BEST"):
- FOXHOUND (Extreme): no special items; alerts 0; kills 0; severe injuries <20;
  damage <5 bars; LifeMed 0; play time <5h; continues 0; saves <25.
- FOX: Hard = strict row (identical to FOXHOUND row but on Hard);
  Extreme = no special; alerts <=3; kills 0; LifeMed 0; <5h; continues 0; saves <35.
- DOBERMAN: Normal = strict row; Hard = alerts <=3, saves <35 variant;
  Extreme = alerts <=5, <5h30, no saves cap.
- HOUND: Easy = strict row; Normal/Hard/Extreme progressively looser
  (<=3 @<5h+<35 saves / <=5 @<5h30 / <=10 @<6h).

"Special item" for the elite rows = EZ Gun, Stealth Camouflage, Infinity Facepaint.
Boss-reward camos do not count.

Worst family (all conditions simultaneously): alerts >250, kills >250, play time >50h,
continues >60, saves >100, damage >30 bars, severe injuries >250, LifeMed >10.
Chicken=VE/E, Mouse=N, Rabbit=H, Ostrich=X.

Specials (chart order): Kerotan (flag), Markhor (flag), Tsuchinoko (flag),
Chameleon (alerts == 0), Leech (flag), Pigeon (kills == 0), then tier families:
severe injuries <20 (Night Owl X / Flying Fox H / Bat N / Flying Squirrel VE,E),
play time <5h (Eagle/Hawk/Falcon/Swallow), meals >250 (Whale/Mammoth/Elephant/Pig),
alerts >250 (Cow), kills >250 (Orca/Jaws/Shark/Piranha),
severe injuries >250 (Tasmanian Devil/Jackal/Hyena/Mongoose),
play time >50h (Giant Panda/Sloth/Capybara/Koala),
saves >100 (Hippopotamus/Zebra/Deer/Cat).

## Known conflicts with other community sources

1. Save cap on elite strict rows / FOX-Extreme: muni prints "Under 25" (and "Under 35");
   mainstream guides say "25 or fewer". We follow muni literally (`<25`, `<35`).
2. Meals threshold for Whale-family: muni says ">250"; older GameFAQs FAQ says "31+"
   (likely cross-contaminated from MGS2's 31-rations Elephant). We encode 250.
3. Worst-family play time: muni says >50h; GameFAQs FAQ says >30h. We encode 50.
4. Worst-family severe injuries: muni includes injuries >250; ANTIBigBoss trainer omits
   it. We keep muni's (stricter, more complete).

## Cross-checked against ANTIBigBoss/MGS3-Cheat-Trainer-GUI rank projection

Second independent source; resolved several open questions:

- RESOLVED damage units: stat is a u32 already denominated in life-bar equivalents
  (FOXHOUND cap compares against 5). No conversion needed. Conflict #2 from earlier list closed.
- RESOLVED regular fallback grid: it is NOT difficulty-tiered ("Any"). Explicit matrix:
  continues band {<=50, >=51} x kills band {1..100, >=101} x alerts
  band {<=20, 21..50, >=51} -> Scorpion/Jaguar/Iguana (k low), Tarantula/Panther/Crocodile
  (k high), Centipede/Leopard/Komodo Dragon (c high, k low), Spider/Puma/Alligator
  (c+k high). Trainer boundary holes were discarded in favor of muni's complete ranges:
  Spider `<=20`, Puma `21..50`, Komodo Dragon `>=51`.
- Cow follows muni's `>250`; trainer-derived `>300` conflicts with muni's special row
  and regular-grid upper bound.
- Markhor is count-based: plants/animals captured byte >= 48 (44 kinds + 4 cure plants),
  so no inventory-flag hook needed.
- DISAGREEMENT kept: trainer merges elite-ladder tiers into single rules (e.g. one FOX
  rule for Hard+Extreme). We keep muni's per-tier rows (classic scaling where each
  difficulty's perfect run awards that tier's top name). Verify in golden child.
- TRAINER BUG (not replicated): their Chameleon condition compares kills instead of
  alerts; every other source agrees Chameleon = zero alerts.

## MGS3 Master Collection memory map (phase 2 probe)

Source: ANTIBigBoss/MGS3-Cheat-Trainer-GUI at "Update for Game Version 3.0.0.0"
(2026-02-15), verified against a live 0x6980B92F (Feb 2026) exe via hex dumps.

- Module: `METAL GEAR SOLID3.exe`
- Stats base: `[[module+0x00ACDE98]]` (single pointer deref; RVA was 0xACBE18 pre-3.0).
  The slot holds the CURRENT engine record cursor, which moves between contexts
  (title/save/stage records carry ASCII tags like "title", "s051a", "r_sna01").
  During gameplay with a save loaded it points at the live stats block; in menus it
  points elsewhere and reads yield zeros/garbage. Re-deref every poll, never cache.
- Offsets CONFIRMED against observed gameplay data:
  - 0x34 u16 continues, 0x36 u16 saves, 0x38 u16 alerts, 0x3A u16 kills
  - 0x3D u8 special-items bitmask (observed 4 = EZ Gun)
  - 0x3F u8 plants+animals captured (observed 23/48)
  - 0x40 u16 severe injuries (observed 4)
  - 0x46 u16 meals eaten (observed 33)
  - 0x4C u32 game time frames @60fps (observed 400500 = 1h51m)
- Damage fields, confirmed by MGS Master Collection Save Decrypter:
  - 0x42 u16 is damage within current life bar and resets at bar boundaries.
  - 0x44 u16 is accumulated life bars shown on end screen. Tracker uses this exact value;
    no estimated conversion.
  - 0x06 difficulty byte uses enum {10=VeryEasy, 20=Easy, 30=Normal, 40=Hard,
    50=Extreme, 60=EuropeanExtreme} (trainer DifficultyMappings). EE maps to Extreme
    for rank purposes. The INI `[stats] difficulty=` override still wins if set.
- Robustness: reads guarded by VirtualQuery range checks; PE TimeDateStamp logged at
  resolve for version identification if offsets drift after a game patch.
- Not yet probed: Kerotan (all 64 shot), Tsuchinoko carried, Leech carried (inventory
  flags; ranks stay hidden until found).

## Regular fallback grid (approximate!)

Superseded: see "Cross-checked against ANTIBigBoss" section above for the explicit
12-rule matrix now encoded in rules_mgs3.cpp.

Flag-based ranks (Kerotan/Markhor/Tsuchinoko/Leech) will read inventory/capture flags
in Phase 2; until probed they evaluate false and simply never win precedence.

## Other games (for later phases)

- MGS2: rank factors documented in PLAN.md research section; table transcription pending.
- MGS1: Integral chart at https://www.tentenpro.com/muni_shinobu/mgs/int_codename.html

## MGS2 Master Collection memory map (phase 3, untested in-game)

Source: sagefantasma/MGS2-Cheat-Trainer (shipped offsets target MC v2.0.1,
matching RMLSNK's table maintenance timeline).

- Module: `METAL GEAR SOLID2.exe`
- Player block: `[[module+0x00949340]]` (v2.0.2 uses 0x948340 - add version
  fallback if reads come back empty after a patch)
- From player pointer:
  - 0x07 u8 gametype {0=Plant, 16=Tanker, 32=Tanker-Plant}
  - 0x10 u8 difficulty {10..60 same enum as MGS3; 60=EuroExtreme has its own
    rank column (muni's "Very Hard") and folds to Extreme for display}
  - 0x12E stats chunk: +4 i16 continues, +8 i16 saves, +10 i32 playtime
    frames@60fps, +18 i16 shots, +20 i16 alerts, +22 i16 kills, +24 i16 damage
    (raw units, approximate 48 units/bar conversion - unverified), +42 i16 mechs
  - 0x1590 i16 rations used
  - 0x1594 u16 times seen by enemy (distinct from alerts)
  - 0x1596 i16 special items used (bitmask)
  - 0x12E stats chunk +14 u16 pull-ups, +42 u16 mechs destroyed
  - 0xFA/+0xFC u16 current/max health
- Rank rules: muni MGS2 chart. Elite ladder = LADDER[min(3,s+t)] over
  strictness rows s0..s3 x tiers t (X,VH/EE,H,N,E); all require Tanker-Plant.
  BIG BOSS requires radar off. GameState discovery supplies live radar state; before
  discovery, saved special/radar usage bit `0x20` supplies fallback state.
- Not probeable yet: Sea Louse flag, Gazelle clearing-escapes counter (omitted).

## Probe robustness + remaining gaps (post phase 3)

- MGS3 stats slot is now located via signature scan
  (48 8B 0D ?? ?? ?? 00 F7 41 08 00 40 00 00 75 09 8B 05 -> RIP-relative slot,
  credit: apel/makotocchi mgs3_pc.asl) with the static RVA kept only as fallback.
  Survives game patches that move globals.
- Story flags slot = stats slot + 0x10; u16 words at +0x2/+0x4 are change-logged.
  Kerotan/Tsuchinoko/Leech bits are expected somewhere in these words or nearby
  inventory structures - correlate log lines while collecting to pin them down.
- MGS3 area code string (stats+0x24, e.g. "s051a") shown in panel; also a good
  probe-health indicator ("title" in menus).
- MGS2 radar on/off has no known address yet; BIG BOSS stays hidden unless
  bbtracker.ini sets [stats] radar_off=1. Set it only when actually playing
  radar-off. Sea Louse and Gazelle ranks remain omitted (no flag/counter found).

## MGS2 radar: solved via passive GameState discovery

RMLSNK's table (Patch201.ct) hooks `add [rcx+0x138],eax` (AOB
`01 81 38 01 00 00`) to capture a second stats struct ("GameState") that the
player-block layout doesn't cover:
- +0x06 u8 radar type {00=TYPE1 on, 20=TYPE2 on, 04=OFF}
- +0x07 u8 game-over-if-discovered {0/16 off, 8/40 on; also encodes mission}
- +0x11A u16 alert state {0 none,1 alert,2 evasion,3 caution}
- +0x132/+0x136/+0x138/+0x140/+0x142 mirrors of continues/saves/gametime/
  shots/alerts

No static global holds it, so instead of injecting a mid-instruction hook we
scan writable memory for candidates passing enum plausibility AND matching our
already-proven player-block reads across five fields simultaneously
(continues+saves+alerts+shots+gametime). Scan throttled to one pass per ~4s
until found; cached hit is revalidated every poll. radar_off then reads
directly from byte@+6, making BIG BOSS attainable in the tracker for genuine
radar-off runs; the [stats] radar_off ini override still wins if set.

## MGS1 variant split (1998 vs Integral) - important

MC ships BOTH MGS1 versions (launcher picks the ISO). Two different rank systems:
- Original 1998: muni /mgs/codename.html. BIG BOSS adds radar-off over FOX ladder
  (disc<4/kills<25/rations<=1/cont=0/<3h). US/EU gate FOX to Hard and BIG BOSS
  to Extreme. Japanese original has no difficulty choice, runs Easy-equivalent,
  and bypasses both elite difficulty gates. Falcon(<2.5h),
  Jaws(>250 kills), Pig(>120 rations), Hippo(>80 saves), Turtle(>18h),
  Chicken(all three); regular grid Leopard/Grizzly/Jackal/Tarantula/Gazelle via
  discovered bands x Y=10*disc/(kills-25).
- Integral: muni /mgs/int_codename.html (the 64-rule system previously encoded).
rules_mgs1.cpp currently implements the 1998 system since the user plays US
original. If Integral support is ever needed, preserve both variants behind a
config flag - the work-array probe is shared, only rules differ.

bmn/livesplit_asl_mgs1 confirms current/max life `+0x29/+0x2B`. Game-time frames sit
at work-array `-0x939D` for US/Integral and `-0x9495` for Japanese original; probe
selects whichever advances at game-clock rate. Original PSX tooling documents 30 fps,
but empirical Master Collection
timing advances this exposed counter at 60 fps; tracker uses 60. These make MGS1
time-based projection and grey health display
available across both tested editions.

MGS1 probe finds live stage records and scores matching work arrays, then watches unknown
bytes automatically. It does not depend on attaching during the `opening` stage.
Launch game, play toward target event, and inspect `mgs1 event candidate` log lines;
known rank fields are excluded and noisy offsets suppress themselves after five changes.
Overlay maps stage IDs to room names using bmn/livesplit_asl_mgs1's complete Location
dictionary and keeps raw ID beside name, for example `Dock (s00a)`.
Hidden Diazepam duration uses universal work-array `+0xA5` signed-frame timer and appears
only while active; source maximum is 1200 frames, converted at PSX logic rate of 30 fps.

In-game MC US-original checks confirmed live kills `+0xB1`, rations used `+0xBF`,
and continues `+0xC1`, including reset after loading an older save. Counter side of
1998 rank formula is therefore empirically working. bmn's version maps place radar state
nine bytes after Location in every supported PSX layout, giving work-array `+0x0C`:
`0x00` visible, `0x20` hidden. Probe latches any visible state during gameplay so an
alert or chaff cannot temporarily qualify radar-on runs. INI override still wins.

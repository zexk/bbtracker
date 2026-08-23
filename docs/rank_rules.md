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

## Known conflicts with other community sources (needs in-game verification)

1. Save cap on elite strict rows / FOX-Extreme: muni prints "Under 25" (and "Under 35");
   mainstream guides say "25 or fewer". We encode inclusive caps (`<=25`, `<=35`).
2. Meals threshold for Whale-family: muni says ">250"; older GameFAQs FAQ says "31+"
   (likely cross-contaminated from MGS2's 31-rations Elephant). We encode 250.
3. Worst-family play time: muni says >50h; GameFAQs FAQ says >30h. We encode 50.
4. Worst-family severe injuries: muni includes injuries >250; ANTIBigBoss trainer omits
   it. We keep muni's (stricter, more complete).

## Cross-checked against ANTIBigBoss/MGS3-Cheat-Trainer-GUI rank projection

Second independent source; resolved several open questions:

- RESOLVED damage units: stat is a u32 already denominated in life-bar equivalents
  (FOXHOUND cap compares against 5). No conversion needed. Conflict #2 from earlier list closed.
- RESOLVED regular fallback grid: it is NOT difficulty-tiered ("Any"). Explicit matrix,
  encoded verbatim: continues band {<=50, >=51} x kills band {1..100, >=101} x alerts
  band {<=20, 21..50, >=51} -> Scorpion/Jaguar/Iguana (k low), Tarantula/Panther/Crocodile
  (k high), Centipede/Leopard/Komodo Dragon (c high, k low), Spider/Puma/Alligator
  (c+k high). Komodo Dragon has an odd extra cell (alerts 81..248 AND injuries >=21);
  encoded as printed. Spider uses alerts <=19 per source.
- Cow changed 250 -> >300 (GameFAQs FAQ + trainer agree on 300; muni says 250).
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
- CORRECTED vs trainer doc:
  - 0x42 damage is u16 in raw engine units, NOT bars (grew 3->73 while taking hits).
    Trainer's u32 read spans the unknown u16 field at 0x44 (observed constant 1).
    Scale derived from the MGS2 trainer (sagefantasma): its datamined Big Boss cap is
    DamageTaken <= 500 raw units against the community "<10.5 life bars" requirement
    -> ~48 units/bar. We use kDamageUnitsPerBar = 48; recalibrate with a controlled
    single-hit test if tracker ratios look wrong. NOTE: not yet verified empirically
    against the game's own end-of-rank screen, but in-game values look plausible.
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
    (raw units, same ~48/bar assumption as MGS3 - unverified), +42 i16 mechs
  - 0x1590 i16 rations used
  - 0x1596 i16 special items used (bitmask)
- Rank rules: muni MGS2 chart. Elite ladder = LADDER[min(3,s+t)] over
  strictness rows s0..s3 x tiers t (X,VH/EE,H,N,E); all require Tanker-Plant.
  BIG BOSS requires radar off which we cannot probe yet -> stays hidden until
  a radar source is found (CE table lists Radar Type values 0/4/20).
- Not probeable yet: Sea Louse flag, Gazelle clearing-escapes counter (omitted).

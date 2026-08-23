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

Source: ANTIBigBoss/MGS3-Cheat-Trainer-GUI (Constants.cs / MainPointerManager.cs).

- Module: `METAL GEAR SOLID3.exe`
- Stats base: `[[module+0x00ACDE98]]` (single pointer deref)
- Offsets from stats base:
  - 0x06 u8 difficulty (enum mapping unverified; values >4 render "(?)" in panel)
  - 0x34 u16 continues, 0x36 u16 saves, 0x38 u16 alerts, 0x3A u16 kills
  - 0x3D u8 special-items bitmask (1=stealth, 2=infinity facepaint, 4=EZ gun)
  - 0x3F u8 plants+animals captured, 0x40 u16 severe injuries, 0x42 u32 total damage (bars)
  - 0x46 u16 meals eaten, 0x4C u32 game time (frames @60fps), 0x5A8 u16 LifeMed used
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

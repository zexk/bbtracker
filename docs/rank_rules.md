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
2. Damage cap on elite strict rows: muni says "<5 life bars"; Siliconera/Game8 guides say
   "<10". We encode 5. Verify by probing the stat counter while taking damage on a rank run.
3. Meals threshold for Whale-family: muni says ">250"; older GameFAQs FAQ says "31+"
   (likely cross-contaminated from MGS2's 31-rations Elephant). We encode 250.
4. Worst-family play time: muni says >50h; GameFAQs FAQ says >30h. We encode 50.

## Regular fallback grid (approximate!)

The chart prints a nested grid (Scorpion/Tarantula/Centipede/Spider,
Jaguar/Panther/Leopard/Puma, Iguana/Crocodile/Komodo Dragon/Alligator) whose per-cell
alert/continue/kills ranges are ambiguous in flat text. Current approximation:
kills >= 1 AND alert bands [1..20] -> Scorpion-family, [21..50] -> Jaguar-family,
[>=51] -> Iguana-family, member chosen by difficulty tier. This is flagged
verify-in-game via the golden child once Phase 2 probes are wired.

Flag-based ranks (Kerotan/Markhor/Tsuchinoko/Leech) will read inventory/capture flags
in Phase 2; until probed they evaluate false and simply never win precedence.

## Other games (for later phases)

- MGS2: rank factors documented in PLAN.md research section; table transcription pending.
- MGS1: Integral chart at https://www.tentenpro.com/muni_shinobu/mgs/int_codename.html

# MGS4 research

MGS4 support assumes Master Collection values match the PlayStation 3 release
until live probing or game data proves otherwise. The game awards every matching
emblem, then displays the highest-priority match as the run title.

## Emblem rules

| Priority | Emblem | PS3 requirement |
| ---: | --- | --- |
| 1 | BIG BOSS | The Boss Extreme; 0 alerts, kills, continues, and recovery items; under 5:00; no Stealth Camouflage or Bandana |
| 2 | FOX HOUND | Big Boss Hard or higher; fewer than 3 alerts; 0 kills, continues, and recovery items; under 5:30; no Stealth Camouflage or Bandana |
| 3 | FOX | Solid Normal or higher; fewer than 5 alerts; 0 kills, continues, and recovery items; under 6:00; no Stealth Camouflage or Bandana |
| 4 | HOUND | Naked Normal or higher; fewer than 10 alerts; 0 kills, continues, and recovery items; under 6:30; no Stealth Camouflage or Bandana |
| 5 | MANTIS | 0 alerts, continues, and recovery items; under 5:00 |
| 6 | WOLF | 0 continues and recovery items |
| 7 | RAVEN | Under 5:00 |
| 8 | OCTOPUS | 0 alerts |
| 9 | BEAR | 100 CQC choke knockouts |
| 10 | EAGLE | 150 headshots |
| 11 | ASSASSIN | 50 knife defeats, 50 CQC holds, and fewer than 25 alerts |
| 12 | PIGEON | 0 kills |
| 13 | BLUE BIRD | Give 50 items to militia or rebels |
| 14 | HAWK | Receive 25 praises from militia or rebels |
| 15 | LITTLE GRAY | Acquire all 69 counted weapons |
| 16 | ANT | Search 50 held-up enemies |
| 17 | GIBBON | Hold up 50 enemies |
| 18 | TORTOISE | Spend 60 minutes in cardboard box or drum can |
| 19 | RABBIT | Turn 100 magazine pages |
| 20 | BEE | Use Syringe or Scanning Plug on 50 enemies |
| 21 | GECKO | Spend 60 minutes pressed against walls |
| 22 | SCARAB | Perform prone side rolls; exact threshold unresolved |
| 23 | FROG | Perform forward rolls; exact threshold unresolved |
| 24 | INCH WORM | Crawl for 60 minutes |
| 25 | LOBSTER | Crouch for 150 minutes |
| 26 | HYENA | Pick up 400 weapons or items |
| 27 | HOG | Enter Combat High 10 times |
| 28 | PIG | Use 40 recovery items |
| 29 | COW | Trigger 100 alerts |
| 30 | CROCODILE | Kill 400 enemies |
| 31 | GIANT PANDA | Exceed 30:00 |
| 32 | SCORPION | Low alerts, low kills, low continues |
| 33 | TARANTULA | Low alerts, high kills, low continues |
| 34 | CENTIPEDE | Low alerts, low kills, high continues |
| 35 | SPIDER | Low alerts, high kills, high continues |
| 36 | JAGUAR | High alerts, low kills, low continues |
| 37 | PANTHER | High alerts, high kills, low continues |
| 38 | LEOPARD | High alerts, low kills, high continues |
| 39 | PUMA | High alerts, high kills, high continues |
| 40 | CHICKEN | 150 alerts, 500 kills, 50 continues, 50 recovery items, and at least 35:00 |

The preliminary rules use contiguous splits at 75 alerts, 250 kills, and 25
continues: values below each threshold are low and values at or above it are
high. Published guides disagree on inclusive boundaries; verify these choices
against game logic or exact end-screen values.

## Required live fields

Existing `GameStats` covers difficulty, alerts, kills, continues, recovery-item
use, play time, and special-item use. MGS4 additionally needs counters or flags
for CQC choke knockouts, headshots, knife defeats, CQC holds, donated items,
praises, acquired weapons, body searches, hold-ups, box time, magazine pages,
Syringe/Scanning Plug uses, wall time, both roll types, crawl time, crouch time,
pickups, and Combat Highs.

Prefer reading the game's accumulated emblem counters over reconstructing them
from actions. Probe must remain read-only and leave stats unavailable when a
field cannot be validated.

## Sources and open questions

- [Piggyback official-guide sample](https://www.piggyback.com/en/wp-content/uploads/sites/7/2020/04/MGS4_E_SamplePages.pdf): priority order and exact elite rules.
- [GameFAQs Emblem FAQ](https://gamefaqs.gamespot.com/ps3/926596-metal-gear-solid-4-guns-of-the-patriots/faqs/53112): all 40 PS3 emblems.
- [MetalGearWeb emblem list](https://www.metalgearweb.it/mgs4/emblemi): full ordered cross-check.

Before rule code lands, verify:

- preliminary regular-grid equality behavior at 75 alerts, 250 kills, and 25 continues;
- whether special thresholds use `>=` or `>` at displayed values;
- preliminary Scarab/Frog mapping: 100 side rolls and 200 forward rolls;
- whether MANTIS uses deaths or continues internally (published guides differ);
- whether LITTLE GRAY is a live weapon-count check or persistent unlock flag;
- actual Master Collection difficulty encoding and timer units.

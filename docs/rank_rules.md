# Rank rules and probe notes

Rule tables live under `src/common/codename/`. Community documentation is not
always consistent, so unresolved differences are listed here instead of being
hidden in code comments.

## MGS1

Source: [muni_shinobu's original MGS codename chart](https://www.tentenpro.com/muni_shinobu/mgs/codename.html).

Original and Integral use separate rank tables; Integral's 64-rule,
difficulty-tiered system comes from muni's `/mgs/int_codename.html`. Probe picks
the variant from the disc serial found in emulated memory (both `SLPM_86x.xx`
path and `SLPM-86xxx` code spellings), rescanning until a serial latches.
Serials were verified against Master Collection Vol.1's own images
(`windata/alldata.bin`, `windata/dlc/dlc_japan.bin`):

- Japanese original: Premium Package `SLPM-86111/86112` (DLC).
- Integral: `SLPM-86247/86248/86249` (DLC).
- Western original: US (`SLUS-00594/00776`) and EU localizations
  (`SLES-01370/11370` UK, `SLES-01506/11506` FR,
  `SLES-01507/11507` DE, `SLES-01508/11508` IT, `SLES-01734/11734` ES).
  VR Missions (`SLUS-00957`) and Special Missions
  (`SLES-02136`) contain no ranked campaign and are intentionally ignored.
  Unrecognized serials keep tracker hidden so no unknown region can slip past
  FOX/BIG BOSS gates.

The tracker image embeds the same strings and is excluded from the scan.
When game-time resets for a newly booted image, probe clears latched serial and
timer offset, then discovers both again. This supports returning to collection
menu and launching another edition without restarting process.
Region never derives from memory layout: game-time offset selection (western
originals and Integral `-0x939D`, assumed shared since EU is the only western
edition probed; JP `-0x9495`; third candidate `-0x9B11` of unknown provenance)
only picks which counter to read, and a mismatch with the serial variant is
logged as a warning.

Rank differences by edition, per muni's chart and the speedrun community:
the Japanese original offers only its fixed Easy-equivalent difficulty and
still ranks rank 1 BIG BOSS, so its elite rows carry no difficulty gate. The
US/EU originals and Integral gate FOX to Hard and BIG BOSS to Extreme while
sharing the same stat thresholds.

Probe uses scored live-stage records derived from NeopolitanDreamz's
`MGS1_master.CT` and bmn's `livesplit_asl_mgs1` layouts. Confirmed live fields include
kills, rations, continues, alerts, saves, game time, health, difficulty, radar state,
and Diazepam duration.

## MGS2

Sources: [muni_shinobu's MGS2 codename chart](https://www.tentenpro.com/muni_shinobu/mgs2/codename.html),
RMLSNK's Master Collection Cheat Engine table, and sagefantasma's MGS2 trainer.

Rank priority, difficulty tiers, minute rounding, and inclusive boundaries follow the
HD source `show_codename.c`. European Extreme is normalized to Extreme before judging.

Probe reads player statistics from Master Collection player block. Radar state
comes from a passively discovered GameState record whose mirrored counters must match
player block. Discovery scans writable memory periodically until a valid record
is found, then revalidates cached address.

Sea Louse and Gazelle use `GM_ShipwormFlag` and `GM_ClearingCount`, identified from
the HD source layout and confirmed by their position beside already-known player fields.

## MGS3

Primary source: [muni_shinobu's MGS3 codename chart](https://www.tentenpro.com/muni_shinobu/mgs3/codename.html).
Rules were cross-checked against ANTIBigBoss's MGS3 Cheat Trainer.

Intentional source choices:

- Elite save caps follow muni literally: under 25 and under 35, not 25/35 or fewer.
- Whale-family meal threshold uses more than 250. Some older guides say 31.
- Worst-family time threshold uses more than 50 hours. Some guides say 30.
- Worst-family includes more than 250 severe injuries.
- Chameleon requires zero alerts. Trainer code appears to compare kills instead.
- Markhor uses at least 48 captured plant/animal types.

Probe locates stats slot by signature and retains a known static address as
fallback. Damage bars use accumulated end-screen field, not an estimated unit
conversion. Difficulty byte values are 10 through 60; European Extreme uses Extreme
rank rules.

Kerotan count comes from a 64-bit hit mask at live stats-block offset `0x6532`.
Checklist order is raw bits 1 through 63 followed by raw bit 0, so overlay rotates
mask right one bit before display. Current-area state maps live area code to that
rotated checklist bit.

Game commits new hits to global mask during area transition. After loading a save,
probe waits for title stats block to be replaced before accepting Kerotan data; this
normally requires at least one screen transition, and count remains empty until then.
Transitions can expose temporary zero, fill pattern `01 00 FF FF FF FF FF FF`, or
stale stats copies with older masks. Probe ignores fill pattern and unions valid masks
monotonically across runtime copies. Returning to title arms a fresh cache, seeded only
after loaded save receives its own stats block.

Leech scans 50 injury records from stats-block offset `0x688`. Records are `0x0E`
bytes; type `7` with positive injury health means a leech remains attached.

Tsuchinoko checks all three live-animal cages for food type `130` with an active
occupancy record. Cage memory was verified by changing a live rabbit (`114`) into
a Tsuchinoko and carrying it across an area transition.

## Snake's Revenge

Snake's Revenge is a possible future target, deferred until after Master Collection
Vol. 2 support. Its one-to-six-star rank is character progression, not an
end-of-playthrough performance grade: rescuing hostages and interrogating officers
raises maximum life and item capacity. Documented cumulative thresholds are 0, 4,
9, 14, 19, and 22 rescues/interrogations. Rank is retained by password, and no
ending rank based on time, kills, alerts, continues, saves, or damage is documented.

Sources: [Konami's Master Collection manual](https://metalgear.konami.net/manual/mc1/snakes_revenge/ps4/en/page09.html),
[original NES manual transcript](https://www.world-of-nintendo.com/manuals/nes/snakes_revenge.shtml),
and [Dammit9x's rank and memory guide](https://gamefaqs.gamespot.com/nes/587630-snakes-revenge/faqs/53499).

## Verification

Native tests validate encoded thresholds and boundary behavior. Live addresses and
rank projections still require in-game checks after game updates. Failed probes must
leave overlay running with `no active ranked run` and must never write game memory.

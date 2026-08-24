# Rank rules and probe notes

Rule tables live under `src/common/codename/`. Community documentation is not
always consistent, so unresolved differences are listed here instead of being
hidden in code comments.

## MGS1

Source: [muni_shinobu's original MGS codename chart](https://www.tentenpro.com/muni_shinobu/mgs/codename.html).

Original and Integral use separate rank tables. Probe detects Integral from its
`SLPM-86247`/`SLPM-86248` disc serial and applies its 64-rule, difficulty-tiered
system. Japanese original uses original rules without US/EU FOX and BIG BOSS
difficulty gates.

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

Known gaps: Kerotan, Tsuchinoko, and Leech game fields remain unavailable, so those
ranks are omitted.

## Verification

Native tests validate encoded thresholds and boundary behavior. Live addresses and
rank projections still require in-game checks after game updates. Failed probes must
leave overlay running with `stats unavailable` and must never write game memory.

#!/usr/bin/env python3
"""Generate src/games/mgs4/flashbacks.h from the MGS4 game files.

The 222 unique-flashback cue IDs live in a sorted table inside mgs4.exe.
Each cue also has a poster JPG named by its ID inside the stage_data VPAKs.
This tool reads both and emits a bit-indexed table for the overlay.
"""
import argparse
import collections
import json
import os
import re
import struct
import sys

HEADER = struct.Struct("<4sHHII")
ENTRY = struct.Struct("<QQQQIIQ")
TABLE_RVA = 0x18E1230
RDATA_RVA_DELTA = 0xC00

# Interstitial scenes use stage numbers unrelated to campaign order.
STAGE_ORDER = {
    "s00a00l": (0, 0),
    "s10a10l": (0, 50),
    "s10a20l": (1, 99),
    "s10a30l": (2, 99),
    "s10a40l": (3, 99),
    "s20a00l": (4, 90),
    "s20a10l": (4, 91),
    "s20a20l": (5, 90),
    "s30a00l": (5, 99),
    "s30a10l": (5, 100),
    "s00a10l": (5, 101),
}

STAGE_AREAS = {
    "s00a00l": "Cemetery: Snake's mission briefing",
    "s00a10l": "Cemetery: Big Boss debriefing",
    "s01a00l": "Middle East Infiltration",
    "s01a05l": "Red Zone Outskirts",
    "s01a10l": "Red Zone: MK.II reunion",
    "s01a20l": "Militia Safehouse",
    "s01a30l": "Urban Ruins",
    "s01a40l": "Advent Palace: Rat Patrol reunion",
    "s01a50l": "Crescent Meridian",
    "s01a57l": "Millennium Park",
    "s01a60l": "Liquid's Encampment: Liquid's attack",
    "s02a10l": "Cove Valley Village",
    "s02a20l": "Power Station",
    "s02a25l": "Power Station: Drebin's briefing",
    "s02a30l": "Confinement Facility",
    "s02a40l": "Vista Mansion",
    "s02a50l": "Research Lab",
    "s02a60l": "Mountain Trail / Riverside",
    "s02a70l": "Vamp Ambush",
    "s02a80l": "High Woodlands Highway",
    "s02a85l": "Marketplace: Raiden's rescue",
    "s02a90l": "Marketplace",
    "s02a95l": "Marketplace Plaza",
    "s03a00l": "Eastern Europe Station",
    "s03a10l": "Midtown: Resistance Tail",
    "s03a15l": "Midtown: Resistance Tail",
    "s03a16l": "Midtown: Canals",
    "s03a20l": "Midtown: Plaza",
    "s03a25l": "Midtown: North Sector",
    "s03a30l": "Church: Big Mama's revelations",
    "s03a35l": "Motorcycle Chase",
    "s03a40l": "Motorcycle Chase",
    "s03a50l": "Raging Raven Ambush",
    "s03a60l": "Motorcycle Chase",
    "s03a65l": "Echo's Beacon",
    "s03a70l": "Echo's Beacon",
    "s03a90l": "Volta River",
    "s04a05l": "Metal Gear Solid Flashback",
    "s04a10l": "Snowfield / Heliport / Tank Hangar",
    "s04a20l": "Nuclear Warhead Storage Building",
    "s04a30l": "Snowfield / Communications Tower",
    "s04a40l": "Blast Furnace / Casting Facility",
    "s04a50l": "Underground Base",
    "s04a60l": "Underground Supply Tunnel",
    "s04a65l": "REX Escape",
    "s04a68l": "Port Area",
    "s04a70l": "Port Area: REX vs. RAY",
    "s04a75l": "After REX vs. RAY: Raiden's memories",
    "s05a10l": "Ship Bow",
    "s05a20l": "Command Center: Mantis and Meryl",
    "s05a30l": "Microwave Corridor",
    "s05a40l": "GW",
    "s05a45l": "Liquid Ocelot: Prelude",
    "s05a50l": "Liquid Ocelot",
    "s05a55l": "Liquid Ocelot: Aftermath",
    "s10a10l": "Nomad Mission Briefing",
    "s10a20l": "Nomad: South America Briefing",
    "s10a30l": "Nomad: Eastern Europe Briefing",
    "s10a40l": "Nomad: Shadow Moses Briefing",
    "s20a00l": "USS Missouri",
    "s20a10l": "USS Missouri vs. Outer Haven",
    "s20a20l": "Campbell's Room",
    "s30a00l": "Wedding",
    "s30a10l": "Hospital",
}

# Cues whose poster only ships in shared slot caches have no stage site. Their
# labels come from matching the extracted poster to the trigger prompt list;
# the rank places them in campaign order. A cue missing here keeps sorting last.
CUE_OVERRIDES = {
    0x00554b29: ("Act 2 \u00b7 Jungle: MGS3 memory", (2, 10)),
    0x00554b2e: ("MGS2: Rose and Raiden codec", None),
    0x00554b4e: ("MGS1: Briefing camera", None),
    0x00554b8e: ("Act 4 \u00b7 Otacon's office: first meeting", (4, 20)),
    0x00554bae: ("Act 4 \u00b7 Otacon's office: first meeting", (4, 20)),
    0x00654b06: ("Act 2 \u00b7 Jungle: Solidus memory", (2, 10)),
    0x00654b10: ("Act 2 \u00b7 Jungle: Vamp memory", (2, 10)),
    0x00654b26: ("Act 2 \u00b7 Jungle: Solidus memory", (2, 10)),
    0x00654b30: ("Act 2 \u00b7 Jungle: Vamp memory", (2, 10)),
    0x00654b46: ("Act 2 \u00b7 Jungle: Raiden vs. Solidus", (2, 10)),
    0x00654b50: ("Act 2 \u00b7 Jungle: Vamp memory", (2, 10)),
    0x00654b66: ("Act 2 \u00b7 Jungle: Solidus memory", (2, 10)),
    0x00654bae: ("Act 4 \u00b7 Underground hangar: REX", (4, 20)),
    0x00654bce: ("Act 4 \u00b7 Underground hangar: REX", (4, 20)),
    0x00754b8e: ("Act 4 \u00b7 Otacon's office: keycards", (4, 20)),
    0x00754bae: ("Act 4 \u00b7 Otacon's office: keycards", (4, 20)),
    0x00754bce: ("Act 4 \u00b7 Otacon's office: first meeting", (4, 20)),
}


def read_exact(stream, size):
    data = stream.read(size)
    if len(data) != size:
        raise ValueError("truncated VPAK")
    return data


def stage_rank(stage):
    if stage in STAGE_ORDER:
        return STAGE_ORDER[stage]
    act = int(stage[1:3])
    return (act, int(stage[4:6]))


def list_jpg_paths(pak_paths):
    """Return {cue_id: [path, ...]} for JPG entries named by a cue ID."""
    found = collections.defaultdict(list)
    for pak in pak_paths:
        with open(pak, "rb") as stream:
            magic, version, flags, count, index_size = HEADER.unpack(read_exact(stream, HEADER.size))
            if magic != b"VPAK" or version != 3:
                raise ValueError(f"not a VPAK archive: {pak}")
            stream.seek(0, 2)
            stream.seek(stream.tell() - index_size)
            for _ in range(count):
                path_size, path_flags = struct.unpack("<IH", read_exact(stream, 6))
                path = read_exact(stream, path_size).decode()
                meta = ENTRY.unpack(read_exact(stream, ENTRY.size))
                if meta[5] < 1:
                    continue
                read_exact(stream, 8 * (meta[5] - 1))
                base = os.path.basename(path)
                if not base.endswith(".jpg"):
                    continue
                try:
                    cue = int(base[:-4], 16)
                except ValueError:
                    continue
                found[cue].append(path)
    return found


def load_ids(exe_path):
    with open(exe_path, "rb") as stream:
        stream.seek(TABLE_RVA - RDATA_RVA_DELTA)
        data = stream.read(222 * 4)
    if len(data) != 222 * 4:
        raise ValueError("short read from mgs4.exe")
    return list(struct.unpack("<222I", data))


def canonical_site(paths):
    sites = []
    for path in paths:
        match = re.search(r"/(s\d{2}a\d{2}l)/", path)
        if match:
            sites.append((match.group(1), path.startswith("ww/")))
    common = [stage for stage, is_ww in sites if not is_ww]
    if common:
        return min(set(common), key=lambda s: (stage_rank(s), s))
    if sites:
        return min({stage for stage, _ in sites}, key=lambda s: (stage_rank(s), s))
    return None


def label_for(site):
    if site is None:
        return "Unmapped (shared cache)"
    area = STAGE_AREAS.get(site, "Hidden")
    if site.startswith("s0") and "a" in site:
        act = int(site[1:3])
        if 1 <= act <= 5:
            return f"Act {act} \u00b7 {area}"
    return area


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exe", required=True)
    parser.add_argument("--pak", action="append", required=True,
                        help="stage_data_compressed VPAK; pass common and ww paks")
    parser.add_argument("--out", required=True)
    args = parser.parse_args()

    ids = load_ids(args.exe)
    paths = list_jpg_paths(args.pak)
    missing = [cue for cue in ids if cue not in paths]
    if missing:
        sys.exit(f"{len(missing)} cue IDs have no JPG: {[hex(m) for m in missing]}")

    sites = [canonical_site(paths[cue]) for cue in ids]

    def cue_rank(index):
        override = CUE_OVERRIDES.get(ids[index])
        if override and override[1] is not None:
            return override[1]
        site = sites[index]
        return stage_rank(site) if site else (99, 0)

    story_order = sorted(range(len(ids)), key=lambda i: (cue_rank(i), ids[i]))

    lines = [
        "// Generated by scripts/gen-mgs4-flashbacks.py; do not edit.",
        "// Bit index into the 28-byte unique-flashback array at save 0x5a49.",
        "// Labels approximate trigger sites from poster caches; some caches preload later cues.",
        "// Cues with no stage cache use identified prompt labels instead.",
        "#pragma once",
        "",
        "#include <cstdint>",
        "#include <iterator>",
        "",
        "namespace bb::mgs4 {",
        "",
        "struct Flashback {",
        "    uint32_t cue_id;",
        "    const char* trigger;",
        "};",
        "",
        "inline constexpr Flashback kFlashbacks[] = {",
    ]
    for index, cue in enumerate(ids):
        site = sites[index]
        override = CUE_OVERRIDES.get(cue)
        label = override[0] if override else label_for(site)
        all_sites = sorted({m.group(1) for p in paths[cue] for m in [re.search(r"/(s\d{2}a\d{2}l)/", p)] if m})
        lines.append(f"    {{{cue:#010x}, \"{label}\"}},  // {index:3d}  {','.join(all_sites) or 'shared slot'}")
    lines += [
        "};",
        "",
        "// Campaign order; values remain bit indexes into kFlashbacks and the save.",
        "inline constexpr uint16_t kFlashbacksStoryOrder[] = {",
    ]
    for start in range(0, len(story_order), 16):
        lines.append("    " + ", ".join(str(i) for i in story_order[start:start + 16]) + ",")
    lines += [
        "};",
        "",
        "static_assert(std::size(kFlashbacks) == 222);",
        "static_assert(std::size(kFlashbacksStoryOrder) == std::size(kFlashbacks));",
        "",
        "}  // namespace bb::mgs4",
        "",
    ]
    with open(args.out, "w") as fh:
        fh.write("\n".join(lines))
    print(f"wrote {args.out}: {len(ids)} entries, {len(missing)} missing")


if __name__ == "__main__":
    main()

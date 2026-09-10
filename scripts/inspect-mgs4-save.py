#!/usr/bin/env python3
"""Decode one MGS4.SAV and print tracker-relevant counters as JSON."""

import argparse
import json
import struct
import sys

KEY = b"kjdyeAiwoGsklcmfu93lwsENf7845ghw523if0ul7Pkj0hn9ejwksSVE8twf03te623DA842rc4oiQL"
FOOTER = b"XPQT3Q5\0"
BODY_SIZE = 0x8344


def scenario_phase(progress):
    for upper_bound, phase in ((51, "ACT 1"), (101, "ACT 2"), (163, "ACT 3"),
                               (223, "ACT 4"), (262, "ACT 5"), (292, "ENDING/RESULT")):
        if progress < upper_bound:
            return phase
    return "UNKNOWN"


def decode(raw):
    if len(raw) != BODY_SIZE + len(FOOTER) + 1 or raw[-1:] != b"\n" or raw[-9:-1] != FOOTER:
        raise ValueError("unsupported MGS4.SAV framing")
    return bytes(byte ^ KEY[index % len(KEY)] for index, byte in enumerate(raw[:BODY_SIZE]))


def inspect(body):
    u16 = lambda offset: struct.unpack_from("<H", body, offset)[0]
    u32 = lambda offset: struct.unpack_from("<I", body, offset)[0]
    difficulty_raw = u16(0x006)
    progress = u32(0x054)
    stage = body[0x034:0x03B].split(b"\0", 1)[0].decode("ascii", errors="replace")
    weapon_states = [u16(0x1D4 + index * 2) for index in range(95)]
    base_weapon_slots = [u16(0x352 + index * 2) for index in range(68)]
    return {
        "clear_count": u32(0x000),
        "unknown_0004": u16(0x004),
        "difficulty_raw": difficulty_raw,
        "difficulty": {20: "LIQUID EASY", 30: "NAKED NORMAL", 35: "SOLID NORMAL",
                       40: "BIG BOSS HARD", 50: "THE BOSS EXTREME"}.get(difficulty_raw, "UNKNOWN"),
        "stage": stage,
        "scenario_progress": progress,
        "scenario_phase": scenario_phase(progress),
        "continues": u16(0x158),
        "play_time_ticks": u32(0x168),
        "play_time_seconds": u32(0x168) / 60,
        "alerts": u16(0x16E),
        "kills": u16(0x178),
        "special_item_value": u16(0x17A),
        "bandana_used": bool(u16(0x17A) & 1),
        "stealth_camouflage_used": bool(u16(0x17A) & 2),
        "cqc_uses": u16(0x180),
        "headshots": u16(0x182),
        "knife_defeats": u16(0x184) + u16(0x186),
        "side_rolls": u16(0x188),
        "forward_rolls": u16(0x18A),
        "combat_highs": u16(0x18C),
        "weapon_pickups": u16(0x18E),
        "item_pickups": u16(0x190),
        "pickups": u16(0x18E) + u16(0x190),
        "hold_ups": u16(0x192),
        "body_searches": u16(0x194),
        "praises": u16(0x196),
        "items_given": u16(0x198),
        "syringe_scanning_uses": u16(0x19A) + u16(0x19C),
        "magazine_pages": u16(0x19E) + u16(0x1A0),
        "crouch_time_ticks": u32(0x1A8),
        "crawl_time_ticks": u32(0x1AC),
        "wall_time_ticks": u32(0x1B4),
        "box_drum_time_ticks": u32(0x1B8) + u32(0x1BC),
        "drebin_points": u32(0x1C0),
        "unknown_01c4": u32(0x1C4),
        "recovery_items_used": u16(0xAE0),
        "flashbacks_viewed": u16(0x5A34),
        "counted_weapon_types": sum(state in (1, 2) for state in weapon_states[1:74]),
        "little_gray_weapon_progress": sum(
            state in (1, 2) for index, state in enumerate(weapon_states[1:74], 1) if index != 11),
        "weapon_11_state": weapon_states[11],
        "base_weapon_slots_owned": sum(value != 0xFFFF for value in base_weapon_slots),
    }


def self_test():
    body = bytearray(BODY_SIZE)
    struct.pack_into("<IHH", body, 0x000, 3, 1, 50)
    body[0x034:0x03B] = b"s05a20l"
    struct.pack_into("<I", body, 0x054, 287)
    struct.pack_into("<I", body, 0x168, 3600)
    struct.pack_into("<H", body, 0x17A, 3)
    struct.pack_into("<HH", body, 0x184, 20, 30)
    struct.pack_into("<HH", body, 0x18E, 150, 250)
    struct.pack_into("<II", body, 0x1B8, 100000, 116000)
    struct.pack_into("<II", body, 0x1C0, 123456789, 123456789)
    struct.pack_into("<H", body, 0x5A34, 273)
    struct.pack_into("<95H", body, 0x1D4, *([0] * 95))
    struct.pack_into("<H", body, 0x1D4 + 1 * 2, 2)
    struct.pack_into("<H", body, 0x1D4 + 25 * 2, 1)
    struct.pack_into("<68H", body, 0x352, *([0xFFFF] * 68))
    struct.pack_into("<H", body, 0x352 + 11 * 2, 0)
    encrypted = bytes(byte ^ KEY[index % len(KEY)] for index, byte in enumerate(body)) + FOOTER + b"\n"
    result = inspect(decode(encrypted))
    assert (result["clear_count"], result["unknown_0004"], result["scenario_progress"]) == (3, 1, 287)
    assert result["scenario_phase"] == "ENDING/RESULT"
    assert [scenario_phase(value) for value in (0, 50, 51, 100, 101, 162, 163, 222, 223, 261, 262, 291, 292)] == [
        "ACT 1", "ACT 1", "ACT 2", "ACT 2", "ACT 3", "ACT 3", "ACT 4",
        "ACT 4", "ACT 5", "ACT 5", "ENDING/RESULT", "ENDING/RESULT", "UNKNOWN"]
    assert result["difficulty"] == "THE BOSS EXTREME"
    assert result["stage"] == "s05a20l"
    assert result["play_time_seconds"] == 60
    assert result["bandana_used"] and result["stealth_camouflage_used"]
    assert result["knife_defeats"] == 50
    assert result["pickups"] == 400
    assert result["box_drum_time_ticks"] == 216000
    assert result["drebin_points"] == result["unknown_01c4"] == 123456789
    assert result["flashbacks_viewed"] == 273
    assert result["counted_weapon_types"] == 2
    assert result["little_gray_weapon_progress"] == 2
    assert result["weapon_11_state"] == 0
    assert result["base_weapon_slots_owned"] == 1


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("save", nargs="?")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        self_test()
        return
    if not args.save:
        parser.error("save is required")
    with open(args.save, "rb") as stream:
        print(json.dumps(inspect(decode(stream.read())), indent=2))


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError) as error:
        sys.exit(str(error))

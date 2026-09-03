#!/usr/bin/env python3
"""Minimal read-only Peace Walker probe for FOXHOUND tracker work.

Resolves the same globals as mgspw-snake-swiss-v3.CT (RedCode) via AOB scan
of executable pages, then prints live mission-timer raw values in full plus
save-block context. Never writes game memory.

Targets live mission time first (same-second best-time override implies a
hidden sub-second field). Weapon-category / tranq-kill mapping is still
UNVERIFIED and reported as candidates only.

Usage:
  python3 scripts/probe-mgspw-memory.py [--pid PID] [--dump-text OUT]
  python3 scripts/probe-mgspw-memory.py --watch [--interval 1.0]
  python3 scripts/probe-mgspw-memory.py --self-test
"""

import argparse
import json
import pathlib
import struct
import sys
import time

EXE_NAME = "METAL GEAR SOLID PEACE WALKER.exe"
TEXT_RVA = 0x1000
TEXT_SIZE = 0x98B65C  # per docs/mgspw_research.md .text virtual size


def parse_pattern(text):
    out = []
    for tok in text.split():
        out.append(None if tok in ("??", "?", "*") else int(tok, 16))
    return bytes(b if b is not None else 0 for b in out), bytes(
        1 if b is not None else 0 for b in out)


PATTERNS = {
    # name: (pattern, disp_offset)
    "PW_SAVEROOT": ("48 8B 05 ?? ?? ?? ?? 48 05 3C BD 00 00 C3", 3),
    "PW_CHARARRAY": ("53 48 83 EC 20 48 8B 05 ?? ?? ?? ?? 48 63 D1 48 8B 0C D0", 8),
    "PW_MISSIONTIME": ("48 89 05 ?? ?? ?? ?? 41 0F BA E1 19", 3),
    # mission-start init: clears the current-mission-id global to -1
    "PW_MISSIONID": ("33 DB BE FF FF FF FF B9 FF FF FF 00 48 89 1D ?? ?? ?? ??"
                     " 8B EB 89 35 ?? ?? ?? ??", 23),
}


def module_base(maps_text):
    base = None
    suffix = "/" + EXE_NAME
    for line in maps_text.splitlines():
        # NB: the exe path contains spaces, so match the raw line tail
        # instead of the last whitespace-split field.
        if not line.rstrip().endswith(suffix):
            continue
        parts = line.split()
        if len(parts) < 5:
            continue
        lo, _ = (int(x, 16) for x in parts[0].split("-", 1))
        if parts[1][:3] == "r--" and parts[2] == "00000000":
            base = lo if base is None else min(base, lo)
    if base is None:
        raise ValueError(f"{EXE_NAME} base mapping not found")
    return base


def code_range(memory, base):
    # Under Proton the decrypted code may live in anonymous mappings, so the
    # scan range comes from the module's own PE headers (like the ASI), not
    # from maps pathnames.
    dos = read_mem(memory, base, 64)
    if dos[:2] != b"MZ":
        raise ValueError("live module does not start with MZ")
    (e_lfanew,) = struct.unpack_from("<I", dos, 0x3C)
    opt = base + e_lfanew + 24
    (sizeof_code,) = struct.unpack("<I", read_mem(memory, opt + 4, 4))
    (base_of_code,) = struct.unpack("<I", read_mem(memory, opt + 20, 4))
    return [(base + base_of_code, base + base_of_code + sizeof_code)]


def find_pid():
    for process in pathlib.Path("/proc").glob("[0-9]*"):
        try:
            module_base((process / "maps").read_text())
            return int(process.name)
        except (OSError, ValueError):
            pass
    raise ValueError(f"running {EXE_NAME} process not found")


def read_mem(memory, address, size):
    memory.seek(address)
    data = memory.read(size)
    if len(data) != size:
        raise ValueError(f"short memory read at {address:#x}")
    return data


def aob_scan(memory, ranges, needle, mask):
    n = len(needle)
    first = needle[0]
    for lo, hi in ranges:
        # read in 1 MiB chunks with overlap
        chunk_size = 1 << 20
        overlap = n - 1
        offset = lo
        carry = b""
        while offset < hi:
            size = min(chunk_size, hi - offset)
            data = read_mem(memory, offset, size)
            buf = carry + data
            base_addr = offset - len(carry)
            start = 0
            while True:
                idx = buf.find(bytes((first,)), start)
                if idx < 0:
                    break
                ok = True
                for i in range(n):
                    if mask[i] and buf[idx + i] != needle[i]:
                        ok = False
                        break
                if ok:
                    addr = base_addr + idx
                    if lo <= addr < hi:
                        return addr
                start = idx + 1
                if start + n > len(buf):
                    break
            carry = buf[-(overlap):] if overlap > 0 else b""
            offset += size
    return None


def rip_target(match_addr, disp_off, memory):
    disp = struct.unpack("<i", read_mem(memory, match_addr + disp_off, 4))[0]
    return match_addr + disp_off + 4 + disp


def resolve(memory, base, ranges):
    resolved = {}
    for name, (pat_text, disp_off) in PATTERNS.items():
        needle, mask_str = parse_pattern(pat_text)
        mask = bytes(mask_str)
        match = aob_scan(memory, ranges, needle, mask)
        if match is None:
            resolved[name] = None
            continue
        resolved[name] = rip_target(match, disp_off, memory)
    return resolved


def snapshot(memory, resolved):
    out = {"resolvers": {}, "mission": {}, "save": {}, "player": {}, "foxhound": {}}
    for name, addr in resolved.items():
        out["resolvers"][name] = f"{addr:#x}" if addr is not None else None

    mt = resolved.get("PW_MISSIONTIME")
    if mt is not None:
        raw = struct.unpack("<Q", read_mem(memory, mt, 8))[0]
        aux = struct.unpack("<Q", read_mem(memory, mt + 0x08, 8))[0]
        area = struct.unpack("<I", read_mem(memory, mt + 0x10, 4))[0]
        secondary = struct.unpack("<I", read_mem(memory, mt + 0x14, 4))[0]
        total_ms = raw  # leading hypothesis: raw is ms
        out["mission"] = {
            "raw_dec": raw,
            "raw_hex": f"{raw:#x}",
            "as_ms_clock": f"{total_ms // 60000}:{(total_ms // 1000) % 60:02d}.{total_ms % 1000:03d}",
            "as_seconds_div1000": raw / 1000.0,
            "as_seconds_div60": raw / 60.0,
            "as_seconds_div30": raw / 30.0,
            "aux_plus08_dec": aux,
            "aux_plus08_hex": f"{aux:#x}",
            "area_plus10_u32": area,
            "secondary_u32": secondary,
            "secondary_hex": f"{secondary:#x}",
        }
    else:
        out["mission"] = {"error": "PW_MISSIONTIME unresolved"}

    sr = resolved.get("PW_SAVEROOT")
    if sr is not None:
        block = struct.unpack("<Q", read_mem(memory, sr, 8))[0]
        out["save"]["block"] = f"{block:#x}" if block else None
        if block:
            stage = read_mem(memory, block + 0x54, 24).split(b"\0", 1)[0]
            total = struct.unpack("<I", read_mem(memory, block + 0x84, 4))[0]
            stage_play = struct.unpack("<I", read_mem(memory, block + 0x88, 4))[0]
            out["save"].update(
                {
                    "stage": stage.decode("ascii", errors="replace"),
                    "total_play_u32": total,
                    "stage_play_u32": stage_play,
                    "heroism": struct.unpack("<i", read_mem(memory, block + 0x64F4, 4))[0],
                    "heroism_delta": struct.unpack("<i", read_mem(memory, block + 0x64EC, 4))[0],
                    "gmp": struct.unpack("<I", read_mem(memory, block + 0xB52C, 4))[0],
                    "last_best_a": struct.unpack("<I", read_mem(memory, block + 0x586C, 4))[0],
                    "last_best_b": struct.unpack("<I", read_mem(memory, block + 0x5874, 4))[0],
                    "clears": struct.unpack("<i", read_mem(memory, block + 0x656C, 4))[0],
                    "s_count": struct.unpack("<i", read_mem(memory, block + 0x9084, 4))[0],
                    "fulton": struct.unpack("<i", read_mem(memory, block + 0x130, 4))[0],
                    # achievement-id-10 counter (see docs: Headshot Hero candidate)
                    "ach10_ctr": struct.unpack("<I", read_mem(memory, block + 0xB4EC, 4))[0],
                    "ach11_ctr_u16": struct.unpack("<H", read_mem(memory, block + 0x22, 2))[0],
                }
            )
            # weapon-record usage candidates [rec+0x14], stride 0x1C
            uses = []
            for i in range(16):
                rec = block + 0xBD3C + i * 0x1C
                try:
                    uses.append(struct.unpack("<H", read_mem(memory, rec + 0x14, 2))[0])
                except ValueError:
                    uses.append(None)
            out["foxhound"]["weapon_use_w00_w15"] = uses
            # Per-mission results, indexed by mission id (see docs):
            # rank u16 at +0x32B4 (0 = S, 0xFFFF = never cleared),
            # co-op/second rank u16 at +0x46F4, best time u32 (300 Hz)
            # at +0x29B4 (0xFFFFFFFF = none). Ids past the live list read as zeros,
            # so only entries with a rank or a time are reported.
            missions = {}
            n = 272
            ranks = struct.unpack("<%dH" % n, read_mem(memory, block + 0x32B4, n * 2))
            ranks2 = struct.unpack("<%dH" % n, read_mem(memory, block + 0x46F4, n * 2))
            times = struct.unpack("<%dI" % n, read_mem(memory, block + 0x29B4, n * 4))
            for i in range(n):
                best = times[i] if times[i] not in (0, 0xFFFFFFFF) else 0
                if ranks[i] == 0xFFFF and ranks2[i] == 0xFFFF and not best:
                    continue
                missions[i] = {
                    "rank": None if ranks[i] == 0xFFFF else ranks[i],
                    "rank2": None if ranks2[i] == 0xFFFF else ranks2[i],
                    "best_ticks": best or None,
                    "best_seconds": round(best / 300.0, 2) if best else None,
                }
            out["foxhound"]["missions"] = missions
            # Per-mission tallies: descriptor +0x18 (ticks live during the
            # mission; the career value at +0x20 settles at results).
            tally = {}
            for iid, recs in idmap(memory, block).items():
                for _off, x, _v in recs:
                    if 0 < x < 10000:
                        tally[f"{iid:#x}"] = max(x, tally.get(f"{iid:#x}", 0))
            out["foxhound"]["mission_tally"] = tally
            out["foxhound"]["headshots_by_category"] = "UNVERIFIED"
            out["foxhound"]["tranq_kill"] = "UNVERIFIED (sleep/stun/incap cat scan pending)"
    else:
        out["save"] = {"error": "PW_SAVEROOT unresolved"}

    mid = resolved.get("PW_MISSIONID")
    if mid is not None:
        out["mission"]["current_id"] = struct.unpack(
            "<i", read_mem(memory, mid, 4))[0]

    ca = resolved.get("PW_CHARARRAY")
    if ca is not None:
        arr = struct.unpack("<Q", read_mem(memory, ca, 8))[0]
        out["player"]["array"] = f"{arr:#x}" if arr else None
        if arr:
            player = struct.unpack("<Q", read_mem(memory, arr, 8))[0]
            if player:
                hp = struct.unpack("<h", read_mem(memory, player + 0x8A0, 2))[0]
                weapon = struct.unpack("<h", read_mem(memory, player + 0x14B8, 2))[0]
                out["player"].update({"hp": hp, "weapon_id": weapon})
    return out


def rate_report(first, second, dt):
    # Compare numeric timer fields across two snapshots; mirrors may show
    # identical deltas (e.g. mt area vs save stage).
    def num(snap, *path):
        node = snap
        for key in path:
            if not isinstance(node, dict) or key not in node:
                return None
            node = node[key]
        return node if isinstance(node, (int, float)) else None

    fields = [
        ("mission.raw", ("mission", "raw_dec")),
        ("mission.aux_plus08", ("mission", "aux_plus08_dec")),
        ("mission.area_plus10", ("mission", "area_plus10_u32")),
        ("mission.secondary", ("mission", "secondary_u32")),
        ("save.total", ("save", "total_play_u32")),
        ("save.stage", ("save", "stage_play_u32")),
    ]
    report = {}
    for label, path in fields:
        a, b = num(first, *path), num(second, *path)
        if a is None or b is None:
            report[label] = "n/a"
        else:
            report[label] = {"before": a, "after": b, "delta": b - a,
                             "per_second": (b - a) / dt if dt else 0}
    for key in ("stage",):
        report["save.stage_name"] = second.get("save", {}).get(key)
    return report


def flat_timers(snap):
    m = snap.get("mission", {})
    s = snap.get("save", {})
    return {
        "mt_raw": m.get("raw_dec"),
        "mt_aux": m.get("aux_plus08_dec"),
        "mt_area": m.get("area_plus10_u32"),
        "mt_sec": m.get("secondary_u32"),
        "save_total": s.get("total_play_u32"),
        "save_stage": s.get("stage_play_u32"),
        "stage": s.get("stage"),
        "heroism": s.get("heroism"),
        "gmp": s.get("gmp"),
        "last_best_a": s.get("last_best_a"),
    }


def idmap(memory, block, size=0x30000):
    # Lifetime-stat descriptors: 48-byte records framing {max,max} around
    # the id: +0x00 u32 max (999999), +0x10 u32 id, +0x18 i32 last-delta,
    # +0x20 i32 value, +0x28 u32 max. Returns id -> list of (offset, X, val).
    data = read_mem(memory, block, size)
    out = {}
    for off in range(0, size - 48, 4):
        if (struct.unpack_from("<I", data, off)[0] != 999999
                or struct.unpack_from("<I", data, off + 0x28)[0] != 999999):
            continue
        iid = struct.unpack_from("<I", data, off + 0x10)[0]
        if iid == 0:
            continue
        x = struct.unpack_from("<i", data, off + 0x18)[0]
        v = struct.unpack_from("<i", data, off + 0x20)[0]
        out.setdefault(iid, []).append((block + off, x, v))
    return out


def self_test():
    needle, mask = parse_pattern("48 8B 05 ?? ?? ?? ?? 48 05 3C BD 00 00 C3")
    assert len(needle) == 14 and mask[3] == 0 and mask[0] == 1
    # rip resolve check: match at 0x1000, disp at +3
    import io

    disp = 0x1234
    match = 0x1000
    fake = bytearray(0x3000)
    struct.pack_into("<i", fake, match + 3 - 0x0, disp)
    memory = io.BytesIO(bytes(fake))
    got = match + 3 + 4 + disp
    assert got == 0x1000 + 3 + 4 + 0x1234
    # ms clock format check
    raw = 90123
    assert f"{raw // 60000}:{(raw // 1000) % 60:02d}.{raw % 1000:03d}" == "1:30.123"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--pid", type=int)
    parser.add_argument("--dump-text")
    parser.add_argument("--watch", action="store_true")
    parser.add_argument("--interval", type=float, default=1.0)
    parser.add_argument("--rate", type=float, default=0.0, metavar="SECONDS",
                        help="snapshot twice this far apart and report per-field rates")
    parser.add_argument("--idmap", action="store_true",
                        help="dump descriptor id -> (offset, last-delta, value)")
    parser.add_argument("--id", type=lambda v: int(v, 0), action="append", default=[],
                        help="with --idmap, only show these ids (repeatable)")
    parser.add_argument("--trace", type=float, default=0.0, metavar="SECONDS",
                        help="log a JSON line whenever any timer changes, for this long")
    parser.add_argument("--trace-interval", type=float, default=0.5)
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        self_test()
        print("self-test ok")
        return

    pid = args.pid or find_pid()
    maps = pathlib.Path(f"/proc/{pid}/maps").read_text()
    base = module_base(maps)
    with open(f"/proc/{pid}/mem", "rb", buffering=0) as memory:
        if args.dump_text:
            pathlib.Path(args.dump_text).write_bytes(read_mem(memory, base + TEXT_RVA, TEXT_SIZE))
        resolved = resolve(memory, base, code_range(memory, base))

        if args.idmap:
            sr = resolved.get("PW_SAVEROOT")
            if sr is None:
                sys.exit("PW_SAVEROOT unresolved")
            block = struct.unpack("<Q", read_mem(memory, sr, 8))[0]
            table = idmap(memory, block)
            if args.id:
                table = {k: v for k, v in table.items() if k in args.id}
            print(json.dumps(
                {"pid": pid, "block": f"{block:#x}",
                 "stats": {f"{k:#x}": [{"off": f"{o:#x}", "x": x, "v": v}
                                       for o, x, v in vlist]
                           for k, vlist in sorted(table.items())}},
                indent=2))
            return

        def emit():
            snap = snapshot(memory, resolved)
            snap["pid"] = pid  # type: ignore[typeddict-item]
            snap["base"] = f"{base:#x}"  # type: ignore[typeddict-item]
            print(json.dumps(snap, indent=2), flush=True)

        if args.rate > 0:
            first = snapshot(memory, resolved)
            time.sleep(args.rate)
            second = snapshot(memory, resolved)
            print(json.dumps({"pid": pid, "base": f"{base:#x}",
                              "dt_requested": args.rate,
                              "rates": rate_report(first, second, args.rate)},
                             indent=2))
            return

        if args.trace > 0:
            deadline = time.time() + args.trace
            last = None
            while time.time() < deadline:
                try:
                    cur = flat_timers(snapshot(memory, resolved))
                except ValueError:
                    break  # game went away
                if cur != last:
                    print(json.dumps({"t": round(time.time(), 2), **cur}),
                          flush=True)
                    last = cur
                time.sleep(args.trace_interval)
            return

        emit()
        while args.watch:
            time.sleep(args.interval)
            emit()


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError) as error:
        sys.exit(str(error))

#!/usr/bin/env python3
"""Differential counter finder for Peace Walker (Proton, read-only).

Cheat-Engine style workflow without Cheat Engine:
  1. snap: record every address whose u32 (and u16) value is inside a window
  2. user performs exactly N of the action in-game
  3. diff: report addresses whose value grew by exactly N

Skips file-backed code/library mappings (counters live in anonymous
heap); the exe path contains spaces, so maps lines match on the tail.

Usage:
  python3 scripts/find-mgspw-counter.py snap --lo 40 --hi 80 --out /tmp/hs1.json [--pid PID]
  ... perform exactly N headshots ...
  python3 scripts/find-mgspw-counter.py diff --old /tmp/hs1.json --delta N [--pid PID]
"""

import argparse
import json
import pathlib
import struct
import sys

EXE_NAME = "METAL GEAR SOLID PEACE WALKER.exe"
CHUNK = 1 << 20


def find_pid():
    for process in pathlib.Path("/proc").glob("[0-9]*"):
        try:
            text = (process / "maps").read_text()
        except OSError:
            continue
        if EXE_NAME in text:
            return int(process.name)
    raise ValueError(f"running {EXE_NAME} process not found")


def readable_regions(pid):
    regions = []
    for line in pathlib.Path(f"/proc/{pid}/maps").read_text().splitlines():
        parts = line.split()
        if len(parts) < 5 or not parts[1].startswith("r"):
            continue
        tail = line[line.index(parts[3]):]
        # skip file-backed code and shared libraries; keep anon heap/stack
        if ".exe" in tail or ".dll" in tail or ".so" in tail:
            continue
        lo, hi = (int(x, 16) for x in parts[0].split("-", 1))
        if hi - lo > 0:
            regions.append((lo, hi))
    return regions


def read_mem(memory, address, size):
    memory.seek(address)
    data = memory.read(size)
    if len(data) != size:
        raise ValueError(f"short memory read at {address:#x}")
    return data


def cmd_snap(args):
    pid = args.pid or find_pid()
    regions = readable_regions(pid)
    total = sum(hi - lo for lo, hi in regions)
    print(f"pid {pid}, {len(regions)} regions, {total / 1e9:.2f} GB to scan",
          file=sys.stderr, flush=True)
    hits = {}
    skipped = 0
    with open(f"/proc/{pid}/mem", "rb", buffering=0) as memory:
        for index, (lo, hi) in enumerate(regions):
            offset = lo
            while offset < hi:
                try:
                    data = read_mem(memory, offset, min(CHUNK, hi - offset))
                except (OSError, ValueError):
                    skipped += 1
                    break  # unreadable page in region; skip rest of it
                for i in range(0, len(data) - 3, 4):
                    v = struct.unpack_from("<I", data, i)[0]
                    if args.lo <= v <= args.hi:
                        hits[f"{offset + i:#x}"] = v
                offset += len(data)
            if index % 100 == 0:
                print(f"  ...{index}/{len(regions)} regions", file=sys.stderr)
    print(f"skipped {skipped} unreadable regions", file=sys.stderr)
    with open(args.out, "w") as stream:
        json.dump({"pid": pid, "lo": args.lo, "hi": args.hi, "hits": hits}, stream)
    print(f"{len(hits)} addresses in [{args.lo},{args.hi}] -> {args.out}")


def cmd_diff(args):
    pid = args.pid or find_pid()
    old = json.load(open(args.old))["hits"]
    matched = []
    with open(f"/proc/{pid}/mem", "rb", buffering=0) as memory:
        for addr_text, old_value in old.items():
            addr = int(addr_text, 16)
            try:
                value = struct.unpack("<I", read_mem(memory, addr, 4))[0]
            except (OSError, ValueError):
                continue
            if value - old_value == args.delta:
                matched.append((addr_text, old_value, value))
    print(f"{len(matched)} addresses grew by exactly {args.delta}:")
    for addr_text, old_value, value in matched:
        print(f"  {addr_text}: {old_value} -> {value}")
    if matched:
        with open(args.old + ".matched.json", "w") as stream:
            json.dump(matched, stream)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(required=True)
    snap = sub.add_parser("snap")
    snap.add_argument("--lo", type=int, required=True)
    snap.add_argument("--hi", type=int, required=True)
    snap.add_argument("--out", required=True)
    snap.add_argument("--pid", type=int)
    snap.set_defaults(func=cmd_snap)
    diff = sub.add_parser("diff")
    diff.add_argument("--old", required=True)
    diff.add_argument("--delta", type=int, required=True)
    diff.add_argument("--pid", type=int)
    diff.set_defaults(func=cmd_diff)
    args = parser.parse_args()
    try:
        args.func(args)
    except (OSError, ValueError) as error:
        sys.exit(str(error))


if __name__ == "__main__":
    main()

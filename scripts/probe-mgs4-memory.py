#!/usr/bin/env python3
"""Inspect or dump MGS4's live PE image through Linux /proc."""

import argparse
import json
import pathlib
import runpy
import struct
import sys

TEXT_RVA = 0x1000
TEXT_SIZE = 0x165CF80
PROBES = (0, TEXT_RVA, 0x5C780, 0x645E0, 0x69DB0, 0x94040, 0x1844DE0)
LINKVARBUF_POINTER_RVA = 0x1C28B28
VARBUF_POINTER_RVA = 0x1C28B38


def module_base(maps):
    for line in maps.splitlines():
        fields = line.split()
        if len(fields) >= 6 and fields[2] == "00000000" and fields[-1].lower().endswith("/mgs4.exe"):
            return int(fields[0].split("-", 1)[0], 16)
    raise ValueError("mgs4.exe base mapping not found")


def find_pid():
    for process in pathlib.Path("/proc").glob("[0-9]*"):
        try:
            module_base((process / "maps").read_text())
            return int(process.name)
        except (OSError, ValueError):
            pass
    raise ValueError("running mgs4.exe process not found")


def read(memory, address, size):
    memory.seek(address)
    data = memory.read(size)
    if len(data) != size:
        raise ValueError(f"short memory read at {address:#x}")
    return data


def self_test():
    maps = "6fffd1aa0000-6fffd1aa1000 r--p 00000000 00:01 1 /game/MGS4/mgs4.exe\n"
    assert module_base(maps) == 0x6FFFD1AA0000


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--pid", type=int)
    parser.add_argument("--dump-text")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        self_test()
        return

    pid = args.pid or find_pid()
    maps = pathlib.Path(f"/proc/{pid}/maps").read_text()
    base = module_base(maps)
    with open(f"/proc/{pid}/mem", "rb", buffering=0) as memory:
        probes = {f"{rva:#x}": read(memory, base + rva, 16).hex() for rva in PROBES}
        if bytes.fromhex(probes["0x0"])[:2] != b"MZ":
            raise ValueError("live module does not start with MZ")
        linkvarbuf = struct.unpack("<Q", read(memory, base + LINKVARBUF_POINTER_RVA, 8))[0]
        varbuf = struct.unpack("<Q", read(memory, base + VARBUF_POINTER_RVA, 8))[0]
        save_tool = runpy.run_path(pathlib.Path(__file__).with_name("inspect-mgs4-save.py"))
        stats = save_tool["inspect"](read(memory, linkvarbuf, save_tool["BODY_SIZE"]))
        stats["eastern_europe_time_ticks"] = struct.unpack("<I", read(memory, varbuf + 0xDB4, 4))[0]
        if args.dump_text:
            pathlib.Path(args.dump_text).write_bytes(read(memory, base + TEXT_RVA, TEXT_SIZE))

    print(json.dumps({"pid": pid, "base": f"{base:#x}", "varbuf": f"{varbuf:#x}",
                      "linkvarbuf": f"{linkvarbuf:#x}", "stats": stats, "probes": probes}, indent=2))


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError) as error:
        sys.exit(str(error))

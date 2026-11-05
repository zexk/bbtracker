#!/usr/bin/env python3
"""Inspect or dump MGS4's live PE image through Linux /proc."""

import argparse
import json
import pathlib
import siblings
import struct
import sys

TEXT_RVA = 0x1000
PROBES = (0, TEXT_RVA, 0x5C780, 0x645E0, 0x69DB0, 0x94040, 0x1844DE0)
LINKVARBUF_POINTER_RVA = 0x1C28B28
VARBUF_POINTER_RVA = 0x1C28B38
DREBIN_POINTS_OFFSET = 0x1C0


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


def text_section(memory, base):
    headers = read(memory, base, 0x1000)
    e_lfanew = struct.unpack_from("<I", headers, 0x3C)[0]
    pe = headers[e_lfanew:]
    if pe[:4] != b"PE\0\0":
        raise ValueError("live module is not a PE image")
    count = struct.unpack_from("<H", pe, 6)[0]
    opt_size = struct.unpack_from("<H", pe, 20)[0]
    offset = 24 + opt_size
    for index in range(count):
        section = pe[offset + index * 40 : offset + (index + 1) * 40]
        if section[:8].rstrip(b"\0") == b".text":
            virtual_size, virtual_address = struct.unpack_from("<II", section, 8)
            return virtual_address, virtual_size
    raise ValueError(".text section not found")


def write_drebin_points(memory, linkvarbuf, points):
    if not 0 <= points <= 0xFFFFFFFF:
        raise ValueError("Drebin points must fit uint32")
    data = struct.pack("<I", points)
    memory.seek(linkvarbuf + DREBIN_POINTS_OFFSET)
    if memory.write(data) != len(data):
        raise ValueError(f"short memory write at {linkvarbuf + DREBIN_POINTS_OFFSET:#x}")


def self_test():
    maps = "6fffd1aa0000-6fffd1aa1000 r--p 00000000 00:01 1 /game/MGS4/mgs4.exe\n"
    assert module_base(maps) == 0x6FFFD1AA0000

    import io
    memory = io.BytesIO(bytes(0x200))
    write_drebin_points(memory, 0, 123456789)
    assert struct.unpack_from("<I", memory.getvalue(), DREBIN_POINTS_OFFSET)[0] == 123456789


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--pid", type=int)
    parser.add_argument("--dump-text")
    parser.add_argument("--set-drebin", type=lambda value: int(value, 0), metavar="POINTS")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        self_test()
        return

    pid = args.pid or find_pid()
    maps = pathlib.Path(f"/proc/{pid}/maps").read_text()
    base = module_base(maps)
    mode = "r+b" if args.set_drebin is not None else "rb"
    with open(f"/proc/{pid}/mem", mode, buffering=0) as memory:
        probes = {f"{rva:#x}": read(memory, base + rva, 16).hex() for rva in PROBES}
        if bytes.fromhex(probes["0x0"])[:2] != b"MZ":
            raise ValueError("live module does not start with MZ")
        text_rva, text_size = text_section(memory, base)
        if args.dump_text:
            pathlib.Path(args.dump_text).write_bytes(read(memory, base + text_rva, text_size))
        linkvarbuf = struct.unpack("<Q", read(memory, base + LINKVARBUF_POINTER_RVA, 8))[0]
        varbuf = struct.unpack("<Q", read(memory, base + VARBUF_POINTER_RVA, 8))[0]
        if args.set_drebin is not None:
            write_drebin_points(memory, linkvarbuf, args.set_drebin)
        save_tool = siblings.load("inspect-mgs4-save.py")
        stats = save_tool.inspect(read(memory, linkvarbuf, save_tool.BODY_SIZE))
        stats["eastern_europe_time_ticks"] = struct.unpack("<I", read(memory, varbuf + 0xDB4, 4))[0]

    print(json.dumps({"pid": pid, "base": f"{base:#x}",
                      "text": {"rva": f"{text_rva:#x}", "size": text_size},
                      "varbuf": f"{varbuf:#x}", "linkvarbuf": f"{linkvarbuf:#x}",
                      "stats": stats, "probes": probes}, indent=2))


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError) as error:
        sys.exit(str(error))

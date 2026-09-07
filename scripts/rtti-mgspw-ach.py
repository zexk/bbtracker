#!/usr/bin/env python3
"""Enumerate MSVC x64 RTTI for MGK achievement classes (read-only).

Prints TypeDescriptor RVAs, Complete Object Locators, vtable addresses
and vtable method targets. Code targets live in encrypted on-disk .text;
resolve them against a live dump (probe-mgspw-memory.py --dump-text)
for capstone disassembly.
"""
import struct

EXE = ("/home/zexk/.local/share/Steam/steamapps/common/MGS_PW/mgspw/"
       "METAL GEAR SOLID PEACE WALKER.exe")
BASE = 0x140000000


def main():
    data = open(EXE, "rb").read()
    assert data[:2] == b"MZ"
    e_lfanew = struct.unpack("<I", data[0x3C:0x40])[0]
    nsec = struct.unpack("<H", data[e_lfanew + 6:e_lfanew + 8])[0]
    optsz = struct.unpack("<H", data[e_lfanew + 20:e_lfanew + 22])[0]
    sec_off = e_lfanew + 24 + optsz
    secs = []
    for i in range(nsec):
        s = data[sec_off + i * 40:sec_off + (i + 1) * 40]
        name = s[:8].rstrip(b"\0").decode()
        vsz, va, rawsz, rawptr = struct.unpack("<IIII", s[8:24])
        secs.append((name, va, vsz, rawptr, rawsz))

    def rva2off(r):
        for _, va, vsz, rawptr, rawsz in secs:
            if va <= r < va + max(vsz, rawsz):
                return rawptr + (r - va)
        return None

    def off2rva(o):
        for _, va, _, rawptr, rawsz in secs:
            if rawptr <= o < rawptr + rawsz:
                return va + (o - rawptr)
        return None

    rdata = [(n, va, vsz, ro, rs) for n, va, vsz, ro, rs in secs
             if "rdata" in n.lower()]

    for tname in (b".?AVMGK_SteamAchievement@@",
                  b".?AVMGK_IAchievementSystem@@"):
        idx = data.find(tname)
        if idx < 0:
            print("missing", tname.decode())
            continue
        td_rva = off2rva(idx - 16)  # TypeDescriptor starts 16B before name
        print(f"{tname.decode()} TypeDescriptor RVA={td_rva:#x}")
        # COL.pTypeDescriptor is an RVA on x64; COL base = match - 12
        needle = struct.pack("<I", td_rva)
        for secname, va, vsz, ro, rs in rdata:
            blob = data[ro:ro + rs]
            start = 0
            while True:
                j = blob.find(needle, start)
                if j < 0:
                    break
                col_off = ro + j - 12
                sig, offb, cdo = struct.unpack("<III", data[col_off:col_off + 12])
                if sig == 1:
                    col_rva = off2rva(col_off)
                    print(f"  COL fileoff={col_off:#x} RVA={col_rva:#x} "
                          f"objoff={offb} cd={cdo} [{secname}]")
                    # vtable[0] points at the COL
                    vptr = struct.pack("<Q", BASE + col_rva)
                    for s2, va2, vsz2, ro2, rs2 in rdata:
                        b2 = data[ro2:ro2 + rs2]
                        k = b2.find(vptr)
                        while k >= 0:
                            vt_off = ro2 + k
                            vt_rva = off2rva(vt_off)
                            print(f"    vtable fileoff={vt_off:#x} "
                                  f"RVA={vt_rva:#x} [{s2}]")
                            methods = struct.unpack(
                                "<16Q", data[vt_off:vt_off + 128])
                            for mi, m in enumerate(methods):
                                if m == 0:
                                    break
                                print(f"      [{mi}] {m:#x} "
                                      f"(rva {m - BASE:#x})")
                            k = b2.find(vptr, k + 1)
                start = j + 1


if __name__ == "__main__":
    main()

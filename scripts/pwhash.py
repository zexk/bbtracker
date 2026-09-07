#!/usr/bin/env python3
"""PW save/logic variable-name hash reversal.

The game resolves named variables through utils.GetHash (RVA 0x98e8? live
0x14011f8d0): eax = accel rol24(h,5)+c, empty -> 1. Baked constants in the
executable (e.g. edx args to the variable lookups at 0x14010abf0) are
precomputed hashes of variable names that live in data files, not in the
exe's own string table. This script harvests candidate names from the whole
MGS_PW install and matches them against constants embedded in the runtime
.text dump.

Usage:
  python3 scripts/pwhash.py --hash noAlert
  python3 scripts/pwhash.py --resolve 0x2b7d05 0x17470 ...
  python3 scripts/pwhash.py --text /tmp/pw_text.bin --map > varmap.json
  python3 scripts/pwhash.py --self-test
"""
import argparse
import json
import pathlib
import re
import struct
import sys

EXE_DIR = pathlib.Path("/home/zexk/.local/share/Steam/steamapps/common/MGS_PW")
EXE = EXE_DIR / "mgspw" / "METAL GEAR SOLID PEACE WALKER.exe"
BASE = 0x140000000
TEXT_RVA = 0x1000

STR_RE = re.compile(rb"[ -~]{2,72}")
# Variable names are identifier-ish: letters, digits, '_' '.', not space/punct.
# A 24-bit hash over arbitrary binary substrings is pure collision noise.
NAME_RE = re.compile(r"^[A-Za-z_][A-Za-z0-9_.]{1,63}$")


def pwhash(name):
    a = 0
    for b in name.encode():
        a = ((a >> 19) | (a << 5)) + b
        a &= 0xFFFFFF
    return a or 1


def harvest():
    """All printable strings from name-bearing files under the install root.

    Skips DLC textures/audio (.PDT, DLCBGM, DLCVOICE) and video (.xmx etc.
    oversized binaries) by default; variable names live in the exe, MLG
    Text, layouts and small data files.
    """
    seen = {}
    skip_ext = {".pdt", ".xmx"}
    for f in EXE_DIR.rglob("*"):
        if not f.is_file():
            continue
        rel = str(f.relative_to(EXE_DIR))
        if f.suffix.lower() in skip_ext or "DLCBGM" in rel or "DLCVOICE" in rel:
            continue
        try:
            if f.stat().st_size > 64 * 1024 * 1024:
                continue
            data = f.read_bytes()
        except OSError:
            continue
        for m in STR_RE.finditer(data):
            s = m.group().decode()
            if NAME_RE.match(s):
                seen.setdefault(s, rel)
    return seen


def parse_sections(data):
    e_lfanew = struct.unpack_from("<I", data, 0x3C)[0]
    nsec = struct.unpack_from("<H", data, e_lfanew + 6)[0]
    optsz = struct.unpack_from("<H", data, e_lfanew + 20)[0]
    so = e_lfanew + 24 + optsz
    secs = {}
    for i in range(nsec):
        s = data[so + i * 40:so + (i + 1) * 40]
        name = s[:8].rstrip(b"\0").decode()
        vsz, va, rsz, rp = struct.unpack_from("<IIII", s, 8)
        secs[name] = {"va": va, "vsz": vsz, "raw": rp, "rawsz": rsz}
    return secs


def text_dwords(text_path, lo=0, hi=0x1000000):
    """Distinct little-endian dword constants in the dump, in hash range."""
    blob = pathlib.Path(text_path).read_bytes()
    out = set()
    for (dw,) in struct.iter_unpack("<I", blob[: len(blob) - (len(blob) % 4)]):
        if lo < dw < hi:
            out.add(dw)
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--hash", help="print hash of NAME")
    ap.add_argument("--resolve", nargs="*", help="resolve hex constants")
    ap.add_argument("--map", action="store_true",
                    help="hash->name map for constants found in --text")
    ap.add_argument("--text", help="runtime .text dump to scan for constants")
    ap.add_argument("--self-test", action="store_true")
    args = ap.parse_args()

    if args.self_test:
        # Rol24-by-5: hash('')==1; monotonic on suffix length; 24-bit bound.
        assert pwhash("") == 1
        assert pwhash("noAlert") < 0x1000000
        assert pwhash("a") != pwhash("b")
        print("self-test ok")
        return

    if args.hash:
        print(f"{pwhash(args.hash):#08x}  {args.hash!r}")
        return

    if args.resolve:
        names = harvest()
        byhash = {}
        for s, src in names.items():
            byhash.setdefault(pwhash(s), []).append((s, src))
        hits = 0
        for raw in args.resolve:
            c = int(raw, 0)
            found = byhash.get(c)
            if found:
                hits += 1
                for s, src in found:
                    print(f"{c:#08x} = {s!r}   ({src})")
            else:
                print(f"{c:#08x} = ?")
        print(f"{hits}/{len(args.resolve)} resolved")
        return

    if args.map:
        if not args.text:
            sys.exit("--map needs --text DUMP")
        names = harvest()
        byhash = {}
        for s in names:
            byhash[pwhash(s)] = s
        out = {}
        for c in sorted(text_dwords(args.text)):
            if c in byhash:
                out[f"{c:#08x}"] = byhash[c]
        print(json.dumps(out, indent=2, sort_keys=True))
        return

    ap.error("pick --hash / --resolve / --map / --self-test")


if __name__ == "__main__":
    main()

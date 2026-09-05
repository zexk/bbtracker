#!/usr/bin/env python3
"""MGSPW .olang string container reader.

Peace Walker keeps its UI text in `.olang` elements inside the encrypted
archives; `scripts/pwtext.py` drives the localization tool that decrypts them
and writes each element out as a file. This script reads one of those files.

Layout, little-endian throughout:

    +0x00  "RBX\\0"
    +0x18  u32  end of the key table / start of the string index
    +0x1C  u32  end of the string index / start of the string blob
    0x20   key table, 8 bytes each: {u32 name hash, u16 first index, u16 count}
           a count above 1 is an array element, count 1 a single named string
    ...    string index, 12 bytes each: {u32 tag, u32 offset, u32 tag}
           the offset is relative to the start of the string blob
    ...    NUL-terminated UTF-8

Names are reached by hash, never by position: the game builds an asset name,
hashes it with utils.GetHash (see scripts/pwhash.py), and looks that up in the
key table. The insignia name getter `0x140544390` formats
`sig_%03d_alp_ovl_nearest` with the insignia id, which is why the insignia
rows in `slot/001FC/3` run in reverse id order.

Usage:
  python3 scripts/pwolang.py --file 003_A7FC85.olang --elements
  python3 scripts/pwolang.py --file 003_A7FC85.olang --insignias
  python3 scripts/pwolang.py --file 003_A7FC85.olang --name sig_001_alp_ovl_nearest
  python3 scripts/pwolang.py --self-test --file 003_A7FC85.olang
"""
import argparse
import importlib.util
import pathlib
import struct
import sys

HERE = pathlib.Path(__file__).resolve().parent
# Element keys in slot/001FC/3, the codename and insignia text.
CODENAME_NAMES = 0x1B1303
CODENAME_DESCS = 0x62CF73
INSIGNIA_NAMES = 0xB906B5
INSIGNIA_DESCS = 0x8CAEAE
INSIGNIA_COUNT = 110


def pwhash(name):
    spec = importlib.util.spec_from_file_location("pwhash", HERE / "pwhash.py")
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod.pwhash(name)


class Olang:
    def __init__(self, data):
        if data[:4] != b"RBX\0":
            raise ValueError("not an .olang file")
        self.data = data
        self.keys_end, self.index_end = struct.unpack_from("<2I", data, 24)
        self.keys = {}
        for at in range(32, self.keys_end, 8):
            key, first, count = struct.unpack_from("<IHH", data, at)
            self.keys[key] = (first, count)
        self.count = (self.index_end - self.keys_end) // 12

    def string(self, index):
        at = self.keys_end + 12 * index
        offset = struct.unpack_from("<I", self.data, at + 4)[0] + self.index_end
        end = self.data.index(b"\0", offset)
        return self.data[offset:end].decode("utf8", "replace")

    def element(self, key):
        """The strings of an array element, in file order."""
        first, count = self.keys[key]
        return [self.string(first + i) for i in range(count)]

    def by_name(self, name, element=None):
        """The string an asset name hashes to, optionally re-based on an element.

        The key table resolves a name to one index. Asking for an element as
        well returns the row at the same position in that element, which is
        how the game pairs a name with its description.
        """
        first, _ = self.keys[pwhash(name)]
        if element is None:
            return self.string(first)
        base, _ = self.keys[element]
        for other, (start, count) in self.keys.items():
            if count > 1 and start <= first < start + count:
                return self.string(base + (first - start))
        raise KeyError(name)


def insignia_names(olang):
    """Insignia id (1..110) -> name, through the getter's own asset names."""
    return {
        n: olang.by_name("sig_%03d_alp_ovl_nearest" % n, INSIGNIA_NAMES)
        for n in range(1, INSIGNIA_COUNT + 1)
    }


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--file", required=True, help="an extracted .olang element")
    ap.add_argument("--elements", action="store_true")
    ap.add_argument("--insignias", action="store_true")
    ap.add_argument("--name")
    ap.add_argument("--self-test", action="store_true")
    args = ap.parse_args()

    olang = Olang(pathlib.Path(args.file).read_bytes())
    if args.elements:
        for key, (first, count) in sorted(olang.keys.items()):
            if count > 1:
                print("%06x  %3d strings from %3d  %s"
                      % (key, count, first, olang.string(first)[:40]))
    if args.insignias:
        for n, name in insignia_names(olang).items():
            print("%3d  %s" % (n, name))
    if args.name:
        print(olang.by_name(args.name))
    if args.self_test:
        names = insignia_names(olang)
        # Ids 1 and 16 are the two the reference profile owns, and both are
        # solo insignias; reading the rows in file order instead makes them
        # VERSUS OPS awards on a save that has never played a match.
        assert names[1] == "Stealth Master (Rank C)", names[1]
        assert names[16] == "Headshot Master (Rank C)", names[16]
        assert names[110] == "VS Fulton Recovery Specialist (Rank A)", names[110]
        assert olang.element(CODENAME_NAMES)[13] == "FOXHOUND"
        print("self-test ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())

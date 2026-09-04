#!/usr/bin/env python3
"""MGSPW GCL bytecode reader.

The mission rank, and by extension anything the results screen computes, is
written by script rather than native code, so reading the script stream is the
only route to those formulas. This decodes the token stream the way the game's
own reader at 0x1400A4C90 does.

Token encoding, taken from that function and its dispatch tables:

  tag & 0xC0 == 0xC0   small immediate, value = (tag & 0x3F) - 1
  tag & 0xF0 == 0x00   low-nibble token, operand per the table at 0x1400A513C:
                         0        end of stream
                         1        s16   (2 bytes)
                         2,3,4    u8    (1 byte)
                         5,11,12  no operand
                         6,8      u24   (3 bytes, little-endian)
                         7        inline blob, u8 length then that many bytes
                         9,10,13  s32   (4 bytes)
                         14       u16 index into the pooled-string table
                         15       u64   (8 bytes)
  otherwise            structural token; only the high nibble dispatches
                       (0x10..0x90 via the tables at 0x1400A5198/0x1400A517C),
                       and those handlers are not fully reversed - they are
                       reported as STRUCT so a caller can see them without the
                       reader pretending to understand them.

24-bit values are worth attention: script command names and variable names are
both 24-bit hashes, so a u24 token is usually one of those.
"""
import argparse
import collections
import json
import pathlib
import struct
import sys

END = 0
NO_OPERAND = {5, 11, 12}
WIDTH = {1: 2, 2: 1, 3: 1, 4: 1, 6: 3, 8: 3, 9: 4, 10: 4, 13: 4, 14: 2, 15: 8}


class Token:
    __slots__ = ("pos", "tag", "kind", "value", "raw")

    def __init__(self, pos, tag, kind, value, raw=b""):
        self.pos, self.tag, self.kind, self.value, self.raw = pos, tag, kind, value, raw

    def __repr__(self):
        if self.kind == "blob":
            return f"{self.pos:#08x}  blob   {self.raw!r}"
        if self.value is None:
            return f"{self.pos:#08x}  {self.kind}"
        return f"{self.pos:#08x}  {self.kind:6s} {self.value:#x} ({self.value})"


def decode(data, pos=0, limit=None):
    """Yield Tokens from data[pos:]. Stops at an end token or a short read."""
    n = len(data) if limit is None else min(len(data), pos + limit)
    while pos < n:
        start = pos
        tag = data[pos]
        if tag & 0xC0 == 0xC0:
            yield Token(start, tag, "imm", (tag & 0x3F) - 1)
            pos += 1
            continue
        if tag & 0xF0:
            # Structural token; the high nibble selects a handler we have not
            # fully reversed, so consume one byte and report it as-is.
            yield Token(start, tag, "STRUCT", None)
            pos += 1
            continue
        pos += 1
        if tag == END:
            yield Token(start, tag, "end", None)
            return
        if tag in NO_OPERAND:
            yield Token(start, tag, "op", None)
            continue
        if tag == 7:
            if pos >= n:
                return
            length = data[pos]
            body = data[pos + 1:pos + 1 + length]
            yield Token(start, tag, "blob", None, body)
            pos += 1 + length
            continue
        width = WIDTH.get(tag)
        if width is None or pos + width > n:
            return
        chunk = data[pos:pos + width]
        if tag == 1:
            value = struct.unpack("<h", chunk)[0]
        elif tag in (2, 3, 4):
            value = chunk[0]
        elif tag in (6, 8):
            value = chunk[0] | chunk[1] << 8 | chunk[2] << 16
        elif tag in (9, 10, 13):
            value = struct.unpack("<i", chunk)[0]
        elif tag == 14:
            value = struct.unpack("<H", chunk)[0]
        else:
            value = struct.unpack("<Q", chunk)[0]
        kind = {6: "hash", 8: "hash", 14: "strref"}.get(tag, "int")
        yield Token(start, tag, kind, value, chunk)
        pos += width


def scan_hashes(data):
    """Every u24 token in the file, which is where command/variable names live."""
    counts = collections.Counter()
    for pos in range(len(data)):
        if data[pos] in (6, 8) and pos + 4 <= len(data):
            b = data[pos + 1:pos + 4]
            counts[b[0] | b[1] << 8 | b[2] << 16] += 1
    return counts


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("file", nargs="?")
    ap.add_argument("--offset", type=lambda v: int(v, 0), default=0)
    ap.add_argument("--limit", type=lambda v: int(v, 0), default=None)
    ap.add_argument("--hashes", action="store_true",
                    help="list the 24-bit name hashes the file references")
    ap.add_argument("--json", action="store_true")
    ap.add_argument("--self-test", action="store_true")
    args = ap.parse_args()

    if args.self_test:
        # Encodings taken straight from the handlers.
        toks = list(decode(bytes([0xC5])))
        assert toks[0].kind == "imm" and toks[0].value == 0x05 - 1, toks
        toks = list(decode(bytes([1, 0x34, 0x12])))
        assert toks[0].value == 0x1234, toks
        toks = list(decode(bytes([6, 0x41, 0xDF, 0x26])))
        assert toks[0].kind == "hash" and toks[0].value == 0x26DF41, toks
        toks = list(decode(bytes([9, 0xFF, 0xFF, 0xFF, 0xFF])))
        assert toks[0].value == -1, toks
        toks = list(decode(bytes([7, 3]) + b"abc"))
        assert toks[0].kind == "blob" and toks[0].raw == b"abc", toks
        assert list(decode(bytes([0])))[0].kind == "end"
        print("self-test ok: immediate, s16, u24 hash, s32, blob, end")
        return

    if not args.file:
        ap.error("need a file (or --self-test)")
    data = pathlib.Path(args.file).read_bytes()

    if args.hashes:
        counts = scan_hashes(data)
        rows = [{"hash": f"{h:#08x}", "count": n} for h, n in counts.most_common()]
        if args.json:
            json.dump(rows, sys.stdout, indent=1)
            print()
        else:
            print(f"{len(counts)} distinct u24 name hashes in {args.file}")
            for row in rows[:60]:
                print(f"  {row['hash']}  x{row['count']}")
        return

    for tok in decode(data, args.offset, args.limit):
        print(tok)


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""MGSPW insignia requirement table.

There are 110 insignias. Ownership is a byte per insignia at
`save+0x1C009 + index` (index 1..110, bit 0 owned, bit 1 seen). The evaluator
`0x1401E9BB0` reads a career counter, compares it against a threshold, and
calls the granter `0x140544B80` when `value > threshold` - which is why the
descriptions read "over $1" and why exactly 100 headshots does not fire.

Thresholds are data, not code: `0x140543810(index, field)` builds a 110x3 dword
table on the stack from .rdata constants and returns `arr[(index-1)*3 + field]`,
where field 0 is the threshold and field 1 the heroism award. This script
reconstructs that table by emulating the function's stores, then walks the
evaluator to pair each insignia with the stat id it tests.

Needs only the on-disk exe plus a runtime .text dump:
  python3 scripts/probe-mgspw-memory.py --dump-text /tmp/pw_text.bin
  python3 scripts/pwinsig.py --text /tmp/pw_text.bin
  python3 scripts/pwinsig.py --text /tmp/pw_text.bin --json
"""
import argparse
import collections
import siblings
import json
import pathlib
import struct
import sys

HERE = pathlib.Path(__file__).resolve().parent
THRESHOLD_FN = (0x140543810, 0x140543D70)
EVALUATOR = 0x1401E9BB0
GRANT = 0x140544B80
GETTERS = {0x1400E3A90, 0x1400E3950, 0x1400E3B60}
COUNT = 110
RBP_BIAS = 0x100  # rbp = rsp + 0x100 once the prologue has run


def load_pwdis():
    return siblings.load("pwdis.py")


def section_bytes(m, secs, exe, va, n):
    """Bytes at a VA if it lands in an initialised section, else None."""
    for name in (".rdata", ".data"):
        s = secs[name]
        rva = va - m.BASE
        if s["va"] <= rva < s["va"] + s["vsz"]:
            off = s["raw"] + (rva - s["va"])
            return exe[off:off + n]
    return None


def threshold_table(m, secs, exe, text):
    """Emulate 0x140543810's stack stores and return arr[(index-1)*3 + field]."""
    insns = list(m.disassemble(text, THRESHOLD_FN[0] - m.BASE,
                               THRESHOLD_FN[1] - m.BASE, detail=True))
    buf = bytearray(0x600)
    xmm = {}
    for insn in insns:
        target = m.rip_mem_target(insn)
        ops = insn.operands
        dest_is_rbp = "rbp" in insn.op_str.split(",")[0]
        if insn.mnemonic == "movdqa" and len(ops) == 2:
            dst, src = ops
            if dst.type == 1 and src.type == 3 and target is not None:
                data = section_bytes(m, secs, exe, target, 16)
                if data:
                    xmm[insn.reg_name(dst.reg)] = data
            elif dst.type == 3 and src.type == 1:
                name = insn.reg_name(src.reg)
                if name in xmm and dst.mem.base != 41:
                    at = dst.mem.disp + (RBP_BIAS if dest_is_rbp else 0)
                    if 0 <= at <= len(buf) - 16:
                        buf[at:at + 16] = xmm[name]
        elif (insn.mnemonic == "mov" and len(ops) == 2
              and ops[0].type == 3 and ops[1].type == 2):
            at = ops[0].mem.disp + (RBP_BIAS if dest_is_rbp else 0)
            size = ops[0].size
            if 0 <= at <= len(buf) - size:
                buf[at:at + size] = (ops[1].imm & ((1 << (8 * size)) - 1)).to_bytes(
                    size, "little")

    def lookup(index, field):
        at = (index * 3 + field) * 4 - 0xC
        return struct.unpack_from("<i", buf, at)[0]

    return lookup


def evaluator_stats(m, secs, exe, text):
    """index -> stat id, by walking the evaluator's grant call sites."""
    funcs = m.pdata_functions(exe, secs)
    span = next(f for f in funcs if m.BASE + f[0] == EVALUATOR)
    out, pending, current = {}, None, None
    for insn in m.disassemble(text, span[0], span[1], detail=True):
        if insn.mnemonic in ("mov", "lea") and insn.op_str.startswith("ecx,"):
            imm = [o.imm for o in insn.operands if o.type == 2] or \
                  [o.mem.disp for o in insn.operands if o.type == 3]
            if imm:
                pending = imm[0]
        elif insn.mnemonic == "call":
            target = m.insn_imm(insn)
            if target in GETTERS:
                current = pending
            elif target == GRANT and pending is not None:
                out[pending] = current
    return out


def build(args):
    m = load_pwdis()
    exe = open(args.exe, "rb").read()
    secs = m.parse_sections(exe)
    text = m.load_text(args.text)
    lookup = threshold_table(m, secs, exe, text)
    stats = evaluator_stats(m, secs, exe, text)
    rows = {}
    for index in range(1, COUNT + 1):
        rows[index] = {
            "stat": stats.get(index),
            "need_over": lookup(index, 0),
            "heroism": lookup(index, 1),
        }
    return rows


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--exe", default=None)
    ap.add_argument("--text", default="/tmp/pw_text.bin",
                    help="runtime .text dump (probe-mgspw-memory.py --dump-text)")
    ap.add_argument("--json", action="store_true", help="emit JSON")
    ap.add_argument("--self-test", action="store_true")
    args = ap.parse_args()
    if args.exe is None:
        args.exe = load_pwdis().EXE

    rows = build(args)

    if args.self_test:
        # Known-good anchors, each confirmed against a live profile.
        assert rows[1]["need_over"] == 25 and rows[1]["heroism"] == 500, rows[1]
        assert rows[16]["need_over"] == 100, rows[16]
        assert rows[10]["need_over"] == 50, rows[10]
        mapped = sum(1 for r in rows.values() if r["stat"] is not None)
        assert mapped >= 80, f"only {mapped} insignias mapped to a stat id"
        print(f"self-test ok: {len(rows)} insignias, {mapped} with a stat id, "
              f"idx1 over {rows[1]['need_over']} for +{rows[1]['heroism']} heroism")
        return

    if args.json:
        json.dump({str(k): v for k, v in rows.items()}, sys.stdout, indent=1)
        print()
        return

    print(f"{'idx':>4} {'stat':>7} {'need >':>9} {'heroism':>8}")
    for index, row in rows.items():
        stat = f"{row['stat']:#x}" if row["stat"] is not None else "-"
        print(f"{index:>4} {stat:>7} {row['need_over']:>9} {row['heroism']:>8}")


if __name__ == "__main__":
    main()

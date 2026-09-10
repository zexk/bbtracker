#!/usr/bin/env python3
"""Dump the MGSPW Steam achievement map (metadata + predicate registrations).

Metadata lives in plaintext .data, so the table alone needs only the exe.
Predicate function pointers are installed at runtime by the registrar in
.text, so mapping id -> predicate needs a runtime .text dump captured with
`probe-mgspw-memory.py --dump-text`.

  python3 scripts/pwach.py --self-test
  python3 scripts/pwach.py --text /tmp/pw_text.bin
"""
import argparse
import struct
import sys
import siblings

pwdis = siblings.load("pwdis.py")

META = 0x14105C230       # 50 rows: u32 steam_no, u32 id, u8 enabled, u8, u8, u8
PRED_TBL = 0x141596A90   # bool(*)() per achievement id, filled by REGISTRAR
REGISTRAR = 0x1400388D0
COUNT = 50


def data_rva_to_off(secs, rva):
    for s in secs.values():
        if s["va"] <= rva < s["va"] + s["vsz"]:
            return s["raw"] + (rva - s["va"])
    raise ValueError(f"rva {rva:#x} in no section")


def metadata(exe, secs):
    off = data_rva_to_off(secs, META - pwdis.BASE)
    rows = []
    for i in range(COUNT):
        steam_no, ach_id, flags = struct.unpack_from("<IiI", exe, off + i * 12)
        rows.append((ach_id, steam_no, flags & 0xFF))
    return rows


def predicates(text):
    """Replay the registrar's lea/mov pairs to recover id -> predicate."""
    import capstone
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
    start = REGISTRAR - pwdis.BASE - pwdis.TEXT_RVA
    ins = list(md.disasm(text[start:start + 0x2C0], REGISTRAR))
    out, pend = {}, None
    for k, i in enumerate(ins):
        nxt = ins[k + 1].address if k + 1 < len(ins) else i.address + i.size
        if "[rip " not in i.op_str:
            continue
        disp = int(i.op_str.split("[rip ")[1].split("]")[0].replace(" ", ""), 0)
        if i.mnemonic == "lea" and i.op_str.startswith("rax, "):
            pend = nxt + disp
        elif i.mnemonic == "mov" and i.op_str.endswith(", rax") and pend:
            out[(nxt + disp - PRED_TBL) // 8] = pend
            pend = None
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--exe", default=pwdis.EXE)
    ap.add_argument("--text", help="runtime .text dump (probe --dump-text)")
    ap.add_argument("--self-test", action="store_true")
    args = ap.parse_args()

    exe = open(args.exe, "rb").read()
    secs = pwdis.parse_sections(exe)
    rows = metadata(exe, secs)

    if args.self_test:
        assert len(rows) == COUNT, len(rows)
        assert [r[0] for r in rows] == list(range(COUNT)), "ids not 0..49"
        assert all(sn == i + 2 for i, sn, _ in rows), "steam_no != id+2"
        assert all(en == 1 for _, _, en in rows), "row not enabled"
        print("self-test ok: 50 rows, ACH_QXS_002..051, all enabled")
        return 0

    preds = predicates(open(args.text, "rb").read()) if args.text else {}
    for ach_id, steam_no, enabled in rows:
        p = preds.get(ach_id)
        pred = f"{p:#x}" if p else ("-" if args.text else "?")
        stub = " (stub)" if p == 0x14002A4E0 else ""
        print(f"id {ach_id:2d}  ACH_QXS_{steam_no:03d}  enabled={enabled}  "
              f"predicate {pred}{stub}")
    return 0


if __name__ == "__main__":
    sys.exit(main())

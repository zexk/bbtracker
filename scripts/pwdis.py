#!/usr/bin/env python3
"""Static+runtime PE analysis for MGSPW FOXHOUND research.

The on-disk .text is encrypted, but .pdata (40,711 RUNTIME_FUNCTION records)
and .rdata are not. This tool overlays the plaintext .pdata function map onto
a runtime-decrypted .text dump (captured with probe-mgspw-memory.py
--dump-text) and disassembles functions with capstone.

Commands work in two modes:
  * --self-test / --functions / --find need only the on-disk exe + a .text dump
  * run `probe-mgspw-memory.py --dump-text text.bin` first, against a live game

Typical session:
  python3 scripts/probe-mgspw-memory.py --dump-text /tmp/pw_text.bin
  python3 scripts/pwdis.py --text /tmp/pw_text.bin --functions | head
  python3 scripts/pwdis.py --text /tmp/pw_text.bin --find-imm 0x2008E
  python3 scripts/pwdis.py --text /tmp/pw_text.bin --xref-string total_play_time
"""
import argparse
import io
import json
import re
import struct
import sys

EXE = ("/home/zexk/.local/share/Steam/steamapps/common/MGS_PW/mgspw/"
       "METAL GEAR SOLID PEACE WALKER.exe")
BASE = 0x140000000
TEXT_RVA = 0x1000


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


def pdata_functions(data, secs):
    """Yield (begin_rva, end_rva) from .pdata RUNTIME_FUNCTION records."""
    p = secs[".pdata"]
    blob = data[p["raw"]:p["raw"] + p["rawsz"]]
    out = []
    for off in range(0, len(blob) - 12, 12):
        begin, end, _unwind = struct.unpack_from("<III", blob, off)
        if begin and end > begin:
            out.append((begin, end))
    return out


def load_text(path):
    return open(path, "rb").read()


def disassemble(text, begin_rva, end_rva, detail=False):
    from capstone import Cs, CS_ARCH_X86, CS_MODE_64
    md = Cs(CS_ARCH_X86, CS_MODE_64)
    md.detail = detail
    start = begin_rva - TEXT_RVA
    code = text[start:start + (end_rva - begin_rva)]
    return list(md.disasm(code, BASE + begin_rva))


def insn_imm(insn):
    """Immediate operand value, or None."""
    from capstone.x86 import X86_OP_IMM
    for op in insn.operands:
        if op.type == X86_OP_IMM:
            return op.imm
    return None


def cmd_functions(args, secs, funcs):
    print(f"{len(funcs)} functions from .pdata")
    for i, (b, e) in enumerate(funcs[:args.limit]):
        print(f"[{i}] {BASE + b:#x} .. {BASE + e:#x}  ({e - b} bytes)")


def cmd_find_imm(args, secs, funcs, text):
    """Find functions whose code uses an immediate exactly (stat id)."""
    target = args.find_imm
    hits = []
    for b, e in funcs:
        for insn in disassemble(text, b, e, detail=True):
            if insn_imm(insn) == target:
                hits.append((b, insn))
    print(f"{len(hits)} exact immediate {target:#x} uses in "
          f"{len(set(h[0] for h in hits))} functions")
    for b, insn in hits[:args.limit]:
        print(f"  func {BASE + b:#x}  {insn.address:#x}: "
              f"{insn.mnemonic} {insn.op_str}")


def find_string_rva(exe_data, secs, s):
    needle = s.encode() + b"\0"
    rd = secs[".rdata"]
    blob = exe_data[rd["raw"]:rd["raw"] + rd["rawsz"]]
    idx = blob.find(needle)
    if idx < 0:
        return None
    return rd["va"] + idx


def rip_mem_target(insn):
    """Absolute target of a RIP-relative memory operand, else None."""
    from capstone.x86 import X86_OP_MEM, X86_REG_RIP
    for op in insn.operands:
        if op.type == X86_OP_MEM and op.mem.base == X86_REG_RIP:
            return insn.address + insn.size + op.mem.disp
    return None


def xref_va(funcs, text, target_va, limit):
    """Find RIP-relative references to an absolute VA across all functions."""
    hits = []
    for b, e in funcs:
        for insn in disassemble(text, b, e, detail=True):
            t = rip_mem_target(insn)
            if t == target_va:
                hits.append((b, insn))
    return hits


def xref_call(funcs, text, target_va, limit):
    """Find direct call sites whose target is target_va."""
    hits = []
    for b, e in funcs:
        for insn in disassemble(text, b, e, detail=True):
            if insn.mnemonic in ("call", "jmp") and insn_imm(insn) == target_va:
                hits.append((b, insn))
    return hits


def cmd_xref_string(args, secs, funcs, text, exe_data):
    """Find an .rdata string, compute its runtime VA, find code refs."""
    str_rva = find_string_rva(exe_data, secs, args.xref_string)
    if str_rva is None:
        sys.exit(f"string {args.xref_string!r} not in .rdata")
    str_va = BASE + str_rva
    print(f"string {args.xref_string!r} at VA {str_va:#x} (rva {str_rva:#x})")
    hits = xref_va(funcs, text, str_va, args.limit)
    print(f"{len(hits)} code refs in {len(set(h[0] for h in hits))} functions")
    for b, insn in hits[:args.limit]:
        print(f"  func {BASE + b:#x}  {insn.address:#x}: "
              f"{insn.mnemonic} {insn.op_str}")


def rdata_string_at(exe_data, secs, va):
    """Printable C string at VA if it lands in .rdata, else None."""
    rd = secs[".rdata"]
    rva = va - BASE
    if not (rd["va"] <= rva < rd["va"] + rd["vsz"]):
        return None
    off = rd["raw"] + (rva - rd["va"])
    end = exe_data.find(b"\0", off)
    if end <= off or end - off > 72:
        return None
    s = exe_data[off:end]
    if all(32 <= c < 127 or c == 37 for c in s):
        try:
            return s.decode()
        except UnicodeDecodeError:
            return None
    return None


def cmd_disasm(args, secs, funcs, text, exe_data):
    """Disassemble one function by runtime address (accepts 0x140... VA)."""
    va = args.disasm
    b = va - BASE
    match = next((x for x in funcs if x[0] <= b < x[1]), None)
    if match is None:
        sys.exit(f"no .pdata function contains VA {va:#x}")
    fb, fe = match
    print(f"function {BASE + fb:#x} .. {BASE + fe:#x} ({fe - fb} bytes)")
    for insn in disassemble(text, fb, fe, detail=True):
        line = f"  {insn.address:#x}  {insn.mnemonic:10} {insn.op_str}"
        t = rip_mem_target(insn)
        if t is not None:
            s = rdata_string_at(exe_data, secs, t)
            if s is not None:
                line += f'   ; "{s}"'
        print(line)


def cmd_raw(args, secs, funcs, text, exe_data):
    """Disassemble a raw RVA window (for leaf functions missing from .pdata)."""
    va = args.raw
    off = (va - BASE) - TEXT_RVA
    code = text[off:off + args.raw_len]
    from capstone import Cs, CS_ARCH_X86, CS_MODE_64
    md = Cs(CS_ARCH_X86, CS_MODE_64)
    md.detail = True
    for insn in md.disasm(code, va):
        line = f"  {insn.address:#x}  {insn.mnemonic:10} {insn.op_str}"
        t = rip_mem_target(insn)
        if t is not None:
            s = rdata_string_at(exe_data, secs, t)
            note = t - BASE
            line += f"   ; -> {t:#x} (rva {note:#x})"
            if s is not None:
                line += f' "{s}"'
        print(line)


def cmd_xref_mem(args, secs, funcs, text):
    """Find RIP-relative references to an absolute VA (global variable)."""
    hits = xref_va(funcs, text, args.xref_mem, args.limit * 4)
    print(f"{len(hits)} refs to {args.xref_mem:#x} in "
          f"{len(set(h[0] for h in hits))} functions")
    for b, insn in hits[:args.limit]:
        print(f"  func {BASE + b:#x}  {insn.address:#x}: "
              f"{insn.mnemonic} {insn.op_str}")


def cmd_cmd_tables(args, secs, exe_data):
    """Dump the script-command dispatch tables that live in .data.

    One record is {hash u32, 0 u32, native ptr u64} = 16 bytes; a table is a
    run of such records with strictly ascending hashes. Script calls reach
    native code only through these, so a hash is the only stable name a GCL
    command has. NB: the records are 16 bytes, not 8 - reading a table at a
    +8 offset yields a plausible but wrong hash/function pairing.
    """
    data = secs[".data"]
    text_lo = BASE + secs[".text"]["va"]
    text_hi = text_lo + secs[".text"]["vsz"]
    off, end = data["raw"], data["raw"] + data["rawsz"]
    va0 = BASE + data["va"]
    rows = []

    def flush():
        if len(rows) >= args.min_table and all(
                rows[i][1] < rows[i + 1][1] for i in range(len(rows) - 1)):
            for va, h, ptr in rows:
                if args.cmd_hash in (None, h) and args.cmd_ptr in (None, ptr):
                    print(f"{va:#x}\t{h:#08x}\t{ptr:#x}")
        rows.clear()

    while off + 16 <= end:
        h, pad, ptr = struct.unpack_from("<IIQ", exe_data, off)
        if pad == 0 and 0 < h < 0x1000000 and text_lo <= ptr < text_hi:
            rows.append((va0 + (off - data["raw"]), h, ptr))
        elif rows:
            flush()
        off += 16
    flush()


def cmd_xref_func(args, secs, funcs, text):
    """List callers of a function VA; resolve a literal string loaded into rcx
    right before each call when the target is a string-keyed lookup."""
    target = args.xref_func
    hits = xref_call(funcs, text, target, args.limit)
    print(f"{len(hits)} callers of {target:#x}")
    for b, insn in hits[:args.limit]:
        line = f"  func {BASE + b:#x}  {insn.address:#x}: {insn.mnemonic}"
        print(line)


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--exe", default=EXE)
    ap.add_argument("--text", help="runtime-decrypted .text dump (probe --dump-text)")
    ap.add_argument("--limit", type=int, default=20)
    ap.add_argument("--functions", action="store_true", help="list .pdata functions")
    ap.add_argument("--find-imm", type=lambda v: int(v, 0),
                    help="find functions referencing this immediate (e.g. 0x2008E)")
    ap.add_argument("--xref-string", help="find code referencing this .rdata string")
    ap.add_argument("--xref-func", type=lambda v: int(v, 0),
                    help="list call sites targeting this function VA")
    ap.add_argument("--disasm", type=lambda v: int(v, 0),
                    help="disassemble the function containing this VA")
    ap.add_argument("--raw", type=lambda v: int(v, 0),
                    help="disassemble a raw window at this VA (ignores .pdata)")
    ap.add_argument("--raw-len", type=lambda v: int(v, 0), default=0x200,
                    help="bytes for --raw (default 0x200)")
    ap.add_argument("--xref-mem", type=lambda v: int(v, 0),
                    help="find RIP-relative refs to this absolute VA")
    ap.add_argument("--cmd-tables", action="store_true",
                    help="dump the script-command hash -> native dispatch tables")
    ap.add_argument("--cmd-hash", type=lambda v: int(v, 0),
                    help="restrict --cmd-tables to this command hash")
    ap.add_argument("--cmd-ptr", type=lambda v: int(v, 0),
                    help="restrict --cmd-tables to this native function VA")
    ap.add_argument("--min-table", type=int, default=8,
                    help="shortest accepted --cmd-tables run (default 8)")
    ap.add_argument("--self-test", action="store_true")
    args = ap.parse_args()

    exe_data = open(args.exe, "rb").read()
    secs = parse_sections(exe_data)
    funcs = pdata_functions(exe_data, secs)

    if args.self_test:
        assert ".pdata" in secs and ".rdata" in secs and ".text" in secs
        assert len(funcs) > 30000, f"only {len(funcs)} functions"
        b, e = funcs[0]
        assert 0 < b < e
        args.cmd_hash, args.cmd_ptr, args.min_table = 0x26DF41, 0x14039BC70, 8
        rows = io.StringIO()
        stdout, sys.stdout = sys.stdout, rows
        try:
            cmd_cmd_tables(args, secs, exe_data)
        finally:
            sys.stdout = stdout
        assert rows.getvalue().split(), "title activation command pairing missing"
        print(f"self-test ok: {len(funcs)} functions, "
              f".text {secs['.text']['vsz']:#x}, .rdata {secs['.rdata']['vsz']:#x}, "
              f"cmd 0x26df41 -> {rows.getvalue().split()[2]}")
        return

    if args.functions:
        cmd_functions(args, secs, funcs)
        return

    if args.cmd_tables:
        cmd_cmd_tables(args, secs, exe_data)
        return

    if not args.text:
        sys.exit("need --text for disassembly commands "
                 "(run probe-mgspw-memory.py --dump-text first)")
    text = load_text(args.text)

    if args.find_imm is not None:
        cmd_find_imm(args, secs, funcs, text)
    elif args.xref_string:
        cmd_xref_string(args, secs, funcs, text, exe_data)
    elif args.xref_func is not None:
        cmd_xref_func(args, secs, funcs, text)
    elif args.raw is not None:
        cmd_raw(args, secs, funcs, text, exe_data)
    elif args.xref_mem is not None:
        cmd_xref_mem(args, secs, funcs, text)
    elif args.disasm is not None:
        cmd_disasm(args, secs, funcs, text, exe_data)
    else:
        ap.error("pick a command: --functions / --find-imm / --xref-string /"
                 " --xref-func / --disasm")


if __name__ == "__main__":
    main()

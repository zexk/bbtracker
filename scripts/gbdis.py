#!/usr/bin/env python3
"""Minimal SM83 disassembler + absolute-address xref scanner for GB ROMs."""
import sys

R8 = ["b","c","d","e","h","l","[hl]","a"]
R16 = ["bc","de","hl","sp"]
R16S = ["bc","de","hl","af"]
CC = ["nz","z","nc","c"]
ALU = ["add a,","adc a,","sub ","sbc a,","and ","xor ","or ","cp "]
ROT = ["rlc","rrc","rl","rr","sla","sra","swap","srl"]

def dis1(b, pc):
    """returns (text, length)."""
    o = b[pc]
    d8 = lambda: b[pc+1]
    d16 = lambda: b[pc+1] | (b[pc+2] << 8)
    s8 = lambda: b[pc+1] - 256 if b[pc+1] > 127 else b[pc+1]
    x, y, z = o >> 6, (o >> 3) & 7, o & 7
    p, q = y >> 1, y & 1
    if o == 0x00: return "nop", 1
    if o == 0x10: return "stop", 2
    if o == 0x76: return "halt", 1
    if o == 0xF3: return "di", 1
    if o == 0xFB: return "ei", 1
    if o == 0x08: return f"ld [${d16():04X}],sp", 3
    if o == 0x18: return f"jr ${pc+2+s8():04X}", 2
    if x == 0 and z == 0 and y >= 4: return f"jr {CC[y-4]},${pc+2+s8():04X}", 2
    if x == 0 and z == 1 and q == 0: return f"ld {R16[p]},${d16():04X}", 3
    if x == 0 and z == 1 and q == 1: return f"add hl,{R16[p]}", 1
    if x == 0 and z == 2:
        tgt = ["[bc]","[de]","[hl+]","[hl-]"][p]
        return (f"ld a,{tgt}" if q else f"ld {tgt},a"), 1
    if x == 0 and z == 3: return f"{'dec' if q else 'inc'} {R16[p]}", 1
    if x == 0 and z == 4: return f"inc {R8[y]}", 1
    if x == 0 and z == 5: return f"dec {R8[y]}", 1
    if x == 0 and z == 6: return f"ld {R8[y]},${d8():02X}", 2
    if x == 0 and z == 7: return ["rlca","rrca","rla","rra","daa","cpl","scf","ccf"][y], 1
    if x == 1: return f"ld {R8[y]},{R8[z]}", 1
    if x == 2: return f"{ALU[y]}{R8[z]}", 1
    if x == 3:
        if z == 0 and y < 4: return f"ret {CC[y]}", 1
        if o == 0xE0: return f"ldh [$FF{d8():02X}],a", 2
        if o == 0xF0: return f"ldh a,[$FF{d8():02X}]", 2
        if o == 0xE8: return f"add sp,{s8()}", 2
        if o == 0xF8: return f"ld hl,sp{s8():+d}", 2
        if z == 1 and q == 0: return f"pop {R16S[p]}", 1
        if o == 0xC9: return "ret", 1
        if o == 0xD9: return "reti", 1
        if o == 0xE9: return "jp hl", 1
        if o == 0xF9: return "ld sp,hl", 1
        if z == 2 and y < 4: return f"jp {CC[y]},${d16():04X}", 3
        if o == 0xE2: return "ld [$FF00+c],a", 1
        if o == 0xF2: return "ld a,[$FF00+c]", 1
        if o == 0xEA: return f"ld [${d16():04X}],a", 3
        if o == 0xFA: return f"ld a,[${d16():04X}]", 3
        if o == 0xC3: return f"jp ${d16():04X}", 3
        if o == 0xCB:
            c = b[pc+1]
            cx, cy, cz = c >> 6, (c >> 3) & 7, c & 7
            if cx == 0: return f"{ROT[cy]} {R8[cz]}", 2
            return f"{['','bit','res','set'][cx]} {cy},{R8[cz]}", 2
        if z == 4 and y < 4: return f"call {CC[y]},${d16():04X}", 3
        if z == 5 and q == 0: return f"push {R16S[p]}", 1
        if o == 0xCD: return f"call ${d16():04X}", 3
        if z == 6: return f"{ALU[y]}${d8():02X}", 2
        if z == 7: return f"rst ${y*8:02X}", 1
    return f"db ${o:02X}", 1

def dis(b, start, end, base=None):
    """disassemble file offsets [start,end); base = CPU address of start."""
    base = start if base is None else base
    pc, out = start, []
    while pc < end:
        try:
            t, n = dis1(b, pc)
        except IndexError:
            break
        raw = b[pc:pc+n].hex().upper()
        out.append(f"{base + (pc-start):04X}  {raw:<8} {t}")
        pc += n
    return "\n".join(out)

def xref(b, lo, hi, banks=None):
    """find every instruction referencing an absolute address in [lo,hi]."""
    hits = []
    nbanks = len(b) // 0x4000
    for bank in range(nbanks):
        if banks is not None and bank not in banks:
            continue
        off = bank * 0x4000
        cpu0 = 0x0000 if bank == 0 else 0x4000
        pc = off
        end = off + 0x4000
        while pc < end:
            try:
                t, n = dis1(b, pc)
            except IndexError:
                break
            if n == 3 and ("ld " in t or "jp" in t or "call" in t):
                a = b[pc+1] | (b[pc+2] << 8)
                if lo <= a <= hi and ("[$" in t):
                    hits.append((bank, cpu0 + (pc - off), pc, t))
            pc += n
    return hits

def scan(rom, addrs):
    """byte-pattern scan for ld [nn],a (EA) / ld a,[nn] (FA) / ld hl,nn (21)."""
    out = []
    for i in range(len(rom) - 2):
        op = rom[i]
        if op not in (0xEA, 0xFA, 0x21):
            continue
        a = rom[i+1] | (rom[i+2] << 8)
        if a in addrs:
            bank = i // 0x4000
            cpu = (i % 0x4000) + (0 if bank == 0 else 0x4000)
            kind = {0xEA: "ld [%04X],a", 0xFA: "ld a,[%04X]", 0x21: "ld hl,%04X"}[op] % a
            out.append((bank, cpu, i, kind))
    return out

if __name__ == "__main__":
    rom = open(sys.argv[1], "rb").read()
    mode = sys.argv[2]
    if mode == "dis":
        s = int(sys.argv[3], 16); e = int(sys.argv[4], 16)
        base = int(sys.argv[5], 16) if len(sys.argv) > 5 else s
        print(dis(rom, s, e, base))
    elif mode == "scan":
        addrs = {int(x, 16) for x in sys.argv[3:]}
        for bank, cpu, off, t in scan(rom, addrs):
            print(f"bank {bank:02X}  {cpu:04X} (file {off:06X})  {t}")
    elif mode == "xref":
        lo = int(sys.argv[3], 16)
        hi = int(sys.argv[4], 16) if len(sys.argv) > 4 else lo
        for bank, cpu, off, t in xref(rom, lo, hi):
            print(f"bank {bank:02X}  {cpu:04X} (file {off:06X})  {t}")


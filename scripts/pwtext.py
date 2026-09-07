#!/usr/bin/env python3
"""Decrypt Peace Walker's archives and dump the mission name table.

The Master Collection wraps every archive in an MT19937 keystream XOR seeded
from a hash of the file name, on top of the PSP-era container formats. The
decryption and container parsing come from Dmytro Bidlov's Peace Walker
Localization Tool, so this script drives that rather than reimplementing it:

    git clone https://github.com/LittleBitUA/PEACE-WALKER-LOCALIZATION-TOOL
    python3 scripts/pwtext.py --tool /path/to/PEACE-WALKER-LOCALIZATION-TOOL/src

Needs numpy. The mission names live in SLOT element `002FF/8`, where Main Ops
sit at `index - 2` and Extra Ops are index-aligned (both anchored on missions
this profile actually cleared).
"""
import argparse
import os
import re
import sys

DEFAULT_GAME = ("/home/zexk/.local/share/Steam/steamapps/common/MGS_PW/mgspw")
NAME_ELEMENT = "slot/002FF/8/"


def dump_text(tool_src, game_dir, out_path):
    sys.path.insert(0, tool_src)
    import slottext
    rel = os.path.join(game_dir, "MLG", "disc0_rel")
    dat = os.path.join(rel, "002aba34.DAT")
    key = os.path.join(rel, "002aba34.KEY")
    return slottext.export(dat, key, out_path)


def mission_names(text_path):
    """-> {index: name} for the mission-title element."""
    out = {}
    with open(text_path, encoding="utf-8") as fh:
        for line in fh:
            if NAME_ELEMENT not in line:
                continue
            m = re.search(re.escape(NAME_ELEMENT) + r"(\d+) \xb7 (.*)$", line.rstrip("\n"))
            if m:
                out[int(m.group(1))] = m.group(2)
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--tool", required=True, help="the localization tool's src/ directory")
    ap.add_argument("--game", default=DEFAULT_GAME)
    ap.add_argument("--text", default="/tmp/pw_slot_text.txt")
    args = ap.parse_args()

    if not os.path.exists(args.text):
        res = dump_text(args.tool, args.game, args.text)
        print(f"exported {res['strings']} strings -> {args.text}", file=sys.stderr)
    names = mission_names(args.text)
    print(f"{len(names)} entries in {NAME_ELEMENT}")
    for idx in sorted(names):
        # Main Ops are shifted by two header strings; Extra Ops line up 1:1.
        guess = idx - 2 if idx < 36 else idx
        print(f"  index {idx:3d}  mission id {guess:3d}?  {names[idx]}")
    return 0


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
"""One-off: scan PW exe strings for achievement/stat anchors (read-only)."""
import re
import sys

path = sys.argv[1] if len(sys.argv) > 1 else (
    "/home/zexk/.local/share/Steam/steamapps/common/MGS_PW/mgspw/"
    "METAL GEAR SOLID PEACE WALKER.exe")
data = open(path, "rb").read()
strs = sorted(set(m.group().decode() for m in re.finditer(rb"[ -~]{5,}", data)))
print(len(strs), "ascii strings")


def show(title, pats, cap=60):
    print("== " + title + " ==")
    n = 0
    for s in strs:
        low = s.lower()
        if any(p in low for p in pats):
            print("  " + s[:150])
            n += 1
            if n >= cap:
                return


show("achievement", ["achiev"])
show("headshot", ["headshot", "head_shot"])
show("steam stats iface", ["steamuserstats", "userstats"])
show("stat set/store", ["setstat", "storestat"])
show("trophy", ["troph"])

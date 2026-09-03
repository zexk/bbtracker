#!/usr/bin/env python3
"""Record which bytes of the Peace Walker save block change over time.

Point this at a running game, play, and it reports every u32 slot that moved,
with its value history. Used to map results-screen fields (rank, score,
alerts, kills) onto save offsets: run it across one mission, then compare the
changed offsets with the numbers the results screen showed.

  python3 scripts/pwwatch.py --seconds 600 --out /tmp/pw_watch.json
"""
import argparse
import json
import signal
import struct
import sys
import time
import importlib.util

_spec = importlib.util.spec_from_file_location(
    "pwprobe", __file__.rsplit("/", 1)[0] + "/probe-mgspw-memory.py")
probe = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(probe)

SIZE = 0x30000  # covers every field the probe knows about


def sample(memory, block):
    return probe.read_mem(memory, block, SIZE)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--pid", type=int)
    ap.add_argument("--seconds", type=float, default=600.0)
    ap.add_argument("--interval", type=float, default=2.0)
    ap.add_argument("--out", default="/tmp/pw_watch.json")
    ap.add_argument("--max-slots", type=int, default=4000,
                    help="stop reporting once this many slots have moved")
    args = ap.parse_args()

    pid = args.pid or probe.find_pid()
    if pid is None:
        print("game not running", file=sys.stderr)
        return 1
    base = probe.module_base(open(f"/proc/{pid}/maps").read())
    memory = open(f"/proc/{pid}/mem", "rb", 0)
    resolved = probe.resolve(memory, base, probe.code_range(memory, base))
    sr = resolved.get("PW_SAVEROOT")
    if sr is None:
        print("PW_SAVEROOT unresolved", file=sys.stderr)
        return 1
    block = struct.unpack("<Q", probe.read_mem(memory, sr, 8))[0]
    print(f"pid {pid} save block {block:#x}; watching {SIZE:#x} bytes "
          f"for {args.seconds:.0f}s", file=sys.stderr)

    prev = sample(memory, block)
    n = SIZE // 4
    history = {}
    t_end = time.time() + args.seconds
    t0 = time.time()
    stop = []
    # SIGTERM/SIGINT end the run and still write the report
    signal.signal(signal.SIGTERM, lambda *_: stop.append(True))
    signal.signal(signal.SIGINT, lambda *_: stop.append(True))
    while time.time() < t_end and not stop:
        time.sleep(args.interval)
        try:
            now_block = struct.unpack("<Q", probe.read_mem(memory, sr, 8))[0]
            if now_block != block:
                # save block reallocated (mission load): rebaseline, keep history
                print(f"save block moved {block:#x} -> {now_block:#x}",
                      file=sys.stderr)
                block = now_block
                prev = sample(memory, block)
                continue
            cur = sample(memory, block)
        except OSError:
            print("read failed (game closed?)", file=sys.stderr)
            break
        if cur == prev:
            continue
        a = struct.unpack(f"<{n}I", cur)
        b = struct.unpack(f"<{n}I", prev)
        for i in range(n):
            if a[i] == b[i]:
                continue
            slot = history.setdefault(i * 4, [b[i]])
            slot.append(a[i])
        prev = cur
        if len(history) > args.max_slots:
            print("too many slots moved; stopping early", file=sys.stderr)
            break
    out = {f"{off:#x}": vals for off, vals in sorted(history.items())}
    with open(args.out, "w") as fh:
        json.dump({"block": f"{block:#x}",
                   "seconds": round(time.time() - t0, 1),
                   "slots": out}, fh, indent=1)
    print(f"{len(out)} u32 slots changed -> {args.out}", file=sys.stderr)
    for off, vals in list(sorted(history.items()))[:40]:
        print(f"  +{off:#07x}  {vals[0]} -> {vals[-1]}"
              f"{'  (' + str(len(vals)) + ' steps)' if len(vals) > 2 else ''}")
    return 0


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env bash
# Static analysis triage for MGS4.exe - run this once the binary is available.
# Usage: ./scripts/analyze-mgs4.sh <path-to-MGS4.exe>
set -euo pipefail

EXE="${1:?usage: $0 <path-to-MGS4.exe>}"
OUTDIR="$(dirname "$0")/../analysis/mgs4"
mkdir -p "$OUTDIR"

echo "=== basic info ==="
r2 -q -c "iI; iS; ii~DLL" "$EXE" > "$OUTDIR/basic.txt" 2>/dev/null
cat "$OUTDIR/basic.txt"

echo "=== string search: rank/emblem/stat keywords ==="
for kw in alert kill continue ration save emblem rank score \
          headshot cqc holdup search crawl crouch roll \
          foxhound foxhound hound doberman pig cow chicken \
          BigBoss FoxHound Fox Hound Mantis Octopus; do
    echo "--- $kw ---"
    r2 -q -c "izz~$kw" "$EXE" 2>/dev/null | head -10
done > "$OUTDIR/strings.txt"

echo "=== import table ==="
r2 -q -c "ii" "$EXE" > "$OUTDIR/imports.txt" 2>/dev/null

echo "=== entry point + main analysis ==="
r2 -q -A -c "afl~main" "$EXE" > "$OUTDIR/functions.txt" 2>/dev/null || true

echo ""
echo "Results in $OUTDIR/"
echo "Next steps:"
echo "  r2 -A $EXE"
echo "  izz~ALERT          # find alert counter string refs"
echo "  axt @ str.ALERT    # cross-references to that string"
echo "  pdf @ <addr>       # disassemble the function using it"

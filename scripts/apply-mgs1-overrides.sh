#!/usr/bin/env bash
set -euo pipefail

P="${MGS1_PREFIX:-$HOME/.local/share/Steam/steamapps/compatdata/2131630/pfx}"
REG="$P/user.reg"
EXE='METAL GEAR SOLID.exe'

if pgrep -f "MGS1/$EXE" >/dev/null 2>&1; then
    echo "error: MGS1 is still running - quit it first, then re-run" >&2
    exit 1
fi

SECTION="[Software\\\\Wine\\\\AppDefaults\\\\$EXE\\\\DllOverrides]"
if grep -qF 'AppDefaults\\METAL GEAR SOLID.exe\\DllOverrides]' "$REG"; then
    echo "override section already present"
else
    {
        echo ""
        echo "$SECTION 1735689600"
        echo '#time=00000000'
        echo '"winmm"="native,builtin"'
        echo '"dinput8"="native,builtin"'
    } >> "$REG"
    echo "added winmm/dinput8 native-first overrides for $EXE"
fi

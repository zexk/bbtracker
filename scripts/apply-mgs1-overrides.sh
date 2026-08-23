#!/usr/bin/env bash
set -euo pipefail

P="${MGS1_PREFIX:-$HOME/.local/share/Steam/steamapps/compatdata/2131630/pfx}"
EXE='METAL GEAR SOLID.exe'
KEY="HKCU\\Software\\Wine\\AppDefaults\\$EXE\\DllOverrides"

WINEBIN="${WINEBIN:-}"
if [[ -z "$WINEBIN" ]]; then
    WINEBIN=$(find /nix/store -maxdepth 4 -path "*wine64-*/bin/wine" 2>/dev/null | sort | tail -1)
fi
if [[ -z "$WINEBIN" || ! -x "$WINEBIN" ]]; then
    echo "error: no wine64 found; set WINEBIN=<path-to-64bit-wine>" >&2
    exit 1
fi

export WINEPREFIX="$P"
"$WINEBIN" reg add "$KEY" /v winmm   /t REG_SZ /d native,builtin /f
"$WINEBIN" reg add "$KEY" /v dinput8 /t REG_SZ /d native,builtin /f
echo "--- current overrides ---"
"$WINEBIN" reg query "$KEY"

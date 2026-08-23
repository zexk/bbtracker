#!/usr/bin/env bash
set -euo pipefail

STEAMAPPS="${STEAMAPPS:-$HOME/.local/share/Steam/steamapps/common}"

cd "$(dirname "$0")"

echo "building..."
nix build .#bbtracker -L

declare -A TARGETS=(
  [bbtracker_mgs1.asi]="$STEAMAPPS/MGS1"
  [bbtracker_mgs3.asi]="$STEAMAPPS/MGS3"
  [bbtracker_mgs2.asi]="$STEAMAPPS/MGS2"
)

rc=0
for file in "${!TARGETS[@]}"; do
  dest="${TARGETS[$file]}"
  if [[ ! -d "$dest" ]]; then
    echo "skip $file: $dest not found (override with STEAMAPPS=...)" >&2
    rc=1
    continue
  fi
  install -v -m 644 "result/bin/$file" "$dest/"
done

MGS1_DIR="$STEAMAPPS/MGS1"
if [[ -d "$MGS1_DIR" ]]; then
  if [[ ! -f "$MGS1_DIR/winbackup/dinput8.dll.ual32" && -f "$MGS1_DIR/dinput8.dll" ]]; then
    file_out=$(file -b "$MGS1_DIR/dinput8.dll" 2>/dev/null || true)
    if [[ "$file_out" == *i386* ]]; then
      mkdir -p "$MGS1_DIR/winbackup"
      mv "$MGS1_DIR/dinput8.dll" "$MGS1_DIR/winbackup/dinput8.dll.ual32"
      echo "backed up stale 32-bit dinput8.dll -> winbackup/"
    fi
  fi
  if [[ ! -f "$MGS1_DIR/dinput8.dll" ]]; then
    install -v -m 644 third_party/ual/dinput8_x64.dll "$MGS1_DIR/dinput8.dll"
  else
    file_now=$(file -b "$MGS1_DIR/dinput8.dll" 2>/dev/null || true)
    if [[ "$file_now" == *i386* ]]; then
      install -v -m 644 third_party/ual/dinput8_x64.dll "$MGS1_DIR/dinput8.dll"
    fi
  fi
fi

exit $rc

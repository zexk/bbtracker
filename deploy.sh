#!/usr/bin/env bash
set -euo pipefail

STEAMAPPS="${STEAMAPPS:-$HOME/.local/share/Steam/steamapps/common}"

cd "$(dirname "$0")"

echo "building..."
nix build .#bbtracker -L

declare -A TARGETS=(
  [bbtracker_mg12.asi]="$STEAMAPPS/MG and MG2"
  [bbtracker_mgs1.asi]="$STEAMAPPS/MGS1"
  [bbtracker_mgs3.asi]="$STEAMAPPS/MGS3"
  [bbtracker_mgs2.asi]="$STEAMAPPS/MGS2"
  [bbtracker_mgs4.asi]="$STEAMAPPS/METAL GEAR SOLID 4/MGS4"
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

exit $rc

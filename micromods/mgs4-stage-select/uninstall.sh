#!/usr/bin/env bash
set -euo pipefail

game_root="${1:-$HOME/.local/share/Steam/steamapps/common/METAL GEAR SOLID 4}"
launcher="$game_root/Launcher/launcher.exe"
backup="$game_root/Launcher/launcher.original.exe"
selector="$game_root/MGS4/mgs4_stage_selector.asi"

if [[ ! -e "$backup" ]]; then
  echo "Original launcher backup not found: $backup" >&2
  exit 1
fi
mv -f "$backup" "$launcher"
rm -f "$selector"
echo "Restored original MGS4 launcher."

#!/usr/bin/env bash
set -euo pipefail

game_root="${1:-$HOME/.local/share/Steam/steamapps/common/METAL GEAR SOLID 4}"
launcher="$game_root/Launcher/launcher.exe"
backup="$game_root/Launcher/launcher.original.exe"
source_dir="$(cd "$(dirname "$0")" && pwd)"
repo_root="$(cd "$source_dir/../.." && pwd)"

# The flake already knows how to cross-compile both pieces, and CI builds that
# same derivation, so install from it rather than repeating the compiler line.
built="$(nix build --no-link --print-out-paths "$repo_root#mgs4-stage-selector")"

if [[ ! -e "$backup" ]]; then
  mv "$launcher" "$backup"
fi
install -m 755 "$built/launcher.exe" "$launcher"
install -m 644 "$built/mgs4_stage_selector.asi" "$game_root/MGS4/"
if [[ ! -e "$game_root/Launcher/mgs4-stage-selector.ini" ]]; then
  install -m 644 "$source_dir/mgs4-stage-selector.ini" "$game_root/Launcher/"
fi
echo "Installed MGS4 stage-selector micro-mod."
echo "Steam launch options: WINEDLLOVERRIDES=\"winmm=n,b\" %command%"

#!/usr/bin/env bash
set -euo pipefail

game_root="${1:-$HOME/.local/share/Steam/steamapps/common/METAL GEAR SOLID 4}"
launcher="$game_root/Launcher/launcher.exe"
backup="$game_root/Launcher/launcher.original.exe"
source_dir="$(cd "$(dirname "$0")" && pwd)"
output="$source_dir/launcher.exe"
selector="$source_dir/mgs4_stage_selector.asi"
runtime="$(nix build --no-link --print-out-paths \
  nixpkgs#pkgsCross.mingwW64.windows.mcfgthreads)"

nix shell nixpkgs#pkgsCross.mingwW64.stdenv.cc -c \
  x86_64-w64-mingw32-gcc -Os -s -static -municode -mwindows \
  -L"$runtime/lib" \
  "$source_dir/launcher.c" -o "$output"
nix shell nixpkgs#pkgsCross.mingwW64.stdenv.cc -c \
  x86_64-w64-mingw32-gcc -Os -s -static -shared \
  -L"$runtime/lib" "$source_dir/selector.c" -o "$selector" -luser32 -lgdi32

if [[ ! -e "$backup" ]]; then
  mv "$launcher" "$backup"
fi
install -m 755 "$output" "$launcher"
install -m 644 "$selector" "$game_root/MGS4/"
if [[ ! -e "$game_root/Launcher/mgs4-stage-selector.ini" ]]; then
  install -m 644 "$source_dir/mgs4-stage-selector.ini" "$game_root/Launcher/"
fi
echo "Installed MGS4 stage-selector micro-mod."
echo "Steam launch options: WINEDLLOVERRIDES=\"winmm=n,b\" %command%"

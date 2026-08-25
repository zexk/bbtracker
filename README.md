# bbtracker: Metal Gear rank tracker

Live rank and codename tracker overlay for Metal Gear Solid: Master Collection.
Tracks Metal Gear, Metal Gear 2, MGS1, MGS2, and MGS3 runs in real time,
including projected rank, requirements, and run stats.

Peace Walker is not a target. It ranks individual missions and awards cumulative
codenames from mission completion, weapon use, play style, and solo/co-op stats;
it has no single end-of-playthrough rank for this tracker to project.

MGS4 is a target. Its end-of-playthrough emblems replace legacy codenames but
still judge stats accumulated across one run, including its highest Big Boss emblem.

[Download latest release](https://github.com/zexk/bbtracker/releases/latest)

## Features

- Live projected rank and codename
- Rank requirements and remaining allowances
- Run stats, special-rank counters, and MGS3 checklists
- Windows, Linux, and Steam Deck support through ASI Loader and Proton

Press `F3` to toggle overlay. In MGS3, press `F4` to cycle tabs and
`Up`/`Down` to scroll checklists.

## Install

Requires Ultimate ASI Loader. Recommended setup:

- MGS1: install [MGSM2Fix](https://github.com/nuggslet/MGSM2Fix/releases).
- MGS2/MGS3: install [MGSHDFix](https://github.com/ShizCalev/MGSHDFix/releases).

Those fixes provide ASI-loading setup. Then download matching bbtracker release and
copy ASI into game directory:

- `bbtracker_mgs1.asi` into `MGS1`
- `bbtracker_mg12.asi` into `MG and MG2`
- `bbtracker_mgs2.asi` into `MGS2`
- `bbtracker_mgs3.asi` into `MGS3`

Follow fix project's Proton/Steam Deck DLL-override instructions when applicable.

## Build

Requires Git, CMake, Ninja, and Visual Studio with Desktop development with C++ workload.
Run from Developer PowerShell; CMake downloads pinned ImGui and MinHook sources automatically:

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Artifacts land in `build/`.

Linux rule tests:

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Rank sources and known gaps: [docs/rank_rules.md](docs/rank_rules.md).

MIT licensed. See [LICENSE](LICENSE).

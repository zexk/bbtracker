# bbtracker

Live codename tracker overlay for Metal Gear 1/2 and Metal Gear Solid 1, 2, and 3 in Metal Gear
Solid: Master Collection. Shows projected rank, requirements, and run stats.
Press `F3` to toggle overlay.
In MGS3, press `F4` to cycle tabs and `Up`/`Down` to scroll checklists.

## Install

bbtracker requires Ultimate ASI Loader. Recommended setup:

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

Requires Git, CMake, and Visual Studio with Desktop development with C++ workload.
Run from Developer PowerShell:

```powershell
git clone --depth 1 --branch v1.92.9b https://github.com/ocornut/imgui deps/imgui
git clone --depth 1 --branch v1.3.4 https://github.com/TsudaKageyu/minhook deps/minhook
$env:imgui = "$PWD\deps\imgui"
$env:minhook = "$PWD\deps\minhook"
cmake -S . -B build -A x64
cmake --build build --config Release
```

Artifacts land in `build/Release/`.

Linux rule tests:

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Rank sources and known gaps: [docs/rank_rules.md](docs/rank_rules.md).

MIT licensed. See [LICENSE](LICENSE).

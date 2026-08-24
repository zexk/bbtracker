# bbtracker - Project Plan

Live codename tracker overlay for Metal Gear Solid 2 / 3 (Master Collection, PC/Steam).
ImGui panel showing currently projected FOXHOUND codename, underlying stats, and top-rank
requirement checklist. Read-only memory probing only.

**Workflow rule: commit at the end of each completed phase.**

## Decisions (locked)

- **MGS3 is the golden child** (only MGS1+3 currently installed; MGS1 needs emu RE).
  Phase order: codename engine -> MGS3 probe -> MGS2 probe -> MGS1 later.
- Toggle key default **F3** (was INSERT), configurable via bbtracker.ini.
- Phase 1 targets MGS2 + MGS3 rules; MGS1 deferred (needs emulated-PSX RAM RE).
- Deliverable shape: **per-game builds** (`bbtracker_mgs2.asi`, `bbtracker_mgs3.asi`),
  shared static core lib.
- Panel content: projected codename + raw stats + live top-rank requirement tracker.
- Read/probe only. Never write game memory. Never patch game code bytes.
- Build on Linux via Nix flake + MinGW-w64 cross toolchain; test via Proton/Steam.

## Research findings

### Mod ecosystem (must coexist)

| Mod | Covers | Loading mechanism |
|---|---|---|
| MGSHDFix (Lyall; active fork ShizCalev) | MGS2, MGS3 (+MG1/2 MSX) | Ultimate ASI Loader (`dinput8.dll` proxy), loads `.asi` |
| MGSM2Fix (nuggslet) | MGS1 (+MG1/2 NES bonus content) | Ultimate ASI Loader |

- All three MC titles render via D3D11 -> kiero Present hook + ImGui works everywhere.
- Our `.asi` is just another UAL plugin dropped in the same game folder. Multi-ASI
  coexistence is already proven by the ecosystem.
- Steam Deck/Proton: fixes require `WINEDLLOVERRIDES="dinput8=n,b;d3d11=n,b"`; we need no extra overrides.
- Use our own ini filename `bbtracker.ini` (no collision with MGSHDFix.ini / MGSM2Fix.ini).

### Codename rules (community-documented sources of truth)

- MGS2: awarded per chapter end (Tanker / Plant / Tanker-Plant). Factors: difficulty,
  alerts, kills, continues, rations used, saves, shots fired, damage taken, playtime,
  radar setting, special items used.
  Big Boss (T-P): Extreme, <3h, <=3 alerts, 0 kills, 0 continues, 0 rations, <=8 saves,
  <700 shots fired, <10 lifebars damage, radar off, no special items.
- MGS3: per-difficulty rank grid (Hound->Doberman->Fox->FOXHOUND ladder etc.). Adds:
  meals eaten, LifeMed uses, damage bars, plants/animals captured; item-flag ranks
  (Markhor, Tsuchinoko, Kerotan, Leech). FOXHOUND = Extreme, <5h, 0 alerts, 0 kills,
  0 continues, <25 saves, <5 bars damage, 0 LifeMed, no special items.
- MGS1 (phase 2): Integral chart at tentenpro.com/muni_shinobu/mgs/int_codename.html;
  factors: discovered/alerts, kills, rations, continues, playtime, saves, difficulty tier.
- Sources: muni_shinobu codename charts (tentenpro.com), GameFAQs "Codename Ranking FAQ"
  by ObituaryBrthday (MGS3), GameFAQs Substance rank list, Siliconera rank guides.

### Existing reverse-engineering work (leverage, do not redo)

- MGS2 MC: RMLSNK FearLess Cheat Engine table (fearlessrevolution.com t=26222).
  Exposes exactly our stats: Alerts, Kills, Rations Used, Saves Used, Continues,
  Shots Fired, Damage Taken, Game Time, Seen By Enemy, Special Items Used.
- MGS3 MC: ANTIBigBoss/MGS3-Cheat-Trainer-GUI (C#), MajorZero69/mgs3-trainer (CE table).
- MGS2 MC trainer: sagefantasma/MGS2-Cheat-Trainer (C#).
- MGS1 MC (later): MGSM2Fix source shows how to reach emulated PSX RAM (Psy-X internals);
  classic PSX-era MGS1 stat addresses as starting point.
- Strategy: pattern-scan primary, hardcoded offset tables per known game version as
  fallback; degrade gracefully (panel warns) when scans fail after game patches.

## Architecture

```
flake.nix                 cross-build outputs + devshell + native test checks
README.md                 install/config instructions
src/
  common/
    config.{h,cpp}        INI via inipp: toggle key, panel pos/scale, log level
    log.h                 minimal file logger next to dll
    stats.h               GameStats struct shared across games
    codename/
      rules_mgs2.cpp      per-chapter x difficulty rank tables
      rules_mgs3.cpp      per-difficulty rank grid incl. item-flag ranks
      eval.h/.cpp         eval(GameStats) -> ranked codenames + attainability flags
    test/
      test_rules.cpp      native unit tests (ctest in devshell, no wine needed)
      fixtures_*.json     known stat combos -> expected codename
  mgs2/
    dllmain.cpp           entry, init thread
    offsets.{h,cpp}       pattern scans + pointer chains (from RMLSNK table)
    overlay.cpp           kiero D3D11 Present hook + ImGui panel
  mgs3/
    dllmain.cpp, offsets.{h,cpp}, overlay.cpp   same shape
vendor/                   flake-fetched pinned: imgui (docking), kiero, inipp
```

Shared core compiled once as static lib; two thin per-game DLL outputs.

Panel layout: header = current projected codename; body = stat rows (kills, alerts,
rations, saves, continues, shots, damage, time, special items); footer section =
top-rank requirement checklist with live pass/fail per requirement.

## flake.nix design

```nix
inputs: nixpkgs, flake-utils, imgui (pinned rev), kiero (pinned rev), inipp (pinned rev)
outputs:
  packages.bbtracker-mgs2   # .asi cross-built with pkgsCross.mingwW64
  packages.bbtracker-mgs3
  devShells.default         # mingwW64 toolchain, cmake, ninja, clang-tools, ctest
  checks.rules-tests        # codename rule unit tests compiled NATIVE
```

- MinGW-w64 cross compile; link `-static-libgcc -static-libstdc++`.
- Deps vendored via fetchFromGitHub pinned revs (reproducible, no submodules).

## Phases (commit at end of each)

- [x] **Phase 0 - scaffolding**: flake.nix (devshell + cross build), both .asi targets
  build as fully static PE32+ DLLs (system DLL imports only). kiero D3D11 Present +
  ResizeBuffers hooks, ImGui panel with placeholder rows, F3 toggle (bbtracker.ini,
  inipp), file logger. In-game smoke test under Proton still pending user run.
- [x] **Phase 1 - codename engine (MGS3)**: GameStats struct; muni_shinobu MGS3 chart
  transcribed into data-driven rules (61 entries: elite ladder, worst, specials, regular
  fallback) with precedence order; eval() + FOXHOUND requirement tracker; 14 native unit
  tests green (`nix build .#checks...`); sources/conflicts documented in
  docs/rank_rules.md. MGS2 table transcription deferred to its probe phase.
- [x] **Phase 2 - MGS3 probe + panel** (golden child): memory map extracted from
  ANTIBigBoss trainer ([module+0xACDE98]->stats struct); VirtualQuery-guarded reads;
  panel shows live projected codename + full stats + FOXHOUND tracker; rules updated
  from trainer cross-check (damage scale estimated, regular grid matrix, Cow>250,
  Markhor via capture count >=48). In-game verification checklist pending user run:
  1) drop bbtracker_mgs3.asi into MGS3 folder with MGSHDFix, F3 toggles panel
  2) stats match CE table readings
  3) difficulty byte mapping sanity (raw shown when >4)
  4) projected codename matches actual end-of-game award
  5) resolve remaining docs/rank_rules.md conflicts observed in play
- [ ] **Phase 3 - MGS2 probe + panel**: extract addresses from RMLSNK CE table /
  sagefantasma trainer; transcribe MGS2 rank tables; same wiring.
- [ ] **Phase 4 - polish**: config persistence, scale/position, version-detect warnings,
  README install guide, smoke-test checklist (both fixes installed simultaneously).
- [x] **Phase 5 - MGS1**: emulator work-array located via scored live-stage scan
  (NeopolitanDreamz MGS1_master.CT); kills/rations/continues confirmed in game,
  alerts/saves u16 probed; original chart transcribed; radar state auto-probed;
  playtime and current/max health derived from bmn's autosplitter layout; difficulty byte
  mapping unverified (ini override available). In-game verification pending.
- [ ] **Phase 6 - remaining gaps**: MGS3 story-flag bit hunting (kerotan/
  tsuchinoko/leech via change-log correlation), MGS2 damage-scale calibration,
  Sea Louse/Gazelle counters if surfaces are ever found.

### Build notes learned in Phase 0

- kiero.h keys off MSVC `_M_X64`: compile with `-D_M_X64=1` under MinGW.
- kiero.cpp `#include <Windows.h>` breaks on case-sensitive FS: generate a shim header
  dir in CMake (`Windows.h -> windows.h`).
- MinHook layout at v1.3.4: sources under `src/`, HDE under `src/hde/hde64.c`.
- inipp 1.0.13: API is `ini.parse(is)` + free fn `inipp::get_value(ini.sections[...], key, val)`;
  header at `inipp/inipp.h`.
- nixpkgs GCC15 mingw uses mcfgthread; `-static` on link options resolves it into the
  DLL statically (GNU ld synthesizes __imp_ thunks) — same trick pragmatrainer uses;
  do NOT ship libmcfgthread-2.dll alongside.
- ImGui deps to link: d3dcompiler (D3DCompile) + dwmapi (imgui_impl_win32).
- Flake gotcha: packages/devShells are TOP-LEVEL keys keyed by system, not nested inside.

## Compatibility rules (hard requirements)

1. Only read game memory; no writes anywhere, ever.
2. Hook only D3D11 Present via kiero vtable swap; call original through chain
   (MGSHDFix may be first in chain).
3. No dinput8 usage, no game code byte patches.
4. Own filenames only: bbtracker_*.asi, bbtracker.ini, bbtracker.log.
5. Degrade gracefully: if scans fail, show warning in panel, never crash.

## Testing & verification

- Native unit tests for codename rules run in devshell on every build (`checks`).
- Manual in-game via Proton: panel appears, toggle works, stat values match CE table,
  projected codename matches actual chapter-end award.
- clang-format / clang-tidy gates available in devshell.

## Risks

| Risk | Mitigation |
|---|---|
| Game patch breaks hardcoded addresses | Pattern scan + per-version offset tables + graceful warn |
| Community rank docs partially wrong | Rules are data tables; amend on observed discrepancy |
| Trainer/CE addresses target older versions | Re-verify against current exes in phases 2-3 |
| Present-hook interaction with MGSHDFix | Standard detour chaining; test with both fixes active |

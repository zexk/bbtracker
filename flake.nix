{
  description = "bbtracker - MGS Master Collection codename tracker overlay";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

    imgui = {
      url = "github:ocornut/imgui/v1.92.9b";
      flake = false;
    };

    minhook = {
      url = "github:TsudaKageyu/minhook/v1.3.4";
      flake = false;
    };

  };

  outputs =
    { self, nixpkgs, imgui, minhook }:
    let
      systems = [ "x86_64-linux" ];
      forAllSystems = nixpkgs.lib.genAttrs systems;
      cmakeProjectLine = nixpkgs.lib.findFirst
        (line: nixpkgs.lib.hasPrefix "project(bbtracker" line)
        "project(bbtracker VERSION 0.0.0 LANGUAGES C CXX)"
        (nixpkgs.lib.splitString "\n" (builtins.readFile ./CMakeLists.txt));
      version = builtins.head
        (builtins.match "project\\(bbtracker VERSION ([0-9]+\\.[0-9]+\\.[0-9]+).*" cmakeProjectLine);
    in
    {
      packages = forAllSystems (system:
        let
          pkgs = nixpkgs.legacyPackages.${system};
          mingw = pkgs.pkgsCross.mingwW64;

          # Full Inter release, subset per weight and renamed (OFL reserved
          # name) to keep the embedded payload tiny.
          interFonts = pkgs.runCommand "bbtracker-inter-fonts" {
            nativeBuildInputs = [ (pkgs.python3.withPackages (ps: [ ps.fonttools ])) ];
          } ''
            mkdir -p $out/extras/ttf
            python3 ${./scripts/subset_fonts.py} ${pkgs.inter.src} $out/extras/ttf
            cp ${pkgs.inter.src}/LICENSE.txt $out/LICENSE.txt
          '';

          bbtracker = mingw.stdenv.mkDerivation {
            pname = "bbtracker";
            inherit version;
            src = self;
            # PE strip -S keeps the multi-MB COFF symbol table; -s drops it.
            stripDebugFlags = [ "-s" ];
            cmakeBuildType = "MinSizeRel";

            nativeBuildInputs = [
              mingw.buildPackages.cmake
              mingw.buildPackages.ninja
            ];

            inherit imgui minhook;
            interSrc = interFonts;

            meta = with pkgs.lib; {
              description = "Live rank and codename tracker for Metal Gear: Master Collection";
              license = licenses.mit;
              platforms = [ "x86_64-windows" ];
            };
          };

          mgs4-stage-selector = mingw.stdenv.mkDerivation {
            pname = "mgs4-stage-selector";
            inherit version;
            src = self;
            stripDebugFlags = [ "-s" ];

            buildPhase = ''
              runHook preBuild
              $CC -Os -s -static -municode -mwindows \
                micromods/mgs4-stage-select/launcher.c -o launcher.exe
              $CC -Os -s -static -shared \
                micromods/mgs4-stage-select/selector.c -o mgs4_stage_selector.asi \
                -luser32 -lgdi32
              runHook postBuild
            '';

            installPhase = ''
              runHook preInstall
              mkdir -p $out
              cp launcher.exe mgs4_stage_selector.asi \
                micromods/mgs4-stage-select/{README.md,mgs4-stage-selector.ini} $out/
              runHook postInstall
            '';
          };
        in
        {
          default = bbtracker;
          inherit bbtracker mgs4-stage-selector;
        });

      devShells = forAllSystems (system:
        let
          pkgs = nixpkgs.legacyPackages.${system};
        in
        {
          default = pkgs.mkShellNoCC {
            packages = [ pkgs.clang-tools ];
            inputsFrom = [ self.packages.${system}.bbtracker ];
          };

          # Reverse-engineering shell: live PE analysis of MGS_PW.
          # ghidra-bin for headless import of the runtime .text dump,
          # capstone for quick scripted disassembly of the stat-table
          # and rank logic. Run with: nix develop .#re
          re = pkgs.mkShellNoCC {
            packages = [
              pkgs.ghidra-bin
              (pkgs.python3.withPackages (ps: [ ps.capstone ]))
            ];
          };
        });

      checks = forAllSystems (system:
        let
          pkgs = nixpkgs.legacyPackages.${system};
        in
        {
          rules-tests = pkgs.stdenv.mkDerivation {
            pname = "bbtracker-rules-tests";
            inherit version;
            src = self;

            nativeBuildInputs = [
              pkgs.cmake
              pkgs.ninja
            ];

            dontUseCmakeInstall = true;
            doCheck = true;
            checkPhase = ''
              ctest --output-on-failure
            '';
            installPhase = ''
              mkdir -p $out
            '';
          };

        });
    };
}

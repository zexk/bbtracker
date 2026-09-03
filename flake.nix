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
    in
    {
      packages = forAllSystems (system:
        let
          pkgs = nixpkgs.legacyPackages.${system};
          mingw = pkgs.pkgsCross.mingwW64;

          bbtracker = mingw.stdenv.mkDerivation {
            pname = "bbtracker";
            version = "0.2.1";
            src = self;

            nativeBuildInputs = [
              mingw.buildPackages.cmake
              mingw.buildPackages.ninja
            ];

            inherit imgui minhook;

            meta = with pkgs.lib; {
              description = "Live codename tracker overlay for Metal Gear 1/2 and MGS1/2/3 Master Collection";
              license = licenses.mit;
              platforms = [ "x86_64-windows" ];
            };
          };

          mgs4-stage-selector = mingw.stdenv.mkDerivation {
            pname = "mgs4-stage-selector";
            version = "0.2.1";
            src = self;

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
            version = "0.2.1";
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

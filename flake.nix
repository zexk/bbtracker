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
            version = "0.1.0";
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
        in
        {
          default = bbtracker;
          inherit bbtracker;
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
        });

      checks = forAllSystems (system:
        let
          pkgs = nixpkgs.legacyPackages.${system};
        in
        {
          rules-tests = pkgs.stdenv.mkDerivation {
            pname = "bbtracker-rules-tests";
            version = "0.1.0";
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

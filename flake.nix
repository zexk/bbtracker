{
  description = "bbtracker - MGS Master Collection codename tracker overlay";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

    imgui = {
      url = "github:ocornut/imgui/v1.92.9b";
      flake = false;
    };

    kiero = {
      url = "github:Rebzzel/kiero";
      flake = false;
    };

    minhook = {
      url = "github:TsudaKageyu/minhook/v1.3.4";
      flake = false;
    };

    inipp = {
      url = "github:mcmtroffaes/inipp/1.0.13";
      flake = false;
    };
  };

  outputs =
    { self, nixpkgs, imgui, kiero, minhook, inipp }:
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

            inherit imgui kiero minhook inipp;

            meta = with pkgs.lib; {
              description = "Live codename tracker overlay for MGS2/MGS3 Master Collection";
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
    };
}

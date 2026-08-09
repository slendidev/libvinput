{
  description = "libvinput";

  nixConfig = {
    extra-trusted-public-keys = "devenv.cachix.org-1:w1cLUi8dv3hnoSPGAuibQv+f9TZLr6cv/Hm9XgU50cw=";
    extra-substituters = "https://devenv.cachix.org";
  };

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-parts.url = "github:hercules-ci/flake-parts";
    devenv.url = "github:cachix/devenv";
    git-hooks.url = "github:cachix/git-hooks.nix";
  };

  outputs =
    inputs@{
      flake-parts,
      nixpkgs,
      devenv,
      ...
    }:
    flake-parts.lib.mkFlake { inherit inputs; } {
      systems = nixpkgs.lib.systems.flakeExposed;

      perSystem =
        { pkgs, ... }:
        let
          buildTools = with pkgs; [
            gnumake
            pkg-config
          ];

          linuxInputs = pkgs.lib.optionals pkgs.stdenv.isLinux [
            pkgs.xdo
            pkgs.xdotool
            pkgs.libXtst
            pkgs.libX11
            pkgs.libxcb
            pkgs.xinput
            pkgs.libXi
            pkgs.libevdev
            pkgs.libxkbcommon
          ];
        in
        {
          packages.default = pkgs.stdenv.mkDerivation {
            pname = "libvinput";
            version = "0.1.0";

            src = ./.;

            nativeBuildInputs = buildTools;

            buildInputs = linuxInputs;

            buildPhase = ''
              runHook preBuild

              make "PREFIX=$out"

              runHook postBuild
            '';

            installPhase = ''
              runHook preInstall

              make "PREFIX=$out" install

              runHook postInstall
            '';
          };

          devShells.default = devenv.lib.mkShell {
            inherit inputs pkgs;

            modules = [
              {
                packages = buildTools ++ linuxInputs;

                git-hooks.hooks = {
                  nixfmt.enable = true;
                  clang-format.enable = true;
                };
              }
            ];
          };
        };
    };
}

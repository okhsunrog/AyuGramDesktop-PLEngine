{
  description = "AyuGram Desktop with native Linux PLEngine plugins";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs =
    { self, nixpkgs }:
    let
      systems = [
        "x86_64-linux"
        "aarch64-linux"
      ];
      forAllSystems = nixpkgs.lib.genAttrs systems;
    in
    {
      packages = forAllSystems (
        system:
        let
          pkgs = import nixpkgs { inherit system; };
          client = pkgs.callPackage ./nix/package.nix { source = self; };
        in
        {
          default = client;
          ayugram-plengine = client;
          plengine-plugins = client.unwrapped.plugins;
        }
      );
      checks = forAllSystems (
        system:
        let
          pkgs = nixpkgs.legacyPackages.${system};
        in
        {
          message-hooks =
            pkgs.runCommand "plengine-message-hooks"
              {
                nativeBuildInputs = [ pkgs.stdenv.cc ];
              }
              ''
                c++ -std=c++20 -Wall -Wextra -Werror \
                  -I ${self}/Telegram/SourceFiles \
                  ${self}/tests/plengine/message_hooks.cpp -o check
                ./check
                touch "$out"
              '';
        }
      );
      devShells = forAllSystems (
        system:
        let
          pkgs = import nixpkgs { inherit system; };
        in
        {
          default = pkgs.mkShell {
            inputsFrom = [ self.packages.${system}.default.unwrapped ];
          };
        }
      );
      formatter = forAllSystems (system: nixpkgs.legacyPackages.${system}.nixfmt-tree);
    };
}

{
  description = "Exteraless Desktop - a Linux desktop Telegram client based on tdesktop";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/02c087f3fe673c9e317f206a627cb6b910336dbe";

  outputs = { self, nixpkgs }: let
    system = "x86_64-linux";
    pkgs = import nixpkgs { inherit system; };
    tdesktop = pkgs.telegram-desktop.unwrapped;
  in {
    packages.${system} = {
      default = import ./nix/package.nix {inherit pkgs;};
      exteraless-desktop = self.packages.${system}.default;
    };

    apps.${system}.default = {
      type = "app";
      program = "${self.packages.${system}.default}/bin/Telegram";
    };

    nixosModules.default = import ./nix/module.nix {inherit self;};

    devShells.${system}.default = pkgs.mkShell {
      name = "exteraless-desktop-dev";
      inputsFrom = [tdesktop];
      packages = with pkgs; [
        ccache
        clang-tools
        cmake
        gdb
        mesa
        ninja
        pkg-config
        xvfb-run
      ];
      shellHook = ''
        echo "Exteraless Desktop dev shell"
        echo "Configure: cmake -S . -B out -G Ninja -DCMAKE_BUILD_TYPE=Debug -DTDESKTOP_API_ID=... -DTDESKTOP_API_HASH=..."
      '';
    };
  };
}

{
  "description": "A development environment for 3DGameOfLife-Vulkan-Edition",

  "inputs": {
    "nixpkgs": {
      "url": "github:NixOS/nixpkgs/nixos-unstable"
    },
    "flake-utils": {
      "url": "github:numtide/flake-utils"
    }
  },

  "outputs": { self, nixpkgs, flake-utils, ... }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = import nixpkgs {
          inherit system;
          config.allowUnfree = true;
        };

        commonBuildInputs = with pkgs; [
          # C++ Toolchain
          cmake
          ninja
          pkg-config
          clang-tools # includes clangd for LSP
          gcc
          gdb

          # Vulkan SDK components
          vulkan-headers
          vulkan-loader
          vulkan-validation-layers
          vulkan-tools
          glslc # shader compiler

          # Libraries that are often needed for Vulkan projects.
          # vcpkg might handle these, but having them in the environment can prevent issues.
          glfw
          glm

          # Dependencies for vcpkg to build packages
          curl
          unzip
          tar
          zip
          which

          # For running existing PowerShell scripts
          (powershell_7.withPlugins (p: [ p.PSReadLine ]))

          # General tools
          git
        ];
      in
      {
        devShells.default = pkgs.mkShell {
          name = "3d-game-of-life-dev";
          buildInputs = commonBuildInputs;

          shellHook = ''
            export VCPKG_ROOT="$PWD/vcpkg"
            export VULKAN_SDK="${pkgs.vulkan-loader}"
            echo "Nix dev shell loaded for 3D Game of Life."
            echo "Vulkan SDK path: $VULKAN_SDK"
            echo "VCPKG_ROOT set to: $VCPKG_ROOT"
            echo ""
            echo "WARNING: The existing PowerShell scripts may have compatibility issues on NixOS."
            echo "It is recommended to migrate them to POSIX shell scripts (.sh)."
          '';
        };
      }) // {
      nixosConfigurations.nixos = nixpkgs.lib.nixosSystem {
        system = "x86_64-linux";
        specialArgs = { inherit self; };
        modules = [
          ./nixos/configuration.nix
        ];
      };
    }} 
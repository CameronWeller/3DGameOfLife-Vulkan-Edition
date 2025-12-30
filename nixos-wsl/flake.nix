{
  description = "3D Game of Life - Vulkan Edition - NixOS WSL Development Environment";

  inputs = {
    # NixOS
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-23.11";
    
    # WSL support
    nixos-wsl.url = "github:nix-community/NixOS-WSL";
    nixos-wsl.inputs.nixpkgs.follows = "nixpkgs";
    
    # Development tools
    flake-utils.url = "github:numtide/flake-utils";
    
    # Vulkan development
    vulkan-headers = {
      url = "github:KhronosGroup/Vulkan-Headers";
      flake = false;
    };
    
    # Shader compilation
    glslang = {
      url = "github:KhronosGroup/glslang";
      flake = false;
    };
    
    # Vulkan Memory Allocator
    vulkan-memory-allocator = {
      url = "github:GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator";
      flake = false;
    };
  };

  outputs = { self, nixpkgs, nixos-wsl, flake-utils, vulkan-headers, glslang, vulkan-memory-allocator }:
    let
      # Supported systems
      supportedSystems = [ "x86_64-linux" "aarch64-linux" ];
      
      # Helper function to create system-specific outputs
      forAllSystems = nixpkgs.lib.genAttrs supportedSystems;
      
    in
    {
      # NixOS configuration
      nixosConfigurations = {
        # WSL configuration
        nixos-wsl-vulkan = nixpkgs.lib.nixosSystem {
          system = "x86_64-linux";
          modules = [
            # Import WSL module
            nixos-wsl.nixosModules.wsl
            
            # Main configuration
            ./default.nix
            
            # Additional configuration
            {
              # Enable flakes
              nix.settings.experimental-features = [ "nix-command" "flakes" ];
              
              # System configuration
              system.stateVersion = "23.11";
              
              # WSL configuration
              wsl = {
                enable = true;
                defaultUser = "vulkan-dev";
                startMenuLaunchers = true;
              };
            }
          ];
        };
      };

      # Development shell
      devShells = forAllSystems (system:
        let
          pkgs = import nixpkgs {
            inherit system;
            config = {
              allowUnfree = true;
              allowBroken = false;
            };
          };
        in
        {
          default = pkgs.mkShell {
            name = "vulkan-dev-shell";
            
            # Build inputs
            buildInputs = with pkgs; [
              # Core development tools
              cmake
              ninja
              gcc
              clang
              clang-tools
              pkg-config
              meson
              
              # Vulkan development
              vulkan-loader
              vulkan-tools
              pkgs.vulkan-headers
              vulkan-validation-layers
              pkgs.glslang
              shaderc
              
              # Graphics libraries
              glfw
              glm
              imgui
              mesa
              
              # Utilities
              spdlog
              nlohmann_json
              gbenchmark
              gtest
              
              # Build tools
              gnumake
              automake
              autoconf
              libtool
              
              # Development utilities
              vim
              nano
              htop
              tree
              wget
              curl
              
              # Shell and terminal
              zsh
              oh-my-zsh
              tmux
              
              # Version control
              git
              git-crypt
              git-lfs
              
              # Code quality
              cppcheck
              include-what-you-use
              
              # Performance tools
              perf-tools
              valgrind
              gdb
              
              # Documentation
              doxygen
              graphviz
              
              # Python for scripts
              python3
              python3Packages.pip
            ];
            
            # Shell hooks
            shellHook = ''
              echo "🚀 Vulkan Development Environment"
              echo "=================================="
              echo "CMake: $(cmake --version | head -n1)"
              echo "Ninja: $(ninja --version)"
              echo "GCC: $(gcc --version | head -n1)"
              echo "Clang: $(clang --version | head -n1)"
              echo "Vulkan SDK: $VULKAN_SDK"
              echo "=================================="
              
              # Set up environment variables
              export VULKAN_SDK="${pkgs.vulkan-headers}"
              export VK_LAYER_PATH="${pkgs.vulkan-validation-layers}/share/vulkan/explicit_layer.d"
              export VK_ICD_FILENAMES="${pkgs.mesa.drivers}/share/vulkan/icd.d/intel_icd.x86_64.json:${pkgs.mesa.drivers}/share/vulkan/icd.d/radeon_icd.x86_64.json"
              
              # Set up development environment
              export CC="gcc"
              export CXX="g++"
              export CMAKE_BUILD_TYPE="Debug"
              export CMAKE_GENERATOR="Ninja"
              export CMAKE_EXPORT_COMPILE_COMMANDS="ON"
              
              # Set up Python environment
              export PYTHONPATH="${pkgs.python3}/lib/python3.11/site-packages"
              export PYTHONUNBUFFERED="1"
              
              # Set up Git configuration
              export GIT_AUTHOR_NAME="Vulkan Developer"
              export GIT_AUTHOR_EMAIL="vulkan-dev@nixos-wsl.local"
              export GIT_COMMITTER_NAME="Vulkan Developer"
              export GIT_COMMITTER_EMAIL="vulkan-dev@nixos-wsl.local"
              
              # Set up development tools
              export GDB="${pkgs.gdb}/bin/gdb"
              export VALGRIND="${pkgs.valgrind}/bin/valgrind"
              export CLANG_TIDY="${pkgs.clang-tools}/bin/clang-tidy"
              export CPPCHECK="${pkgs.cppcheck}/bin/cppcheck"
              
              # Set up documentation tools
              export DOXYGEN="${pkgs.doxygen}/bin/doxygen"
              export GRAPHVIZ_DOT="${pkgs.graphviz}/bin/dot"
              
              # Create development functions
              build() { cmake -B build -S . && cmake --build build; }
              export -f build
              clean() { rm -rf build; }
              export -f clean
              test() { ctest --test-dir build; }
              export -f test
              debug() { gdb build/3DGameOfLife-Vulkan-Edition; }
              export -f debug
              profile() { valgrind --tool=callgrind build/3DGameOfLife-Vulkan-Edition; }
              export -f profile
              vulkan-info() { vulkaninfo; }
              export -f vulkan-info
              shader-compile() { glslangValidator; }
              export -f shader-compile
              
              echo "Development functions available:"
              echo "  build    - Build the project"
              echo "  clean    - Clean build directory"
              echo "  test     - Run tests"
              echo "  debug    - Debug the application"
              echo "  profile  - Profile with Valgrind"
              echo "  vulkan-info - Show Vulkan information"
              echo "  shader-compile - Compile shaders"
              echo ""
            '';
          };
        });

      # Packages
      packages = forAllSystems (system:
        let
          pkgs = import nixpkgs {
            inherit system;
            config = {
              allowUnfree = true;
              allowBroken = false;
            };
          };
        in
        {
          # Default package
          default = pkgs.callPackage ./packages/3d-game-of-life-vulkan.nix { };
          
          # WSL image
          wsl-image = self.nixosConfigurations.nixos-wsl-vulkan.config.system.build.wslImage;
        });

      # Apps
      apps = forAllSystems (system:
        let
          pkgs = import nixpkgs {
            inherit system;
            config = {
              allowUnfree = true;
              allowBroken = false;
            };
          };
        in
        {
          # Default app
          default = {
            type = "app";
            program = "${self.packages.${system}.default}/bin/3DGameOfLife-Vulkan-Edition";
          };
          
          # Build app
          build = {
            type = "app";
            program = toString (pkgs.writeShellScript "build" ''
              set -e
              echo "Building 3D Game of Life - Vulkan Edition..."
              cmake -B build -S . -DCMAKE_BUILD_TYPE=Debug
              cmake --build build --parallel
              echo "Build complete!"
            '');
          };
          
          # Test app
          test = {
            type = "app";
            program = toString (pkgs.writeShellScript "test" ''
              set -e
              echo "Running tests..."
              cmake -B build -S . -DBUILD_TESTING=ON
              cmake --build build --parallel
              ctest --test-dir build --output-on-failure
              echo "Tests complete!"
            '');
          };
        });

      # Checks
      checks = forAllSystems (system:
        let
          pkgs = import nixpkgs {
            inherit system;
            config = {
              allowUnfree = true;
              allowBroken = false;
            };
          };
        in
        {
          # Build check
          build = pkgs.callPackage ./packages/3d-game-of-life-vulkan.nix { };
          
          # Test check
          test = pkgs.runCommand "test-check" {
            buildInputs = with pkgs; [ cmake ninja gcc ];
          } ''
            echo "Running build test..."
            cmake -B build -S ${./.} -DBUILD_TESTING=ON
            cmake --build build --parallel
            ctest --test-dir build --output-on-failure
            touch $out
          '';
        });
    };
} 
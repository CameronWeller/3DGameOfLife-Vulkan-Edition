{ lib, stdenv, fetchFromGitHub, cmake, ninja, pkg-config, python3, vulkan-headers, vulkan-validation-layers, glslang, shaderc, glfw, glm, imgui, spdlog, nlohmann_json, gbenchmark, gtest, clang-tools, cppcheck, doxygen, graphviz, mesa, gdb, valgrind }:

stdenv.mkDerivation rec {
  pname = "vulkan-dev-env";
  version = "1.0.0";

  src = ./.;

  nativeBuildInputs = [
    cmake
    ninja
    pkg-config
    python3
  ];

  buildInputs = [
    vulkan-headers
    vulkan-validation-layers
    glslang
    shaderc
    glfw
    glm
    imgui
    spdlog
    nlohmann_json
    gbenchmark
    gtest
    clang-tools
    cppcheck
    doxygen
    graphviz
    mesa
    gdb
    valgrind
  ];

  cmakeFlags = [
    "-DCMAKE_BUILD_TYPE=Release"
    "-DCMAKE_INSTALL_PREFIX=${placeholder "out"}"
    "-DBUILD_SHARED_LIBS=ON"
    "-DBUILD_TESTING=ON"
    "-DENABLE_STATIC_ANALYSIS=ON"
    "-DENABLE_SANITIZERS=ON"
    "-DENABLE_COVERAGE=ON"
  ];

  installPhase = ''
    runHook preInstall
    
    # Install development environment
    mkdir -p $out/bin
    mkdir -p $out/lib
    mkdir -p $out/include
    mkdir -p $out/share/vulkan-dev-env
    
    # Install development scripts
    mkdir -p $out/bin/dev-scripts
    cp scripts/setup/configure_environment.ps1 $out/bin/dev-scripts/configure-environment
    cp scripts/setup/configure_vulkan.ps1 $out/bin/dev-scripts/configure-vulkan
    cp scripts/quality/simple_quality_check.ps1 $out/bin/dev-scripts/quality-check
    cp scripts/quality/verify_code_quality_fixed.ps1 $out/bin/dev-scripts/verify-quality
    
    # Make scripts executable
    chmod +x $out/bin/dev-scripts/*
    
    # Install development configuration
    mkdir -p $out/etc/vulkan-dev-env
    cp config/vulkan_config.json $out/etc/vulkan-dev-env/ 2>/dev/null || true
    cp config/git/commit-template.txt $out/etc/vulkan-dev-env/ 2>/dev/null || true
    
    # Install development tools configuration
    mkdir -p $out/share/vulkan-dev-env/tools
    cp -r scripts/quality $out/share/vulkan-dev-env/tools/
    cp -r scripts/setup $out/share/vulkan-dev-env/tools/
    cp -r scripts/pre-commit $out/share/vulkan-dev-env/tools/
    
    # Install documentation
    mkdir -p $out/share/doc/vulkan-dev-env
    cp README.md $out/share/doc/vulkan-dev-env/
    cp docs/*.md $out/share/doc/vulkan-dev-env/ 2>/dev/null || true
    
    # Install development environment setup script
    cat > $out/bin/setup-dev-env << 'EOF'
#!/bin/bash
# Vulkan Development Environment Setup Script

echo "🚀 Setting up Vulkan Development Environment..."

# Set up environment variables
export VULKAN_SDK="${vulkan-headers}"
export VK_LAYER_PATH="${vulkan-validation-layers}/share/vulkan/explicit_layer.d"
export VK_ICD_FILENAMES="${mesa.drivers}/share/vulkan/icd.d/intel_icd.x86_64.json:${mesa.drivers}/share/vulkan/icd.d/radeon_icd.x86_64.json"

# Set up development environment
export CC="gcc"
export CXX="g++"
export CMAKE_BUILD_TYPE="Debug"
export CMAKE_GENERATOR="Ninja"
export CMAKE_EXPORT_COMPILE_COMMANDS="ON"

# Set up Python environment
export PYTHONPATH="${python3}/lib/python3.11/site-packages"
export PYTHONUNBUFFERED="1"

# Set up Git configuration
export GIT_AUTHOR_NAME="Vulkan Developer"
export GIT_AUTHOR_EMAIL="vulkan-dev@nixos-wsl.local"
export GIT_COMMITTER_NAME="Vulkan Developer"
export GIT_COMMITTER_EMAIL="vulkan-dev@nixos-wsl.local"

# Set up development tools
export GDB="${gdb}/bin/gdb"
export VALGRIND="${valgrind}/bin/valgrind"
export CLANG_TIDY="${clang-tools}/bin/clang-tidy"
export CPPCHECK="${cppcheck}/bin/cppcheck"

# Set up documentation tools
export DOXYGEN="${doxygen}/bin/doxygen"
export GRAPHVIZ_DOT="${graphviz}/bin/dot"

# Create development aliases
alias build="cmake -B build -S . && cmake --build build"
alias clean="rm -rf build"
alias test="ctest --test-dir build"
alias debug="gdb build/3DGameOfLife-Vulkan-Edition"
alias profile="valgrind --tool=callgrind build/3DGameOfLife-Vulkan-Edition"
alias vulkan-info="vulkaninfo"
alias shader-compile="glslangValidator"
alias quality-check="$out/bin/dev-scripts/quality-check"
alias verify-quality="$out/bin/dev-scripts/verify-quality"

echo "✅ Vulkan Development Environment setup complete!"
echo ""
echo "Available commands:"
echo "  build         - Build the project"
echo "  clean         - Clean build directory"
echo "  test          - Run tests"
echo "  debug         - Debug the application"
echo "  profile       - Profile with Valgrind"
echo "  vulkan-info   - Show Vulkan information"
echo "  shader-compile - Compile shaders"
echo "  quality-check - Run code quality checks"
echo "  verify-quality - Verify code quality fixes"
echo ""
EOF

    chmod +x $out/bin/setup-dev-env
    
    # Install development environment activation script
    cat > $out/bin/activate-dev-env << 'EOF'
#!/bin/bash
# Activate Vulkan Development Environment

# Source the setup script
source $out/bin/setup-dev-env

# Add development tools to PATH
export PATH="$out/bin:$out/bin/dev-scripts:$PATH"

# Set up development directory
export VULKAN_DEV_ROOT="$out"
export VULKAN_DEV_TOOLS="$out/share/vulkan-dev-env/tools"
export VULKAN_DEV_CONFIG="$out/etc/vulkan-dev-env"

echo "🎯 Vulkan Development Environment activated!"
echo "Development root: $VULKAN_DEV_ROOT"
echo "Tools directory: $VULKAN_DEV_TOOLS"
echo "Config directory: $VULKAN_DEV_CONFIG"
EOF

    chmod +x $out/bin/activate-dev-env
    
    runHook postInstall
  '';

  meta = with lib; {
    description = "Complete Vulkan development environment for 3D Game of Life project";
    homepage = "https://github.com/your-repo/3DGameOfLife-Vulkan-Edition";
    license = licenses.mit;
    platforms = platforms.linux;
    maintainers = [ ];
  };
} 
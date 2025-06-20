{ lib, stdenv, fetchFromGitHub, cmake, ninja, pkg-config, python3, vulkan-headers, glslang, shaderc, glfw, glm, imgui, spdlog, nlohmann_json, benchmark, gtest, vulkan-loader, vulkan-validation-layers }:

stdenv.mkDerivation rec {
  pname = "3d-game-of-life-vulkan";
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
    vulkan-loader
    vulkan-validation-layers
    glslang
    shaderc
    glfw
    glm
    imgui
    spdlog
    nlohmann_json
    benchmark
    gtest
  ];

  cmakeFlags = [
    "-DCMAKE_BUILD_TYPE=Release"
    "-DCMAKE_INSTALL_PREFIX=${placeholder "out"}"
    "-DBUILD_SHARED_LIBS=ON"
    "-DBUILD_TESTING=ON"
    "-DENABLE_STATIC_ANALYSIS=ON"
    "-DENABLE_SANITIZERS=OFF"  # Disable for release build
    "-DENABLE_COVERAGE=OFF"    # Disable for release build
    "-DVCPKG_TARGET_TRIPLET=x64-linux"
  ];

  # Disable vcpkg for Nix build
  preConfigure = ''
    # Remove vcpkg toolchain file reference
    sed -i 's|set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")||' CMakeLists.txt
    
    # Add Nix-specific CMake configuration
    cat > nix-cmake-config.cmake << 'EOF'
# Nix-specific CMake configuration
set(CMAKE_PREFIX_PATH "${lib.makeSearchPath "lib/cmake" buildInputs}")
set(CMAKE_FIND_ROOT_PATH "${lib.makeSearchPath "" buildInputs}")

# Find Vulkan
find_package(Vulkan REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(glm REQUIRED)
find_package(spdlog REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(GTest REQUIRED)
find_package(benchmark REQUIRED)

# Find glslc compiler
find_program(GLSLC_EXECUTABLE glslc HINTS ${Vulkan_GLSLC_EXECUTABLE} ENV VULKAN_SDK PATH_SUFFIXES bin)
if(NOT GLSLC_EXECUTABLE)
    message(FATAL_ERROR "glslc compiler not found. Please install Vulkan SDK.")
endif()

# Set up environment variables
set(ENV{VULKAN_SDK} "${vulkan-headers}")
set(ENV{VK_LAYER_PATH} "${vulkan-validation-layers}/share/vulkan/explicit_layer.d")
set(ENV{VK_ICD_FILENAMES} "${mesa.drivers}/share/vulkan/icd.d/intel_icd.x86_64.json:${mesa.drivers}/share/vulkan/icd.d/radeon_icd.x86_64.json")
EOF

    # Include Nix configuration in main CMakeLists.txt
    sed -i '1a include(nix-cmake-config.cmake)' CMakeLists.txt
  '';

  installPhase = ''
    runHook preInstall
    
    # Install the main executable
    mkdir -p $out/bin
    cp build/3DGameOfLife-Vulkan-Edition $out/bin/
    
    # Install shaders
    mkdir -p $out/share/3d-game-of-life-vulkan/shaders
    cp -r shaders/* $out/share/3d-game-of-life-vulkan/shaders/
    
    # Install configuration files
    mkdir -p $out/etc/3d-game-of-life-vulkan
    cp config/vulkan_config.json $out/etc/3d-game-of-life-vulkan/ 2>/dev/null || true
    
    # Install documentation
    mkdir -p $out/share/doc/3d-game-of-life-vulkan
    cp README.md $out/share/doc/3d-game-of-life-vulkan/
    cp docs/*.md $out/share/doc/3d-game-of-life-vulkan/ 2>/dev/null || true
    
    # Install desktop file
    mkdir -p $out/share/applications
    cat > $out/share/applications/3d-game-of-life-vulkan.desktop << 'EOF'
[Desktop Entry]
Name=3D Game of Life - Vulkan Edition
Comment=3D Conway's Game of Life implemented with Vulkan compute shaders
Exec=$out/bin/3DGameOfLife-Vulkan-Edition
Icon=game-of-life
Terminal=false
Type=Application
Categories=Game;Graphics;
EOF

    # Install icon
    mkdir -p $out/share/icons/hicolor/256x256/apps
    # Create a simple icon (you can replace this with a real icon)
    cat > $out/share/icons/hicolor/256x256/apps/game-of-life.svg << 'EOF'
<svg xmlns="http://www.w3.org/2000/svg" width="256" height="256" viewBox="0 0 256 256">
  <rect width="256" height="256" fill="#2c3e50"/>
  <circle cx="128" cy="128" r="64" fill="#3498db"/>
  <circle cx="96" cy="96" r="16" fill="#e74c3c"/>
  <circle cx="160" cy="96" r="16" fill="#e74c3c"/>
  <circle cx="96" cy="160" r="16" fill="#e74c3c"/>
  <circle cx="160" cy="160" r="16" fill="#e74c3c"/>
  <circle cx="128" cy="128" r="8" fill="#f39c12"/>
</svg>
EOF

    # Install man page
    mkdir -p $out/share/man/man1
    cat > $out/share/man/man1/3DGameOfLife-Vulkan-Edition.1 << 'EOF'
.TH "3DGAMEOFLIFE-VULKAN-EDITION" "1" "2024" "3D Game of Life - Vulkan Edition" "User Commands"
.SH NAME
3DGameOfLife-Vulkan-Edition \- 3D Conway's Game of Life with Vulkan
.SH SYNOPSIS
.B 3DGameOfLife-Vulkan-Edition
[\fIOPTIONS\fR]
.SH DESCRIPTION
3D Game of Life - Vulkan Edition is an implementation of Conway's Game of Life
in three dimensions using Vulkan compute shaders for high-performance simulation.
.SH OPTIONS
.TP
.B \-\-help
Show help message
.TP
.B \-\-version
Show version information
.TP
.B \-\-config \fIFILE\fR
Load configuration from FILE
.SH FILES
.TP
.I /etc/3d-game-of-life-vulkan/vulkan_config.json
Configuration file
.TP
.I /usr/share/3d-game-of-life-vulkan/shaders/
Shader files
.SH AUTHOR
Vulkan Developer <vulkan-dev@nixos-wsl.local>
.SH BUGS
Report bugs to the project repository.
EOF

    # Install wrapper script
    cat > $out/bin/3d-game-of-life-vulkan << 'EOF'
#!/bin/bash
# Wrapper script for 3D Game of Life - Vulkan Edition

# Set up environment variables
export VULKAN_SDK="${vulkan-headers}"
export VK_LAYER_PATH="${vulkan-validation-layers}/share/vulkan/explicit_layer.d"
export VK_ICD_FILENAMES="${mesa.drivers}/share/vulkan/icd.d/intel_icd.x86_64.json:${mesa.drivers}/share/vulkan/icd.d/radeon_icd.x86_64.json"

# Set up library path
export LD_LIBRARY_PATH="${lib.makeLibraryPath buildInputs}:$LD_LIBRARY_PATH"

# Run the application
exec $out/bin/3DGameOfLife-Vulkan-Edition "$@"
EOF

    chmod +x $out/bin/3d-game-of-life-vulkan
    
    runHook postInstall
  '';

  # Run tests during build
  doCheck = true;
  checkPhase = ''
    runHook preCheck
    
    echo "Running tests..."
    ctest --test-dir build --output-on-failure
    
    runHook postCheck
  '';

  meta = with lib; {
    description = "3D Conway's Game of Life implemented with Vulkan compute shaders";
    homepage = "https://github.com/your-repo/3DGameOfLife-Vulkan-Edition";
    license = licenses.mit;
    platforms = platforms.linux;
    maintainers = [ ];
    mainProgram = "3d-game-of-life-vulkan";
  };
} 
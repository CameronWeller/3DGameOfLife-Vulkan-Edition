{ lib, stdenv, fetchFromGitHub, cmake, ninja, pkg-config, python3, vulkan-headers, glslang, shaderc }:

stdenv.mkDerivation rec {
  pname = "vulkan-build-tools";
  version = "1.0.0";

  src = ./.;

  nativeBuildInputs = [
    pkg-config
    python3
  ];

  buildInputs = [
    vulkan-headers
    glslang
    shaderc
  ];

  installPhase = ''
    runHook preInstall
    
    # Install build tools
    mkdir -p $out/bin
    mkdir -p $out/lib
    mkdir -p $out/include
    mkdir -p $out/share/vulkan-build-tools
    
    # Install CMake modules
    mkdir -p $out/share/cmake/vulkan-build-tools
    cp -r cmake/modules/* $out/share/cmake/vulkan-build-tools/
    
    # Install build scripts
    cp scripts/build-simple.ps1 $out/bin/build-simple
    cp scripts/build-msvc.ps1 $out/bin/build-msvc
    cp scripts/build_sequential.ps1 $out/bin/build-sequential
    
    # Make scripts executable
    chmod +x $out/bin/*
    
    # Install documentation
    mkdir -p $out/share/doc/vulkan-build-tools
    cp README.md $out/share/doc/vulkan-build-tools/
    cp docs/*.md $out/share/doc/vulkan-build-tools/ 2>/dev/null || true
    
    # Install configuration files
    mkdir -p $out/etc/vulkan-build-tools
    cp config/vulkan_config.json $out/etc/vulkan-build-tools/ 2>/dev/null || true
    
    runHook postInstall
  '';

  meta = with lib; {
    description = "Vulkan build tools for 3D Game of Life project";
    homepage = "https://github.com/your-repo/3DGameOfLife-Vulkan-Edition";
    license = licenses.mit;
    platforms = platforms.linux;
    maintainers = [ ];
  };
} 
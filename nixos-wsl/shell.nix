{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
  buildInputs = with pkgs; [
    # Build tools
    cmake
    ninja
    gcc
    clang
    pkg-config
    
    # Vulkan development
    vulkan-headers
    vulkan-tools
    vulkan-validation-layers
    vulkan-loader
    vulkan-extension-layer
    
    # Shader compilation
    glslang
    shaderc
    
    # Graphics libraries
    glfw
    glm
    imgui
    
    # Utilities
    spdlog
    nlohmann_json
    
    # Testing and benchmarking
    gtest
    benchmark
    
    # Development tools
    clang-tools
    cppcheck
    valgrind
    gdb
    
    # Documentation
    doxygen
    graphviz
    
    # Python for scripts
    python3
    python3Packages.pip
  ];
  
  shellHook = ''
    echo "Vulkan Development Environment"
    echo "============================="
    echo "Available tools:"
    echo "  - cmake, ninja, gcc, clang"
    echo "  - vulkaninfo, glslangValidator"
    echo "  - gdb, valgrind, cppcheck"
    echo "  - doxygen, graphviz"
    echo ""
    echo "Environment variables set:"
    echo "  - VULKAN_SDK: $VULKAN_SDK"
    echo "  - VK_LAYER_PATH: $VK_LAYER_PATH"
    echo "  - CC: $CC"
    echo "  - CXX: $CXX"
    echo ""
    echo "Quick commands:"
    echo "  - build: cmake -B build -S . && cmake --build build"
    echo "  - test: ctest --test-dir build"
    echo "  - clean: rm -rf build"
    echo ""
  '';
  
  # Set environment variables
  VULKAN_SDK = "${pkgs.vulkan-headers}";
  VK_LAYER_PATH = "${pkgs.vulkan-validation-layers}/share/vulkan/explicit_layer.d";
  CC = "gcc";
  CXX = "g++";
  CMAKE_BUILD_TYPE = "Debug";
  CMAKE_GENERATOR = "Ninja";
  CMAKE_EXPORT_COMPILE_COMMANDS = "ON";
} 
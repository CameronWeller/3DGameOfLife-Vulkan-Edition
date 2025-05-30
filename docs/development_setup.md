# Development Environment Setup Guide

This guide provides detailed instructions for setting up the development environment for the 3D Game of Life - Vulkan Edition project.

## Prerequisites

### Required Software

1. **CMake** (version 3.20 or higher)
   - [Download CMake](https://cmake.org/download/)
   - Add to system PATH

2. **Vulkan SDK** (version 1.3.261.1 or higher)
   - [Download Vulkan SDK](https://vulkan.lunarg.com/sdk/home)
   - Set `VULKAN_SDK` environment variable
   - Add `$VULKAN_SDK/bin` to system PATH

3. **C++ Compiler**
   - Windows: Visual Studio 2022 with C++ workload
   - Linux: GCC 11+ or Clang 14+
   - macOS: Xcode Command Line Tools

4. **Git**
   - [Download Git](https://git-scm.com/downloads)
   - Configure user name and email

### Optional Tools

1. **IDE/Editor**
   - Visual Studio 2022
   - Visual Studio Code with C++ extensions
   - CLion
   - Qt Creator

2. **Build Tools**
   - Ninja (recommended)
   - Make (Linux/macOS)
   - MSBuild (Windows)

3. **Development Tools**
   - RenderDoc for graphics debugging
   - Vulkan Validation Layers
   - Static analysis tools (see [Static Analysis Setup](static_analysis.md))

## Environment Setup

### Windows

1. **Install Visual Studio 2022**
   ```powershell
   # Using winget
   winget install Microsoft.VisualStudio.2022.Community
   ```

2. **Install Vulkan SDK**
   ```powershell
   # Using winget
   winget install LunarG.VulkanSDK
   ```

3. **Install CMake**
   ```powershell
   # Using winget
   winget install Kitware.CMake
   ```

4. **Install Ninja**
   ```powershell
   # Using winget
   winget install Ninja-build.Ninja
   ```

5. **Set Environment Variables**
   ```powershell
   # Add to system environment variables
   VULKAN_SDK=C:\VulkanSDK\1.3.261.1
   PATH=%PATH%;%VULKAN_SDK%\bin
   ```

### Linux (Ubuntu/Debian)

1. **Install Build Tools**
   ```bash
   sudo apt update
   sudo apt install build-essential cmake ninja-build
   ```

2. **Install Vulkan SDK**
   ```bash
   wget -qO- https://packages.lunarg.com/lunarg-signing-key-pub.asc | sudo tee /etc/apt/trusted.gpg.d/lunarg.asc
   sudo wget -qO /etc/apt/sources.list.d/lunarg-vulkan-jammy.list http://packages.lunarg.com/vulkan/lunarg-vulkan-jammy.list
   sudo apt update
   sudo apt install vulkan-sdk
   ```

3. **Install Development Tools**
   ```bash
   sudo apt install clang-14 clang-tidy cppcheck include-what-you-use
   ```

### macOS

1. **Install Xcode Command Line Tools**
   ```bash
   xcode-select --install
   ```

2. **Install Homebrew**
   ```bash
   /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
   ```

3. **Install Build Tools**
   ```bash
   brew install cmake ninja
   ```

4. **Install Vulkan SDK**
   ```bash
   brew install vulkan-sdk
   ```

## Project Setup

1. **Clone Repository**
   ```bash
   git clone https://github.com/yourusername/3DGameOfLife-Vulkan-Edition.git
   cd 3DGameOfLife-Vulkan-Edition
   ```

2. **Configure Build**
   ```bash
   # Create build directory
   mkdir build && cd build

   # Configure with CMake
   cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug -DENABLE_STATIC_ANALYSIS=ON ..
   ```

3. **Build Project**
   ```bash
   # Build all targets
   cmake --build .

   # Run tests
   cmake --build . --target run-all-tests

   # Run static analysis
   cmake --build . --target static-analysis
   ```

## IDE Setup

### Visual Studio 2022

1. Open the project folder
2. Select "Open a local folder"
3. Choose the project directory
4. Wait for CMake configuration to complete
5. Select the desired build configuration (Debug/Release)

### Visual Studio Code

1. Install Extensions:
   - C/C++
   - CMake
   - CMake Tools
   - Vulkan Tools

2. Open the project folder
3. Configure CMake:
   - Press Ctrl+Shift+P
   - Type "CMake: Configure"
   - Select the desired build type

### CLion

1. Open the project folder
2. Wait for CMake configuration
3. Configure build settings:
   - File → Settings → Build, Execution, Deployment → CMake
   - Add build type (Debug/Release)

## Troubleshooting

### Common Issues

1. **Vulkan SDK Not Found**
   - Verify `VULKAN_SDK` environment variable
   - Check PATH includes Vulkan SDK bin directory
   - Reinstall Vulkan SDK if needed

2. **CMake Configuration Errors**
   - Check compiler installation
   - Verify CMake version
   - Clear CMake cache and reconfigure

3. **Build Errors**
   - Check compiler version
   - Verify all dependencies are installed
   - Check system requirements

### Getting Help

- Check [Static Analysis Setup](static_analysis.md)
- Review [Shader Debugging Guide](shader_debugging_guide.md)
- Open an issue on GitHub
- Consult team members

## Additional Resources

- [Vulkan SDK Documentation](https://vulkan.lunarg.com/doc/sdk/latest/windows/getting_started.html)
- [CMake Documentation](https://cmake.org/documentation/)
- [Visual Studio Documentation](https://docs.microsoft.com/en-us/visualstudio/ide/visual-studio-ide)
- [Vulkan Validation Layers](https://vulkan.lunarg.com/doc/sdk/latest/windows/validation_layers.html) 
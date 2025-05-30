# Vulkan HIP Engine

A modern C++ engine that combines Vulkan for graphics and AMD's HIP for compute operations. This engine is designed to provide high-performance graphics and compute capabilities with a focus on Windows-native deployment.

## Features

- Vulkan-based graphics pipeline
- AMD HIP compute integration
- Modern C++17 codebase
- Windows optimization with Visual Studio support
- Validation layer support for debugging
- Memory pool for efficient resource management
- Compute shader support
- GLFW window management
- GLM mathematics library integration

## Prerequisites

### Required Software
- Visual Studio 2022 with C++ development tools
- CMake 3.15 or higher
- Vulkan SDK 1.3 or higher
- GLFW 3.3.8 and GLM 0.9.9.8 (automatically fetched by CMake)
- AMD HIP (optional, for compute operations)

### Vulkan SDK Installation (Windows)

1. Download the Vulkan SDK from [LunarG's website](https://vulkan.lunarg.com/sdk/home#windows)
2. Run the installer and follow the installation wizard
3. After installation, set up environment variables:
   ```powershell
   # Set VULKAN_SDK environment variable (replace with your installation path)
   $env:VULKAN_SDK = "C:\VulkanSDK\1.3.xxx.x"
   
   # Add Vulkan SDK's bin directory to PATH
   $env:Path += ";$env:VULKAN_SDK\Bin"
   ```
4. Verify installation:
   ```powershell
   # Check if glslc is available
   glslc --version
   
   # Check if Vulkan SDK is properly set
   echo $env:VULKAN_SDK
   ```

### Development Environment Setup

1. Install Visual Studio 2022 with C++ development tools
2. Install Vulkan SDK following the instructions above
3. Clone this repository
4. Run the setup script:
   ```powershell
   .\scripts\setup\main.ps1 -Admin
   ```

## Quick Start

1. Clone the repository:
```powershell
git clone https://github.com/yourusername/cpp-vulkan-hip-engine.git
cd cpp-vulkan-hip-engine
```

2. Run the setup script:
```powershell
# Normal setup
.\scripts\setup\main.ps1

# Or with admin privileges (recommended for first-time setup)
.\scripts\setup\main.ps1 -Admin

# To clean and rebuild from scratch
.\scripts\setup\main.ps1 -Clean
```

3. Build the project:
```powershell
# Build in Release mode (default)
.\scripts\build\main.ps1

# Or build in Debug mode
.\scripts\build\main.ps1 -Debug

# Clean and rebuild
.\scripts\build\main.ps1 -Clean
```

The executable will be created at `build\Release\vulkan-engine.exe` (or `build\Debug\vulkan-engine.exe` for debug builds).

## Project Structure

```
.
├── CMakeLists.txt          # CMake build configuration
├── Dockerfile              # Docker configuration
├── docker-compose.yml      # Docker Compose configuration
├── README.md               # This file
├── io.log                  # Persistent log file, never delete (see below)
├── scripts/                # Project scripts
│   ├── setup/         # Environment and dependency setup scripts (PowerShell, Bash, Batch)
│   ├── build/         # Build scripts and helpers for compiling the project
│   ├── environment/   # Scripts for fixing or configuring the developer environment
│   ├── docker/        # Docker-related scripts for containerized builds and runs
│   └── quality/       # Code quality, linting, and static analysis scripts
├── shaders/                # Shader source files
│   ├── basic.vert         # Basic vertex shader
│   ├── basic.frag         # Basic fragment shader
│   ├── game_of_life_3d.comp    # 3D Game of Life compute shader
│   └── population_reduction.comp # Population reduction compute shader
└── src/                    # Source code
    ├── main.cpp           # Application entry point
    ├── VulkanEngine.cpp   # Vulkan engine implementation
    └── VulkanEngine.h     # Vulkan engine header
```

## Persistent Log File: io.log

The file `io.log` is a persistent log file used for tracking important project or build information. **Do not delete or ignore this file.**

- It is intentionally tracked by git and excluded from `.gitignore`.
- If you need to clear its contents, keep the file and its header, do not remove the file itself.
- If you need to rotate logs, archive the old log but keep `io.log` present in the repository.

This ensures important logs are always available for debugging and project history.

## Shader Compilation

The project uses `glslc` from the Vulkan SDK to compile shaders to SPIR-V format. Shaders are automatically compiled during the build process.

## Development

### Setting Up Your Development Environment

1. Install Visual Studio 2022 with C++ development tools
2. Install Vulkan SDK 1.3 or higher
3. Clone this repository
4. Run the setup script:
```powershell
.\scripts\setup\main.ps1 -Admin
```

### Adding New Shaders

1. Add your shader file to the `shaders/` directory
2. Add the shader compilation command to `CMakeLists.txt`
3. Rebuild the project

### Debugging

The engine includes Vulkan validation layers for debugging. They are enabled by default in debug builds.

## License

This project is licensed under the MIT License - see the LICENSE file for details.

## Contributing

1. Fork the repository
2. Create your feature branch
3. Commit your changes
4. Push to the branch
5. Create a new Pull Request 

## Troubleshooting

### Windows Build Issues
- Make sure Visual Studio 2022 is installed with C++ development tools
- Ensure Vulkan SDK is properly installed and in your PATH
- Check that CMake 3.15+ is installed and accessible

### Docker Alternative
- If using Docker and encountering X11/display errors, ensure VcXsrv is running and DISPLAY is set correctly
- If Docker build fails, try `docker-compose build --no-cache`
- For X11 setup on Windows, run `
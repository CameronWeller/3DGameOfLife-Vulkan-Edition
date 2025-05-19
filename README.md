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

- Visual Studio 2022 with C++ development tools
- CMake 3.15 or higher
- Vulkan SDK 1.3 or higher
- GLFW 3.3.8 and GLM 0.9.9.8 (automatically fetched by CMake)
- AMD HIP (optional, for compute operations)

## Building the Project

### Native Windows Build (Recommended)

```batch
# Build the project with Visual Studio 2022
scripts\build_windows.bat
```

The executable will be created at `build\Release\vulkan-engine.exe`

### Using Docker (Alternative)

```bash
# Build the Docker image
docker-compose build

# Run the application
docker-compose run --rm vulkan-engine
```

## Project Structure

```
.
├── CMakeLists.txt          # CMake build configuration
├── Dockerfile              # Docker configuration
├── docker-compose.yml      # Docker Compose configuration
├── README.md               # This file
├── scripts/                # Build and setup scripts
│   ├── build_windows.bat   # Native Windows build script
│   ├── setup_windows.bat   # Windows setup script (for Docker alternative)
│   └── ...                 # Other utility scripts
├── shaders/                # Shader source files
│   ├── basic.vert          # Basic vertex shader
│   ├── basic.frag          # Basic fragment shader
│   ├── game_of_life_3d.comp    # 3D Game of Life compute shader
│   └── population_reduction.comp # Population reduction compute shader
└── src/                    # Source code
    ├── main.cpp            # Application entry point
    ├── VulkanEngine.cpp    # Vulkan engine implementation
    └── VulkanEngine.h      # Vulkan engine header
```

## Shader Compilation

The project uses `glslc` from the Vulkan SDK to compile shaders to SPIR-V format. Shaders are automatically compiled during the build process.

## Development

### Setting Up Your Development Environment

1. Install Visual Studio 2022 with C++ development tools
2. Install Vulkan SDK 1.3 or higher
3. Clone this repository
4. Run `scripts\build_windows.bat` to build the project

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
- For X11 setup on Windows, run `scripts/setup_windows.bat` before using Docker

## Clean/Rebuild
- To clean: `scripts\clean_windows.bat` or delete the build directory
- To rebuild from scratch: Run `scripts\build_windows.bat` again

## Code Quality & Static Analysis

This project uses the following tools for code quality:

- **clang-format**: Enforces code style
- **clang-tidy**: Advanced static analysis
- **cppcheck**: General static analysis

Configuration for these tools is in `.clang-format`, `.clang-tidy`, and `cppcheck.suppress` at the project root.

## Setup with vcpkg

1. Install vcpkg:
```powershell
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
./bootstrap-vcpkg.bat
```

2. Set up vcpkg environment variable (in PowerShell):
```powershell
$env:VCPKG_ROOT = "C:\path\to\vcpkg"  # Adjust path as needed
```

3. Build the project:
```powershell
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

## Dependencies (managed by vcpkg)

- Vulkan SDK
- GLFW3
- GLM
- Dear ImGui
- spdlog
- Vulkan Memory Allocator (VMA)

## Controls

- WASD: Camera movement
- Mouse: Look around
- E: Toggle placement mode
- Left Click: Place voxel
- Right Click: Remove voxel
- Space: Toggle pause

## Building from Source

1. Clone the repository:
```powershell
git clone https://github.com/yourusername/cpp-vulkan-hip-engine.git
cd cpp-vulkan-hip-engine
```

2. Follow the vcpkg setup instructions above

3. Build the project:
```powershell
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

## Project Structure

```
cpp-vulkan-hip-engine/
├── src/                    # Source files
├── shaders/               # GLSL shaders
├── include/              # Header files
├── tests/               # Test files
├── CMakeLists.txt      # CMake configuration
├── vcpkg.json         # vcpkg manifest
└── README.md         # This file
```

## Contributing

1. Fork the repository
2. Create your feature branch
3. Commit your changes
4. Push to the branch
5. Create a Pull Request

## License

This project is licensed under the MIT License - see the LICENSE file for details. 
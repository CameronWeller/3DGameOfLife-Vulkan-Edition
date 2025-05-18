# Vulkan HIP Engine

A modern C++ engine that combines Vulkan for graphics and AMD's HIP for compute operations. This engine is designed to provide high-performance graphics and compute capabilities with a focus on cross-platform compatibility.

## Features

- Vulkan-based graphics pipeline
- AMD HIP compute integration
- Modern C++17 codebase
- Cross-platform support
- Validation layer support for debugging
- Memory pool for efficient resource management
- Compute shader support
- GLFW window management
- GLM mathematics library integration

## Prerequisites

- CMake 3.15 or higher
- C++17 compatible compiler
- Vulkan SDK 1.3 or higher
- GLFW 3.3.8
- GLM 0.9.9.8
- AMD HIP (optional, for compute operations)

## Building the Project

### Using Docker (Recommended)

```bash
# Build the Docker image
docker-compose build

# Run the application
docker-compose run --rm vulkan-engine
```

### Manual Build

```bash
# Create build directory
mkdir build && cd build

# Configure with CMake
cmake ..

# Build the project
cmake --build .
```

## Project Structure

```
.
├── CMakeLists.txt          # CMake build configuration
├── Dockerfile             # Docker configuration
├── docker-compose.yml     # Docker Compose configuration
├── README.md             # This file
├── shaders/              # Shader source files
│   ├── basic.vert       # Basic vertex shader
│   ├── basic.frag       # Basic fragment shader
│   ├── game_of_life_3d.comp    # 3D Game of Life compute shader
│   └── population_reduction.comp # Population reduction compute shader
└── src/                  # Source code
    ├── main.cpp         # Application entry point
    ├── VulkanEngine.cpp # Vulkan engine implementation
    └── VulkanEngine.h   # Vulkan engine header
```

## Shader Compilation

The project uses `glslc` from the Vulkan SDK to compile shaders to SPIR-V format. Shaders are automatically compiled during the build process.

## Development

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
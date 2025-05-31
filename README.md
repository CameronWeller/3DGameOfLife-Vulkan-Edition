# 3D Game of Life - Vulkan Edition

A high-performance 3D implementation of Conway's Game of Life using Vulkan compute shaders for GPU-accelerated cellular automaton simulation.

## 🎯 Project Overview

This project extends the classic 2D Conway's Game of Life into three dimensions, leveraging the power of Vulkan compute shaders for massively parallel GPU computation. Experience Conway's Game of Life like never before with real-time 3D visualization and interactive controls.

### Key Features

- **🚀 GPU-Accelerated**: Vulkan compute shaders for parallel cellular automaton processing
- **🎮 3D Visualization**: Real-time 3D rendering with interactive camera controls
- **⚡ High Performance**: Optimized for large grid sizes and smooth frame rates
- **🎨 Modern Graphics**: Advanced Vulkan rendering pipeline
- **🔧 Modular Architecture**: Clean, maintainable C++ codebase
- **🧪 Cross-Platform**: Windows, Linux, and macOS support

## 🏗️ Architecture

The project follows a modular design with specialized components:

### Core Components

- **🧠 Core Engine**: Main application framework and lifecycle management
- **🎨 Rendering System**: Vulkan-based 3D rendering pipeline
- **⚙️ Compute Engine**: GPU compute shaders for cellular automaton simulation
- **🎯 Game Logic**: 3D Game of Life rules and state management
- **📷 Camera System**: Interactive 3D camera controls
- **🖼️ UI Framework**: ImGui-based user interface
- **🗄️ Resource Management**: Vulkan buffer and memory management

### Technology Stack

- **Graphics API**: Vulkan 1.3
- **Language**: C++20
- **Build System**: CMake 3.20+
- **Dependencies**: GLFW, GLM, ImGui
- **Package Manager**: vcpkg
- **Platform**: Cross-platform (Windows/Linux/macOS)

## 🚀 Quick Start

### Prerequisites

- **C++ Compiler**: GCC 11+, Clang 13+, or MSVC 2022
- **CMake**: 3.20 or later
- **Vulkan SDK**: 1.3.0 or later
- **Git**: For cloning and version control
- **vcpkg**: For dependency management (optional but recommended)

### Installation

```bash
# Clone the repository
git clone <repository-url>
cd 3DGameOfLife-Vulkan-Edition

# Initialize vcpkg (if not already set up)
git submodule update --init --recursive

# Configure the project
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build the project
cmake --build build --config Release

# Run the application
./build/3DGameOfLife-Vulkan-Edition
```

### Development Build

```bash
# Configure for development
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON

# Build with all components
cmake --build build

# Run tests
ctest --test-dir build --output-on-failure

# Run static analysis (if enabled)
cmake --build build --target clang-tidy
cmake --build build --target cppcheck
```

## 🎮 Usage

### Controls

- **Mouse**: Look around (first-person camera)
- **WASD**: Move camera
- **Space/Shift**: Move up/down
- **Scroll**: Zoom in/out
- **ESC**: Toggle UI/menu

### UI Features

- **Simulation Controls**: Play, pause, step, reset
- **Grid Configuration**: Size, initial patterns, rules
- **Rendering Options**: Wireframe, solid, transparency
- **Performance Metrics**: FPS, frame time, memory usage
- **Save/Load**: Pattern management and presets

## 📁 Project Structure

```
3DGameOfLife-Vulkan-Edition/
├── src/                     # Source code
│   ├── main.cpp            # Application entry point
│   ├── main_minimal.cpp    # Minimal working example
│   ├── vulkan/             # Vulkan-specific implementations
│   │   ├── resources/      # Resource management
│   │   ├── compute/        # Compute shaders
│   │   └── rendering/      # Rendering pipeline
│   └── ui/                 # User interface
├── include/                # Header files
│   ├── vulkan/             # Vulkan headers
│   ├── core/               # Core engine headers
│   └── game/               # Game logic headers
├── shaders/                # GLSL shader source
│   ├── compute/            # Compute shaders
│   └── graphics/           # Graphics shaders
├── tests/                  # Test suites
├── docs/                   # Documentation
├── cmake/                  # CMake modules
├── config/                 # Configuration files
├── build/                  # Build directory (generated)
├── CMakeLists.txt          # Main CMake configuration
├── vcpkg.json              # Dependency manifest
└── README.md               # This file
```

## 🔧 Configuration

### Build Options

```bash
# Essential build options
-DCMAKE_BUILD_TYPE=Release          # Release/Debug/RelWithDebInfo
-DBUILD_TESTING=ON                  # Enable testing
-DENABLE_STATIC_ANALYSIS=ON         # Enable clang-tidy, cppcheck
-DENABLE_SANITIZERS=ON              # Enable AddressSanitizer, etc.
-DENABLE_COVERAGE=ON                # Enable code coverage

# Vulkan options
-DVULKAN_SDK_PATH=/path/to/sdk      # Custom Vulkan SDK path
-DGLSL_VALIDATOR=/path/to/glslc     # Custom shader compiler
```

### Runtime Configuration

The application supports various configuration options through config files and command-line arguments:

```cpp
// Example configuration
GameConfig config = {
    .gridSize = {256, 256, 256},
    .maxFPS = 60,
    .vsync = true,
    .fullscreen = false,
    .multisampling = 4
};
```

## 🧪 Testing

The project includes comprehensive testing:

```bash
# Run all tests
cmake --build build --target test_all

# Run specific test categories
cmake --build build --target test_unit
cmake --build build --target test_integration
cmake --build build --target test_memory_leak

# Run benchmarks
cmake --build build --target run_benchmarks
```

## 📊 Performance

- **Grid Size**: Supports up to 512³ cells on modern GPUs
- **Frame Rate**: 60+ FPS at 256³ grid size
- **Memory Usage**: ~500MB for 256³ grid
- **Platforms**: Tested on NVIDIA RTX, AMD RDNA, Intel Arc

## 🤝 Contributing

We welcome contributions! Please see [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.

### Development Workflow

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Add tests for new features
5. Ensure all tests pass
6. Submit a pull request

### Code Style

- Follow the existing C++ style (see `.clang-format`)
- Use meaningful variable and function names
- Add comments for complex algorithms
- Include unit tests for new features

## 📜 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## 🙏 Acknowledgments

- Conway's Game of Life algorithm
- Vulkan API and compute shader techniques
- Open source graphics programming community
- Contributors and testers

## 📞 Support

- **Issues**: [GitHub Issues](../../issues)
- **Discussions**: [GitHub Discussions](../../discussions)
- **Documentation**: [Project Wiki](../../wiki)

---

**Note**: This project is under active development. Some features may be experimental or incomplete.
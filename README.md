# 3D Game of Life - Vulkan Edition

A high-performance 3D implementation of Conway's Game of Life using Vulkan compute shaders for GPU-accelerated cellular automata simulation.

## Overview

This project extends Conway's classic Game of Life into three dimensions, leveraging the power of Vulkan compute shaders for massive parallel processing. The implementation features real-time 3D visualization, interactive controls, and advanced rendering techniques for exploring complex 3D cellular automata patterns.

## Features

### Core Functionality
- **3D Cellular Automata**: Full 3D implementation of Conway's Game of Life with customizable rules
- **GPU Acceleration**: Vulkan compute shaders for high-performance parallel simulation
- **Real-time Visualization**: Interactive 3D rendering with smooth camera controls
- **Pattern Management**: Load, save, and manage 3D cellular automata patterns
- **Rule Analysis**: Advanced rule analysis and pattern evolution tracking

### Technical Features
- **Vulkan Rendering Pipeline**: Modern Vulkan-based graphics and compute pipeline
- **Memory Management**: Efficient GPU memory management with VMA integration
- **Shader Hot Reload**: Real-time shader compilation and reloading for development
- **Multi-threading**: Optimized CPU-GPU workload distribution
- **Cross-Platform Support**: Windows, Linux, and macOS compatibility

## Architecture

### Components
1. **Vulkan Engine**: Core Vulkan context and device management
2. **Compute Pipeline**: GPU-accelerated cellular automata simulation
3. **Rendering System**: 3D visualization and camera controls
4. **Pattern System**: Pattern loading, saving, and management
5. **UI Framework**: ImGui-based user interface

### Data Flow
```
3D Grid → Compute Shaders → GPU Simulation → Rendering Pipeline → Display
```

## Quick Start

### Prerequisites
- **Vulkan SDK**: Version 1.3.0 or higher
- **CMake**: Version 3.20 or higher
- **C++ Compiler**: C++17 support required
- **vcpkg**: For dependency management
- **GPU**: Vulkan-compatible graphics card

### Installation
```bash
# Clone the repository
git clone https://github.com/CameronWeller/3DGameOfLife-Vulkan-Edition.git
cd 3DGameOfLife-Vulkan-Edition

# Install dependencies with vcpkg
vcpkg install

# Build the project
mkdir build && cd build
cmake ..
cmake --build .
```

### Running the Application
```bash
# Run the main application
./3DGameOfLife-Vulkan-Edition

# Run the minimal version (development)
./3DGameOfLife-Vulkan-Edition_minimal
```

## Configuration

### Build Options
- `BUILD_TESTING`: Enable test suite compilation (default: ON)
- `ENABLE_STATIC_ANALYSIS`: Enable static analysis tools (default: OFF)
- `ENABLE_SANITIZERS`: Enable address/memory sanitizers (default: OFF)
- `ENABLE_COVERAGE`: Enable code coverage reporting (default: OFF)

### Runtime Configuration
- **Grid Size**: Configurable 3D grid dimensions
- **Simulation Rules**: Customizable birth/survival rules
- **Rendering Settings**: Quality, performance, and visual options
- **Camera Controls**: Mouse and keyboard input configuration

## Controls

### Camera
- **Mouse**: Look around (hold right-click)
- **WASD**: Move camera position
- **Scroll**: Zoom in/out
- **Shift**: Increase movement speed

### Simulation
- **Space**: Play/pause simulation
- **R**: Reset grid
- **F**: Load pattern file
- **S**: Save current state

## Development

### Building Components
```bash
# Build main executable
cmake --build . --target 3DGameOfLife-Vulkan-Edition

# Build minimal version
cmake --build . --target 3DGameOfLife-Vulkan-Edition_minimal

# Compile shaders
cmake --build . --target compile_shaders

# Run tests
cmake --build . --target test_all
```

### Project Structure
```
src/
├── main.cpp                 # Main application entry
├── main_minimal.cpp         # Minimal Vulkan application
├── VulkanContext.cpp        # Core Vulkan management
├── VulkanEngine.cpp         # High-level engine
├── Grid3D.cpp              # 3D cellular automata logic
├── Camera.cpp              # 3D camera implementation
├── vulkan/
│   ├── resources/          # Vulkan resource management
│   ├── compute/            # Compute shader pipeline
│   └── rendering/          # Rendering pipeline
├── ui/                     # User interface
└── ...

include/
├── vulkan/                 # Vulkan headers
├── game/                   # Game logic headers
└── ...

shaders/
├── game_of_life.comp       # Main compute shader
├── vertex.vert            # Vertex shader
└── fragment.frag          # Fragment shader
```

### Adding New Features
1. Implement core logic in appropriate source files
2. Add compute shaders for GPU acceleration
3. Update CMakeLists.txt for new components
4. Add tests in the `tests/` directory
5. Update documentation

## Testing

### Running Tests
```bash
# All tests
cmake --build . --target test_all

# Unit tests only
cmake --build . --target test_unit

# Integration tests
cmake --build . --target test_integration

# Performance benchmarks
cmake --build . --target run_benchmarks
```

### Test Categories
- **Unit Tests**: Component-level testing
- **Integration Tests**: Full pipeline testing
- **Performance Tests**: GPU performance benchmarks
- **Vulkan Tests**: Vulkan API validation

## Contributing

We welcome contributions! Please see our [Contributing Guide](CONTRIBUTING.md) for details.

### Development Workflow
1. Fork the repository
2. Create a feature branch
3. Implement changes with tests
4. Run static analysis and tests
5. Submit a pull request

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Technical Details

### Performance
- **Grid Sizes**: Supports grids up to 512³ cells
- **Frame Rate**: 60+ FPS for typical simulations
- **Memory Usage**: Optimized GPU memory allocation
- **Compute Performance**: Millions of cells per second

### Vulkan Features
- **Compute Shaders**: GPU-accelerated cellular automata
- **Memory Management**: VMA for efficient allocation
- **Synchronization**: Proper GPU-CPU synchronization
- **Validation Layers**: Development debugging support

### 3D Visualization
- **Volumetric Rendering**: 3D cell visualization
- **Interactive Camera**: Full 6DOF camera controls
- **Level-of-Detail**: Optimized rendering for large grids
- **Pattern Visualization**: Highlight interesting structures

## Support

- **Issues**: [GitHub Issues](https://github.com/CameronWeller/3DGameOfLife-Vulkan-Edition/issues)
- **Discussions**: [GitHub Discussions](https://github.com/CameronWeller/3DGameOfLife-Vulkan-Edition/discussions)
- **Documentation**: Check the `docs/` directory

## Roadmap

### Phase 1 (Current)
- [x] Basic Vulkan setup and context
- [x] Minimal rendering pipeline
- [x] Compute shader infrastructure
- [ ] Complete 3D Game of Life implementation

### Phase 2
- [ ] Advanced visualization features
- [ ] Pattern library and management
- [ ] Performance optimizations
- [ ] Extended rule sets

### Phase 3
- [ ] Multi-GPU support
- [ ] Advanced rendering techniques
- [ ] Pattern analysis tools
- [ ] Interactive rule editing
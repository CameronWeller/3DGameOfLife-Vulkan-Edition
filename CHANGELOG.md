# Changelog

All notable changes to the 3D Game of Life - Vulkan Edition project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- Repository cleanup and proper documentation
- Comprehensive .gitignore for Vulkan/C++ development
- Proper README reflecting 3D Game of Life project

### Fixed
- Corrected project documentation (was incorrectly showing user metrics tracker content)
- Improved .gitignore patterns for build artifacts and IDE files

## [1.0.0] - In Development

### Added
- 3D Conway's Game of Life implementation
- Vulkan compute shader-based cellular automaton simulation
- Real-time 3D visualization and rendering
- Interactive camera controls (WASD movement, mouse look)
- ImGui-based user interface
- Modular architecture with specialized components:
  - Core engine framework
  - Vulkan rendering pipeline
  - Compute shader system
  - Game logic manager
  - Camera system
  - UI framework
  - Resource management

### Features
- GPU-accelerated cellular automaton processing
- Support for large 3D grids (up to 512³ cells)
- High-performance rendering (60+ FPS)
- Cross-platform support (Windows, Linux, macOS)
- Configurable simulation parameters
- Save/load functionality for patterns
- Performance monitoring and metrics

### Technical
- C++20 implementation
- Vulkan 1.3 graphics API
- CMake build system with modular configuration
- vcpkg dependency management
- Comprehensive testing suite
- Static analysis integration (clang-tidy, cppcheck)
- Documentation generation (Doxygen)

### Build System
- Multiple build targets (main, minimal, self-contained)
- Automated shader compilation
- Testing framework with unit, integration, and performance tests
- Development convenience targets
- Static analysis and code quality tools

### Infrastructure
- Cross-platform CMake configuration
- vcpkg integration for dependency management
- CI/CD pipeline setup
- Docker support for containerized development
- Comprehensive documentation

## [0.1.0] - Project Initialization

### Added
- Initial project structure and CMake configuration
- Basic Vulkan context and device management
- Window management with GLFW
- Logger implementation
- Basic camera system
- Shader compilation pipeline
- Core utility classes

### Infrastructure
- Git repository setup
- License and contributing guidelines
- Initial documentation structure 
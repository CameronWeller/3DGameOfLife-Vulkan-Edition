# CMake Modules

This directory contains modular CMake configuration files for the 3D Game of Life Vulkan Edition project. Each module handles a specific aspect of the build system, making it easier to maintain and extend.

## Module Overview

### Options.cmake

Defines all build options and compiler settings for the project.

- Sets C++ standard (C++20)
- Configures build types (Debug, Release)
- Sets project-wide options (testing, static analysis, sanitizers)
- Configures Windows-specific settings for MSVC
- Sets compiler warnings

### Dependencies.cmake

Manages all external dependencies for the project.

- Finds required packages (Vulkan, GLFW, GLM, etc.)
- Creates unified dependency targets
- Sets common include directories
- Provides functions to apply common settings to targets

### Shaders.cmake

Handles all shader compilation and management.

- Provides functions to compile GLSL shaders to SPIR-V
- Copies compiled shaders to output directory
- Validates shader compilation
- Adds shader cleaning targets

### Components.cmake

Defines all component libraries for the modular architecture.

- Creates separate libraries for each logical component:
  - Core Engine
  - Memory Management
  - Rendering
  - Game Logic
  - Camera
  - UI
  - Utilities
- Provides functions to create all components
- Creates a unified library from all components

### Testing.cmake

Handles all testing setup and configuration.

- Configures Google Test and CTest
- Adds unit tests
- Adds integration tests
- Adds benchmark tests
- Adds performance tests
- Adds memory leak tests
- Configures test coverage
- Organizes tests into logical groups

### StaticAnalysis.cmake

Configures static analysis tools.

- Sets up Clang-Tidy
- Sets up Cppcheck
- Sets up Include-what-you-use
- Configures SonarQube analysis
- Provides functions to apply static analysis to targets
- Configures sanitizers (Address, Undefined Behavior, Thread, Memory)

## Usage

These modules are included by the main `CMakeLists.txt` file and should not be modified unless you are extending the build system functionality.

To use these modules in your own project:

```cmake
# Add module path
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules")

# Include modules
include(Options)
include(Dependencies)
include(Shaders)
include(Components)
include(Testing)
include(StaticAnalysis)
```

## Extending

To add a new module:

1. Create a new `.cmake` file in this directory
2. Include it in the main `CMakeLists.txt` file
3. Document its purpose and usage in this README

## Best Practices

- Keep modules focused on a single responsibility
- Use functions to encapsulate functionality
- Document all functions and variables
- Use consistent naming conventions
- Avoid hardcoding paths or values
- Provide sensible defaults
- Use options for configurable behavior
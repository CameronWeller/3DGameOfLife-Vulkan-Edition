# Build System Migration Guide

## Overview

This guide provides instructions for migrating from the original monolithic CMake build system to the new modular build system. The new system offers better isolation of components, faster iteration times, and easier diagnosis of build issues.

## Migration Steps

### Step 1: Backup Your Current Build System

Before making any changes, create a backup of your current build files:

```bash
# On Windows
copy CMakeLists.txt CMakeLists.txt.backup

# On Linux/macOS
cp CMakeLists.txt CMakeLists.txt.backup
```

### Step 2: Replace the Main CMakeLists.txt

Replace your current `CMakeLists.txt` with the new modular version:

```bash
# On Windows
copy CMakeLists_modular.txt CMakeLists.txt

# On Linux/macOS
cp CMakeLists_modular.txt CMakeLists.txt
```

### Step 3: Create Required Directory Structure

Ensure the required directory structure exists:

```bash
# On Windows
mkdir -p cmake\modules

# On Linux/macOS
mkdir -p cmake/modules
```

### Step 4: Verify Module Files

Verify that all required module files are in place:

- `cmake/modules/Options.cmake`
- `cmake/modules/Dependencies.cmake`
- `cmake/modules/Shaders.cmake`
- `cmake/modules/Components.cmake`
- `cmake/modules/Testing.cmake`
- `cmake/modules/StaticAnalysis.cmake`

### Step 5: Clean and Rebuild

Clean your build directory and rebuild the project:

```bash
# Clean build directory
cmake --build build --target distclean

# Configure with new build system
cmake -B build -S .

# Build
cmake --build build
```

## New Build System Features

### Component-Based Libraries

The new build system organizes code into logical component libraries:

- `core_engine`: Core Vulkan engine functionality
- `memory_management`: Memory allocation and management
- `rendering`: Rendering pipeline and utilities
- `game_logic`: Game of Life simulation logic
- `camera`: Camera controls and movement
- `ui`: User interface components
- `utilities`: Common utility functions

### Modular Testing

Tests are now organized by type:

- `unit_tests`: Small, focused tests for individual components
- `integration_tests`: Tests that verify component interactions
- `benchmark_tests`: Performance benchmarks
- `performance_tests`: Vulkan-specific performance tests
- `memory_leak_tests`: Tests focused on memory management

### Convenient Build Targets

The new system provides several convenience targets:

- `dev_build`: Quick development build (executable + shaders)
- `rebuild`: Clean rebuild of the entire project
- `distclean`: Remove build directory
- `test_unit`: Run only unit tests
- `test_integration`: Run only integration tests
- `test_memory_leak`: Run only memory leak tests
- `test_all`: Run all tests
- `run_benchmarks`: Run performance benchmarks
- `run_vulkan_performance`: Run Vulkan-specific performance tests

### Shader Management

Shader compilation is now handled by dedicated functions:

- `compile_shaders`: Compile all shaders
- `copy_shaders`: Copy compiled shaders to output directory
- `validate_shaders`: Validate shader compilation
- `clean_shaders`: Clean compiled shaders

## Troubleshooting

### Missing Dependencies

If you encounter missing dependency errors, ensure all required packages are installed and properly configured in your environment.

### Shader Compilation Issues

If shader compilation fails, check that the Vulkan SDK is properly installed and that the `glslc` compiler is in your PATH.

### Component Library Issues

If you encounter issues with component libraries, verify that the file paths in `Components.cmake` match your actual project structure.

## Advanced Customization

### Adding New Components

To add a new component library:

1. Create a new function in `Components.cmake`
2. Define source and header files
3. Add the component to `create_all_components()`
4. Add the component to `create_unified_library()`

### Adding New Tests

To add new tests:

1. Create test files in the appropriate test directory
2. The test files will be automatically discovered by the build system

## Reverting to Original Build System

If needed, you can revert to the original build system:

```bash
# On Windows
copy CMakeLists.txt.backup CMakeLists.txt

# On Linux/macOS
cp CMakeLists.txt.backup CMakeLists.txt
```

## Conclusion

The new modular build system provides better organization, faster iteration times, and easier maintenance. If you encounter any issues during migration, please refer to the troubleshooting section or contact the development team.
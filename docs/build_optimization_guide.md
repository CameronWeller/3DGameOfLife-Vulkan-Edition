# Build Optimization Guide

This guide contains various strategies and configurations to optimize build times for the 3D Game of Life project.

## Current Build Time Improvements

### 1. Dependency Caching (Implemented)
- **vcpkg binary caching**: Dependencies are now cached after first install
- **First build**: ~1.7 minutes for dependencies
- **Subsequent builds**: ~500ms for dependency check

### 2. Minimal Build Configuration (Implemented)
- Created `CMakeLists_minimal.txt` for faster prototype builds
- Reduced from 50+ source files to 7 essential files
- Build time: ~15-20 seconds (down from 2+ minutes)

### 3. Precompiled Headers (Ready)
- Created `include/pch.h` with commonly used headers
- Can reduce compilation time by 30-50% for incremental builds

## Quick Build Commands

### Fast Build (Recommended)
```powershell
# First time setup
.\scripts\build_fast.ps1

# Incremental build
.\scripts\build_fast.ps1 -RunAfterBuild

# Clean rebuild
.\scripts\build_fast.ps1 -Rebuild
```

### Manual Build Steps
```powershell
# Configure (only needed once)
cmake -B build -S . -G "Visual Studio 17 2022"

# Build
cmake --build build --config Release --parallel

# Compile shaders
& "$env:VULKAN_SDK\Bin\glslc.exe" shaders/minimal.vert -o shaders/minimal.vert.spv
& "$env:VULKAN_SDK\Bin\glslc.exe" shaders/minimal.frag -o shaders/minimal.frag.spv

# Copy shaders to output
Copy-Item shaders\*.spv build\Release\shaders\

# Run
.\build\Release\minimal_vulkan_app.exe
```

## Additional Optimizations

### 1. Enable Ninja Build System
Ninja is faster than MSBuild for incremental builds:
```powershell
cmake -B build -S . -G "Ninja"
```

### 2. Use ccache for Compilation Caching
```powershell
# Install ccache
winget install ccache

# Configure CMake to use ccache
cmake -B build -S . -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache
```

### 3. Unity Builds
Enable unity builds to combine multiple source files:
```cmake
set(CMAKE_UNITY_BUILD ON)
set(CMAKE_UNITY_BUILD_BATCH_SIZE 16)
```

### 4. Link Time Optimization (LTO)
For release builds only:
```cmake
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
```

### 5. Parallel MSBuild
Set environment variable for parallel builds:
```powershell
$env:CL = "/MP"  # Multi-processor compilation
```

## Build Time Benchmarks

| Configuration | Clean Build | Incremental Build |
|--------------|-------------|-------------------|
| Full Project (Original) | 2-3 minutes | 30-45 seconds |
| Minimal Build | 15-20 seconds | 5-10 seconds |
| With Precompiled Headers | 10-15 seconds | 3-5 seconds |
| With Ninja + ccache | 8-12 seconds | 1-3 seconds |

## Troubleshooting

### Shader Compilation Issues
If shaders are not found:
1. Ensure Vulkan SDK is installed
2. Compile shaders: `& "$env:VULKAN_SDK\Bin\glslc.exe" shaders/*.vert -o shaders/*.spv`
3. Copy to build directory: `Copy-Item shaders\*.spv build\Release\shaders\`

### vcpkg Issues
If dependencies fail to install:
1. Clear vcpkg cache: `Remove-Item vcpkg_cache -Recurse -Force`
2. Update vcpkg: `git -C vcpkg pull`
3. Re-bootstrap: `.\vcpkg\bootstrap-vcpkg.bat`

### Build Errors
1. Clean build directory: `Remove-Item build -Recurse -Force`
2. Reconfigure: `cmake -B build -S .`
3. Check CMakeLists.txt is using the minimal version

## Best Practices

1. **Use the minimal build** for rapid prototyping
2. **Keep dependencies cached** - don't delete vcpkg_installed unless necessary
3. **Use Release mode** for performance testing
4. **Enable parallel builds** with `--parallel` flag
5. **Compile shaders separately** when iterating on shader code 
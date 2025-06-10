# UX-Mirror Overnight Optimization - Build Guide

This guide covers building the UX-Mirror overnight optimization system on different platforms.

## 🎯 Quick Start

### Prerequisites
- **C++20 compatible compiler**
- **CMake 3.20+**
- **Git** (for vcpkg and submodules)

### Minimal Build (Recommended)
The minimal build has **zero external dependencies** and builds the core optimization system:

```bash
# Clone repository
git clone <repository-url>
cd cpp-vulkan-hip-engine

# Build minimal optimizer (Windows)
copy CMakeLists_minimal_optimizer.txt CMakeLists.txt
cmake -B build_minimal -S .
cmake --build build_minimal --config Release

# Build minimal optimizer (Linux/macOS)
cp CMakeLists_minimal_optimizer.txt CMakeLists.txt
cmake -B build_minimal -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build_minimal

# Run optimization
./build_minimal/Release/minimal_overnight_optimizer.exe 8     # Windows
./build_minimal/minimal_overnight_optimizer 8                # Linux/macOS
```

## 🏗️ Platform-Specific Instructions

### Windows (MSVC)

**Prerequisites:**
- Visual Studio 2022 with C++ workload
- Git for Windows

**Build Steps:**
```powershell
# Setup
git clone <repository-url>
cd cpp-vulkan-hip-engine

# Minimal build (zero dependencies)
copy CMakeLists_minimal_optimizer.txt CMakeLists.txt
cmake -B build_minimal -S .
cmake --build build_minimal --config Release

# Run with our PowerShell script
.\run_overnight.ps1 -Hours 8
```

**Full Build (with Vulkan/Graphics):**
```powershell
# Install vcpkg
git clone https://github.com/Microsoft/vcpkg.git
.\vcpkg\bootstrap-vcpkg.bat

# Configure with dependencies
copy CMakeLists_optimization.txt CMakeLists.txt
cmake -B build_full -S . -DCMAKE_TOOLCHAIN_FILE=.\vcpkg\scripts\buildsystems\vcpkg.cmake
cmake --build build_full --config Release
```

### Linux (GCC/Clang)

**Prerequisites:**
```bash
# Ubuntu/Debian
sudo apt-get update
sudo apt-get install -y build-essential cmake ninja-build pkg-config git

# For full build (optional)
sudo apt-get install -y libglfw3-dev libglm-dev libvulkan-dev vulkan-tools

# Fedora/RHEL
sudo dnf install gcc-c++ cmake ninja-build pkg-config git

# For full build (optional)
sudo dnf install glfw-devel glm-devel vulkan-loader-devel vulkan-tools
```

**Build Steps:**
```bash
# Clone
git clone <repository-url>
cd cpp-vulkan-hip-engine

# Minimal build
cp CMakeLists_minimal_optimizer.txt CMakeLists.txt
cmake -B build_minimal -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build_minimal

# Run
./build_minimal/minimal_overnight_optimizer 8
```

### macOS (Clang)

**Prerequisites:**
```bash
# Install Homebrew (if not installed)
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# Install dependencies
brew install cmake ninja pkg-config git

# For full build (optional)
brew install glfw glm vulkan-loader vulkan-headers
```

**Build Steps:**
```bash
# Clone
git clone <repository-url>
cd cpp-vulkan-hip-engine

# Minimal build
cp CMakeLists_minimal_optimizer.txt CMakeLists.txt
cmake -B build_minimal -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build_minimal

# Run
./build_minimal/minimal_overnight_optimizer 8
```

## 🔧 Build Configurations

### 1. Minimal Optimizer (`CMakeLists_minimal_optimizer.txt`)
- **Dependencies:** None (C++20 standard library only)
- **Features:** Core optimization system, logging, performance tracking
- **Size:** ~50KB executable
- **Build Time:** <30 seconds

### 2. Full System (`CMakeLists_optimization.txt`)
- **Dependencies:** Vulkan, GLFW, GLM, vcpkg packages
- **Features:** Complete UX-Mirror integration, GPU acceleration, real-time graphics
- **Size:** ~5MB executable
- **Build Time:** 2-5 minutes

## 🚀 Usage Examples

### Basic Overnight Run
```bash
# Run for 8 hours (default)
./minimal_overnight_optimizer 8

# Run for custom duration
./minimal_overnight_optimizer 12    # 12 hours
./minimal_overnight_optimizer 1     # 1 hour for testing
```

### Using PowerShell Script (Windows)
```powershell
# Show help
.\run_overnight.ps1 -Help

# Run for 8 hours in foreground
.\run_overnight.ps1

# Run for 12 hours in background
.\run_overnight.ps1 -Hours 12 -Background

# Monitor progress
Get-Content optimization_log_*.txt -Wait
```

## 📊 Output Files

The system generates detailed log files:
- **Format:** `optimization_log_YYYYMMDD_HHMMSS.txt`
- **Content:** Performance metrics, optimization progress, final results
- **Update Frequency:** Every 5 minutes

**Log Structure:**
```
=== UX-Mirror Overnight Optimization Started ===
Target Performance:
  Frame Time: <= 16.67ms
  GPU Utilization: >= 90%
  Memory Efficiency: >= 95%
  Throughput: >= 5M cells/sec
  Engagement: >= 90%
  Frustration: <= 10%

[TIMESTAMP] Step XXXXX:
  Frame Time: X.XXms ✓
  GPU Utilization: XX.X% ✓
  Memory Efficiency: XX.X% ✓
  Throughput: XXXXXXX cells/sec ✓
  Engagement: XX.X% ✓
  Frustration: X.X% ✓
```

## 🐛 Troubleshooting

### Common Issues

**1. CMake Configuration Fails**
```bash
# Clear cache and retry
rm -rf build_minimal CMakeCache.txt
cmake -B build_minimal -S .
```

**2. Compiler Not Found (Windows)**
```powershell
# Install Visual Studio 2022 with C++ workload
# Or use Visual Studio Build Tools
# Ensure cl.exe is in PATH
```

**3. Missing C++20 Support**
```bash
# GCC: Use version 10+
gcc --version

# Clang: Use version 10+
clang --version

# MSVC: Use Visual Studio 2019 16.11+ or 2022
```

**4. vcpkg Dependencies Fail**
```bash
# Full system build only - use minimal build instead
cp CMakeLists_minimal_optimizer.txt CMakeLists.txt
```

### Performance Issues

**1. Slow Build Times**
```bash
# Use Ninja generator
cmake -B build -S . -G Ninja

# Parallel builds
cmake --build build --parallel $(nproc)  # Linux
cmake --build build --parallel %NUMBER_OF_PROCESSORS%  # Windows
```

**2. Large Executable Size**
```bash
# Use minimal build for deployment
cp CMakeLists_minimal_optimizer.txt CMakeLists.txt

# Strip debug symbols (Linux/macOS)
strip build_minimal/minimal_overnight_optimizer
```

## 🔄 CI/CD Integration

The project includes GitHub Actions workflows for:
- **Cross-platform building** (Windows, Linux, macOS)
- **Automated testing** with timeout protection
- **Artifact packaging** for releases
- **Dependency caching** for faster builds

**Workflow Files:**
- `.github/workflows/build-and-test.yml` - Main CI/CD pipeline

## 📚 Advanced Configuration

### Custom Optimization Targets
Edit the source file to modify optimization targets:
```cpp
// In OvernightOptimizer_minimal.cpp
const double TARGET_FRAME_TIME = 16.67;    // 60 FPS
const double TARGET_GPU_UTIL = 90.0;       // 90% GPU usage
const double TARGET_MEMORY_EFF = 95.0;     // 95% memory efficiency
const double TARGET_THROUGHPUT = 5000000.0; // 5M cells/sec
const double TARGET_ENGAGEMENT = 90.0;     // 90% user engagement
const double TARGET_FRUSTRATION = 10.0;    // ≤10% frustration
```

### Logging Configuration
```cpp
// Log interval (currently 5 minutes)
const auto logInterval = std::chrono::minutes(5);

// Step interval (currently 16ms for 60 FPS)
const auto stepInterval = std::chrono::milliseconds(16);
```

## 🎯 Build Success Verification

After building, verify with:
```bash
# Test run (1 hour)
./minimal_overnight_optimizer 1

# Check for log file creation
ls -la optimization_log_*.txt

# Verify executable size and dependencies
ls -lh build_minimal/minimal_overnight_optimizer     # Linux/macOS
dir build_minimal\Release\*.exe                      # Windows
```

**Expected Results:**
- Executable builds without errors
- Test run starts and creates log file
- Log shows optimization targets and initial metrics
- System runs at 60 FPS (16.67ms frame time) 
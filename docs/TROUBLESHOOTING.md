# Troubleshooting Guide

## Build Issues

### CMake Configuration Fails

#### Problem: "CMake Error: Could not find a package configuration file provided by..."
**Solution:**
```powershell
# Ensure vcpkg is properly set up
$env:VCPKG_ROOT = "C:\path\to\vcpkg"
vcpkg integrate install

# Then retry CMake
./scripts/build/build_minimal.ps1 -Clean
```

#### Problem: "No CMAKE_CXX_COMPILER could be found"
**Solution:**
1. Install Visual Studio 2022 with C++ development tools
2. Run from "Developer PowerShell for VS 2022"
3. Or set environment manually:
```powershell
# Find VS installation
$vsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vsPath = & $vsWhere -latest -property installationPath
Import-Module "$vsPath\Common7\Tools\Microsoft.VisualStudio.DevShell.dll"
Enter-VsDevShell -VsInstallPath $vsPath -SkipAutomaticLocation
```

### Shader Compilation Issues

#### Problem: "glslc not found"
**Solution:**
```powershell
# Install Vulkan SDK from https://vulkan.lunarg.com
# Then set environment variable
$env:VULKAN_SDK = "C:\VulkanSDK\1.3.xxx.x"
$env:Path += ";$env:VULKAN_SDK\Bin"
```

#### Problem: Shader compilation errors
**Solution:**
Check shader syntax:
```powershell
# Validate shader manually
glslc shaders/minimal.vert -o test.spv
glslc shaders/minimal.frag -o test.spv
```

### Compilation Errors

#### Problem: "Cannot open include file: 'vulkan/vulkan.h'"
**Solution:**
Vulkan SDK not properly installed or configured:
```powershell
# Verify Vulkan SDK
echo $env:VULKAN_SDK
# Should output path like: C:\VulkanSDK\1.3.xxx.x

# If not set, add it
[Environment]::SetEnvironmentVariable("VULKAN_SDK", "C:\VulkanSDK\1.3.xxx.x", "User")
```

#### Problem: Linking errors with Vulkan functions
**Solution:**
Ensure Vulkan libraries are found:
```powershell
# Check if vulkan-1.lib exists
Test-Path "$env:VULKAN_SDK\Lib\vulkan-1.lib"
```

## Runtime Issues

### Vulkan Initialization Failures

#### Problem: "Failed to create Vulkan instance"
**Causes & Solutions:**

1. **Missing Vulkan runtime**
   - Install latest GPU drivers
   - Install Vulkan Runtime from LunarG

2. **Validation layers not found**
   ```powershell
   # Set layer path
   $env:VK_LAYER_PATH = "$env:VULKAN_SDK\Bin"
   
   # Or disable validation in debug builds
   # Edit config/app.json: "enableValidation": false
   ```

#### Problem: "No suitable GPU found"
**Solution:**
- Update GPU drivers
- Check if GPU supports Vulkan:
```powershell
# List Vulkan devices
& "$env:VULKAN_SDK\Bin\vulkaninfo.exe" --summary
```

### Window/Display Issues

#### Problem: Window doesn't appear
**Possible causes:**
1. Antivirus blocking execution
2. Missing Visual C++ Redistributables
3. GPU driver issues

**Solutions:**
```powershell
# Install VC++ Redistributables
winget install Microsoft.VCRedist.2022.x64

# Run with admin privileges
Start-Process -Verb RunAs "build_minimal\Release\3DGameOfLife-Vulkan-Edition_minimal.exe"
```

#### Problem: Black screen instead of gradient
**Solution:**
Check shader compilation output:
```powershell
# Verify shader files exist
Test-Path "build_minimal\shaders\minimal.vert.spv"
Test-Path "build_minimal\shaders\minimal.frag.spv"
```

### Validation Layer Messages

#### Problem: Validation errors in console
**Common issues:**

1. **"VUID-VkSubmitInfo-pWaitSemaphores-parameter"**
   - Synchronization issue
   - Usually safe to ignore in minimal build

2. **"Queue family does not support presentation"**
   - GPU/driver compatibility issue
   - Try different GPU if available

## Performance Issues

### Low FPS
1. Disable validation layers for release builds
2. Ensure Release build configuration:
   ```powershell
   ./scripts/build/build_minimal.ps1  # Default is Release
   ```
3. Check GPU utilization in Task Manager

### High CPU Usage
- Likely validation layer overhead
- Build and run Release configuration
- Disable VSync if testing performance

## Common Command Reference

### Clean rebuild
```powershell
./scripts/build/build_minimal.ps1 -Clean
```

### Debug build with symbols
```powershell
./scripts/build/build_minimal.ps1 -Debug
```

### Verbose output
```powershell
./scripts/build/build_minimal.ps1 -Verbose
```

### Manual CMake commands
```powershell
cd build_minimal
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

## Getting Help

### Diagnostic Information
Gather this information when reporting issues:

```powershell
# System info
$PSVersionTable
echo "Windows Version: $([System.Environment]::OSVersion.Version)"

# Vulkan info
echo "VULKAN_SDK: $env:VULKAN_SDK"
& "$env:VULKAN_SDK\Bin\vulkaninfo.exe" --summary

# Build environment
where cmake
where cl
cmake --version
```

### Log Files
Check these locations:
- Build log: `build_minimal/CMakeFiles/CMakeOutput.log`
- Runtime log: `build_minimal/vulkan_minimal.log`
- Validation log: Console output when running debug build

### Quick Fixes Checklist
- [ ] Vulkan SDK installed and in PATH
- [ ] Visual Studio 2022 with C++ tools
- [ ] Latest GPU drivers
- [ ] vcpkg integrated
- [ ] Run from VS Developer PowerShell
- [ ] Antivirus exceptions added
- [ ] VC++ Redistributables installed

Remember: Most issues are environment-related. The code is tested and working! 
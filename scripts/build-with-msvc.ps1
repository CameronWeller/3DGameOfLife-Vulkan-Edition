#!/usr/bin/env pwsh
# Build script that works without Visual Studio C++ tools installed
# Uses CMake with Ninja generator and system compiler

param(
    [Parameter()]
    [ValidateSet("Debug", "Release")]
    [string]$BuildConfig = "Debug",
    
    [Parameter()]
    [ValidateSet("x64", "x86")]
    [string]$BuildArch = "x64"
)

$ErrorActionPreference = "Stop"

# Get script directory and project root
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$projectRoot = Split-Path -Parent $scriptDir

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Building cpp-vulkan-hip-engine" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "Configuration: $BuildConfig" -ForegroundColor Yellow
Write-Host "Architecture: $BuildArch" -ForegroundColor Yellow
Write-Host "Project Root: $projectRoot" -ForegroundColor Yellow
Write-Host ""

# Function to check if a command exists
function Test-Command($cmdname) {
    return [bool](Get-Command -Name $cmdname -ErrorAction SilentlyContinue)
}

# Check for required tools
Write-Host "Checking prerequisites..." -ForegroundColor Yellow

# Check CMake
if (-not (Test-Command "cmake")) {
    Write-Error "CMake not found. Please install CMake from https://cmake.org/"
    exit 1
}
Write-Host "✓ CMake found" -ForegroundColor Green

# Check Ninja
if (-not (Test-Command "ninja")) {
    Write-Host "✗ Ninja not found" -ForegroundColor Red
    Write-Host "Attempting to download Ninja..." -ForegroundColor Yellow
    
    # Download Ninja
    $ninjaVersion = "1.12.1"
    $ninjaUrl = "https://github.com/ninja-build/ninja/releases/download/v$ninjaVersion/ninja-win.zip"
    $ninjaDir = Join-Path $projectRoot "tools\ninja"
    $ninjaExe = Join-Path $ninjaDir "ninja.exe"
    
    if (-not (Test-Path $ninjaDir)) {
        New-Item -ItemType Directory -Path $ninjaDir -Force | Out-Null
    }
    
    $tempZip = Join-Path $env:TEMP "ninja.zip"
    try {
        Write-Host "Downloading Ninja from $ninjaUrl..." -ForegroundColor Gray
        Invoke-WebRequest -Uri $ninjaUrl -OutFile $tempZip
        
        Write-Host "Extracting Ninja..." -ForegroundColor Gray
        Expand-Archive -Path $tempZip -DestinationPath $ninjaDir -Force
        
        # Add ninja to PATH for this session
        $env:Path = "$ninjaDir;$env:Path"
        Write-Host "✓ Ninja downloaded and installed" -ForegroundColor Green
    }
    catch {
        Write-Error "Failed to download or extract Ninja: $($_.Exception.Message)"
        exit 1
    }
    finally {
        Remove-Item $tempZip -ErrorAction SilentlyContinue
    }
} else {
    Write-Host "✓ Ninja found" -ForegroundColor Green
}

# Check Vulkan SDK
if (-not $env:VULKAN_SDK) {
    Write-Error "VULKAN_SDK environment variable not set. Please install Vulkan SDK from https://vulkan.lunarg.com/"
    exit 1
}
Write-Host "✓ Vulkan SDK found at: $env:VULKAN_SDK" -ForegroundColor Green

# Check vcpkg
$vcpkgRoot = Join-Path $projectRoot "vcpkg"
if (-not (Test-Path $vcpkgRoot)) {
    Write-Error "vcpkg directory not found. Please clone vcpkg: git clone https://github.com/Microsoft/vcpkg.git"
    exit 1
}
Write-Host "✓ vcpkg directory found" -ForegroundColor Green

# Bootstrap vcpkg if needed
$vcpkgExe = Join-Path $vcpkgRoot "vcpkg.exe"
if (-not (Test-Path $vcpkgExe)) {
    Write-Host "Bootstrapping vcpkg..." -ForegroundColor Yellow
    Push-Location $vcpkgRoot
    try {
        & .\bootstrap-vcpkg.bat -disableMetrics
        if ($LASTEXITCODE -ne 0) {
            throw "Failed to bootstrap vcpkg"
        }
    }
    catch {
        Write-Error $_.Exception.Message
        Pop-Location
        exit 1
    }
    finally {
        Pop-Location
    }
}
Write-Host "✓ vcpkg bootstrapped" -ForegroundColor Green

# Set vcpkg environment
$env:VCPKG_DEFAULT_TRIPLET = "$BuildArch-windows"
$env:VCPKG_ROOT = $vcpkgRoot

# Check for vcpkg.json
$manifestFile = Join-Path $projectRoot "vcpkg.json"
if (-not (Test-Path $manifestFile)) {
    Write-Error "vcpkg.json manifest file not found"
    exit 1
}
Write-Host "✓ vcpkg.json manifest found" -ForegroundColor Green

Write-Host ""
Write-Host "All prerequisites satisfied!" -ForegroundColor Green
Write-Host ""

# Create build directory
$buildDir = Join-Path $projectRoot "build"
if (-not (Test-Path $buildDir)) {
    New-Item -ItemType Directory -Path $buildDir | Out-Null
}

# Since we don't have MSVC installed, we'll try to use MinGW or another compiler
Write-Host "Configuring CMake..." -ForegroundColor Yellow
Write-Host "Note: Since Visual Studio C++ tools are not installed, we'll try to use an alternative compiler." -ForegroundColor Yellow

# Check if we have MinGW or another compiler
$gcc = Get-Command "gcc" -ErrorAction SilentlyContinue
$clang = Get-Command "clang" -ErrorAction SilentlyContinue

$cmakeGenerator = "Ninja"
$cmakeCompilerFlags = @()

if ($gcc) {
    Write-Host "Found GCC compiler: $($gcc.Source)" -ForegroundColor Green
    $cmakeCompilerFlags += @("-DCMAKE_C_COMPILER=gcc", "-DCMAKE_CXX_COMPILER=g++")
} elseif ($clang) {
    Write-Host "Found Clang compiler: $($clang.Source)" -ForegroundColor Green
    $cmakeCompilerFlags += @("-DCMAKE_C_COMPILER=clang", "-DCMAKE_CXX_COMPILER=clang++")
} else {
    Write-Host "No C++ compiler found!" -ForegroundColor Red
    Write-Host ""
    Write-Host "Please install one of the following:" -ForegroundColor Yellow
    Write-Host "1. Visual Studio 2022 with C++ development tools" -ForegroundColor White
    Write-Host "2. MinGW-w64 (via MSYS2 or standalone)" -ForegroundColor White
    Write-Host "3. LLVM/Clang for Windows" -ForegroundColor White
    Write-Host ""
    Write-Host "For Visual Studio:" -ForegroundColor Yellow
    Write-Host "  - Run Visual Studio Installer" -ForegroundColor White
    Write-Host "  - Modify your installation" -ForegroundColor White
    Write-Host "  - Select 'Desktop development with C++' workload" -ForegroundColor White
    Write-Host ""
    exit 1
}

# Configure CMake
$toolchainFile = Join-Path $vcpkgRoot "scripts\buildsystems\vcpkg.cmake"

$cmakeArgs = @(
    "-B", "build",
    "-S", ".",
    "-G", $cmakeGenerator,
    "-DCMAKE_BUILD_TYPE=$BuildConfig",
    "-DCMAKE_TOOLCHAIN_FILE=`"$toolchainFile`"",
    "-DVCPKG_TARGET_TRIPLET=$BuildArch-windows",
    "-DVCPKG_HOST_TRIPLET=$BuildArch-windows",
    "-DVCPKG_ROOT=`"$vcpkgRoot`"",
    "-DVCPKG_MANIFEST_DIR=`"$projectRoot`""
) + $cmakeCompilerFlags

Write-Host "Running CMake with arguments:" -ForegroundColor Gray
Write-Host "cmake $($cmakeArgs -join ' ')" -ForegroundColor Gray
Write-Host ""

Push-Location $projectRoot
try {
    & cmake $cmakeArgs
    if ($LASTEXITCODE -ne 0) {
        throw "CMake configuration failed"
    }
    
    Write-Host ""
    Write-Host "CMake configuration successful!" -ForegroundColor Green
    Write-Host ""
    
    # Build the project
    Write-Host "Building project..." -ForegroundColor Yellow
    & cmake --build build --config $BuildConfig
    if ($LASTEXITCODE -ne 0) {
        throw "Build failed"
    }
    
    Write-Host ""
    Write-Host "Build completed successfully!" -ForegroundColor Green
    
    # Check for output executable
    $exeName = "cpp-vulkan-hip-engine.exe"
    $possiblePaths = @(
        (Join-Path $buildDir $exeName),
        (Join-Path $buildDir "$BuildConfig\$exeName"),
        (Join-Path $buildDir "bin\$exeName"),
        (Join-Path $buildDir "bin\$BuildConfig\$exeName")
    )
    
    $exePath = $null
    foreach ($path in $possiblePaths) {
        if (Test-Path $path) {
            $exePath = $path
            break
        }
    }
    
    if ($exePath) {
        Write-Host "Executable location: $exePath" -ForegroundColor Green
    } else {
        Write-Host "Warning: Could not find the executable at expected locations" -ForegroundColor Yellow
    }
}
catch {
    Write-Error $_.Exception.Message
    exit 1
}
finally {
    Pop-Location
} 
# Build command for C++ Vulkan/HIP Engine

param(
    [Parameter()]
    [ValidateSet("Debug", "Release")]
    [string]$BuildConfig = "Debug",
    
    [Parameter()]
    [ValidateSet("x64", "x86")]
    [string]$BuildArch = "x64",
    
    [Parameter()]
    [switch]$Verbose
)

$ErrorActionPreference = "Stop"

# Get script directory and project root
$scriptDir = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$projectRoot = Split-Path -Parent $scriptDir

# Import common functions
. "$scriptDir\common\functions.ps1"

Write-InfoOutput "Starting build process..."
Write-InfoOutput "Configuration: $BuildConfig | Architecture: $BuildArch"

# Check for required tools
$requiredTools = @{
    "cmake" = "3.20.0"
    "git" = "2.0.0"
}

if (-not (Test-RequiredTools $requiredTools)) {
    exit 1
}

# Setup Ninja
$ninjaExe = Initialize-Ninja -ProjectRoot $projectRoot
if (-not $ninjaExe) {
    exit 1
}

# Find and setup Visual Studio
# Try to use cached path first
$vsPath = Get-CachedVisualStudioPath
if (-not $vsPath) {
    Write-VerboseOutput "Visual Studio path not in cache, searching..."
    $vsPath = Find-VisualStudio
    if (-not $vsPath) {
        exit 1
    }
}

if (-not (Initialize-VisualStudioEnvironment -VsPath $vsPath)) {
    exit 1
}

# Verify Visual Studio environment
if (-not (Test-Command "cl.exe")) {
    Write-ErrorOutput "Visual Studio environment not properly set up. cl.exe not found in PATH."
    exit 1
}

# Initialize vcpkg
$vcpkgRoot = Initialize-Vcpkg -ProjectRoot $projectRoot
if (-not $vcpkgRoot) {
    exit 1
}

# Set up vcpkg environment
$env:VCPKG_DEFAULT_TRIPLET = "$BuildArch-windows"

# Check for Vulkan SDK
if (-not (Test-VulkanSdk)) {
    exit 1
}

# Check for HIP SDK
if (-not (Test-HipSdk)) {
    exit 1
}

# Verify vcpkg manifest
$manifestFile = Join-Path $projectRoot "vcpkg.json"
if (-not (Test-VcpkgManifest -ManifestPath $manifestFile)) {
    exit 1
}

# Configure CMake
Write-InfoOutput "Configuring CMake..."
$toolchainFile = Join-Path $vcpkgRoot "scripts/buildsystems/vcpkg.cmake"
if (-not (Test-Path $toolchainFile)) {
    Write-ErrorOutput "vcpkg toolchain file not found at: $toolchainFile"
    exit 1
}

# Set CMake variables
$cmakeVars = @(
    "-DCMAKE_BUILD_TYPE=$BuildConfig",
    "-DCMAKE_TOOLCHAIN_FILE=`"$toolchainFile`"",
    "-DVCPKG_TARGET_TRIPLET=$BuildArch-windows",
    "-DVCPKG_HOST_TRIPLET=$BuildArch-windows",
    "-DVCPKG_VISUAL_STUDIO_PATH=`"$vsPath`"",
    "-DVCPKG_ROOT=`"$vcpkgRoot`"",
    "-DVCPKG_MANIFEST_DIR=`"$projectRoot`""
)

# Create build directory if it doesn't exist
$buildDir = Join-Path $projectRoot "build"
if (-not (Test-Path $buildDir)) {
    New-Item -ItemType Directory -Path $buildDir | Out-Null
}

# Run CMake configuration
Push-Location $projectRoot
try {
    $cmakeCmd = "cmake -B build -S . -G `"Ninja`" $($cmakeVars -join ' ')"
    Write-VerboseOutput "Running: $cmakeCmd"
    
    $output = & cmake -B build -S . -G "Ninja" $cmakeVars 2>&1
    if ($LASTEXITCODE -ne 0) {
        Write-ErrorOutput "CMake configuration failed:"
        Write-ErrorOutput $output
        exit 1
    }
    
    # Build the project
    Write-InfoOutput "Building project..."
    $output = & cmake --build build --config $BuildConfig 2>&1
    if ($LASTEXITCODE -ne 0) {
        Write-ErrorOutput "Build failed:"
        Write-ErrorOutput $output
        exit 1
    }
}
finally {
    Pop-Location
}

Write-SuccessOutput "Build completed successfully!"

# Check for executable
$exePath = Join-Path $buildDir "$BuildConfig\cpp-vulkan-hip-engine.exe"
if (Test-Path $exePath) {
    Write-InfoOutput "Executable location: $exePath"
} else {
    # Try alternative location
    $exePath = Join-Path $buildDir "cpp-vulkan-hip-engine.exe"
    if (Test-Path $exePath) {
        Write-InfoOutput "Executable location: $exePath"
    } else {
        Write-WarningOutput "Could not find the executable at the expected location."
    }
} 
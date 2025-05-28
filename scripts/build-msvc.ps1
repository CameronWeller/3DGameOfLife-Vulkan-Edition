#!/usr/bin/env pwsh
# Build script for cpp-vulkan-hip-engine with proper MSVC configuration

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
Write-Host "Building cpp-vulkan-hip-engine with MSVC" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# Find Visual Studio installation
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) {
    Write-Error "vswhere not found. Please install Visual Studio 2022."
    exit 1
}

$vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vsPath) {
    Write-Error "Visual Studio with C++ tools not found."
    exit 1
}

Write-Host "Found Visual Studio at: $vsPath" -ForegroundColor Green

# Use the Developer Command Prompt script to set up environment
$devShellModule = Join-Path $vsPath "Common7\Tools\Microsoft.VisualStudio.DevShell.dll"
if (Test-Path $devShellModule) {
    Write-Host "Using Visual Studio Developer PowerShell module..." -ForegroundColor Yellow
    Import-Module $devShellModule
    Enter-VsDevShell -VsInstallPath $vsPath -SkipAutomaticLocation -DevCmdArguments "-arch=$BuildArch"
} else {
    Write-Host "Setting up environment manually..." -ForegroundColor Yellow
    
    # Find vcvarsall.bat
    $vcvarsall = Join-Path $vsPath "VC\Auxiliary\Build\vcvarsall.bat"
    if (-not (Test-Path $vcvarsall)) {
        Write-Error "vcvarsall.bat not found"
        exit 1
    }
    
    # Run vcvarsall and capture environment
    $tempFile = [System.IO.Path]::GetTempFileName()
    $cmd = "`"$vcvarsall`" $BuildArch && set"
    cmd /c $cmd > $tempFile
    
    # Parse and apply environment variables
    Get-Content $tempFile | ForEach-Object {
        if ($_ -match "^([^=]+)=(.*)$") {
            [Environment]::SetEnvironmentVariable($matches[1], $matches[2], "Process")
        }
    }
    
    Remove-Item $tempFile
}

# Verify cl.exe is available
$cl = Get-Command cl.exe -ErrorAction SilentlyContinue
if (-not $cl) {
    Write-Error "cl.exe not found after environment setup"
    exit 1
}
Write-Host "✓ MSVC compiler found at: $($cl.Source)" -ForegroundColor Green

# Check vcpkg
$vcpkgRoot = Join-Path $projectRoot "vcpkg"
if (-not (Test-Path $vcpkgRoot)) {
    Write-Error "vcpkg not found at: $vcpkgRoot"
    exit 1
}

# Bootstrap vcpkg if needed
$vcpkgExe = Join-Path $vcpkgRoot "vcpkg.exe"
if (-not (Test-Path $vcpkgExe)) {
    Write-Host "Bootstrapping vcpkg..." -ForegroundColor Yellow
    Push-Location $vcpkgRoot
    & .\bootstrap-vcpkg.bat
    Pop-Location
}

# Configure build
$buildDir = Join-Path $projectRoot "build"
if (Test-Path $buildDir) {
    Write-Host "Cleaning build directory..." -ForegroundColor Yellow
    Remove-Item -Path $buildDir -Recurse -Force
}
New-Item -ItemType Directory -Path $buildDir | Out-Null

# Run CMake
Write-Host ""
Write-Host "Configuring with CMake..." -ForegroundColor Yellow

$cmakeArgs = @(
    "-B", "build",
    "-S", ".",
    "-G", "Ninja",
    "-DCMAKE_BUILD_TYPE=$BuildConfig",
    "-DCMAKE_TOOLCHAIN_FILE=$vcpkgRoot\scripts\buildsystems\vcpkg.cmake",
    "-DVCPKG_TARGET_TRIPLET=$BuildArch-windows",
    "-DCMAKE_CXX_COMPILER=cl.exe",
    "-DCMAKE_C_COMPILER=cl.exe"
)

Push-Location $projectRoot
try {
    & cmake $cmakeArgs
    if ($LASTEXITCODE -ne 0) {
        throw "CMake configuration failed"
    }
    
    Write-Host ""
    Write-Host "Building project..." -ForegroundColor Yellow
    & cmake --build build --config $BuildConfig
    if ($LASTEXITCODE -ne 0) {
        throw "Build failed"
    }
    
    Write-Host ""
    Write-Host "Build completed successfully!" -ForegroundColor Green
    
    $exePath = Join-Path $buildDir "cpp-vulkan-hip-engine.exe"
    if (Test-Path $exePath) {
        Write-Host "Executable: $exePath" -ForegroundColor Green
    }
}
finally {
    Pop-Location
} 
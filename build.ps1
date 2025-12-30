#!/usr/bin/env pwsh

# UX Testing Framework Build Script
# This script builds the UX testing framework with OpenCV support

param(
    [string]$BuildType = "Release",
    [string]$Generator = "Visual Studio 16 2019",
    [string]$Architecture = "x64",
    [switch]$Clean,
    [switch]$Install,
    [switch]$Test,
    [string]$VcpkgPath = ""
)

Write-Host "=== UX Testing Framework Build Script ===" -ForegroundColor Green
Write-Host "Build Type: $BuildType" -ForegroundColor Yellow
Write-Host "Generator: $Generator" -ForegroundColor Yellow
Write-Host "Architecture: $Architecture" -ForegroundColor Yellow

# Check if we're on Windows
if ($IsWindows -or $env:OS -eq "Windows_NT") {
    Write-Host "Detected Windows platform" -ForegroundColor Green
} else {
    Write-Host "Warning: This script is optimized for Windows" -ForegroundColor Yellow
}

# Create build directory
$BuildDir = "build"
if ($Clean -and (Test-Path $BuildDir)) {
    Write-Host "Cleaning build directory..." -ForegroundColor Yellow
    Remove-Item -Recurse -Force $BuildDir
}

if (!(Test-Path $BuildDir)) {
    New-Item -ItemType Directory -Path $BuildDir | Out-Null
}

# Navigate to build directory
Push-Location $BuildDir

try {
    # Configure CMake
    Write-Host "Configuring CMake..." -ForegroundColor Green
    
    $CmakeArgs = @(
        "..",
        "-G", $Generator,
        "-A", $Architecture,
        "-DCMAKE_BUILD_TYPE=$BuildType"
    )
    
    # Add vcpkg toolchain if specified
    if ($VcpkgPath -and (Test-Path $VcpkgPath)) {
        $ToolchainPath = Join-Path $VcpkgPath "scripts\buildsystems\vcpkg.cmake"
        if (Test-Path $ToolchainPath) {
            $CmakeArgs += "-DCMAKE_TOOLCHAIN_FILE=$ToolchainPath"
            Write-Host "Using vcpkg toolchain: $ToolchainPath" -ForegroundColor Green
        } else {
            Write-Host "Warning: vcpkg toolchain not found at $ToolchainPath" -ForegroundColor Yellow
        }
    }
    
    # Try to find vcpkg automatically
    if (!$VcpkgPath) {
        $PossibleVcpkgPaths = @(
            "C:\vcpkg",
            "C:\dev\vcpkg",
            "$env:USERPROFILE\vcpkg",
            "$env:LOCALAPPDATA\vcpkg"
        )
        
        foreach ($path in $PossibleVcpkgPaths) {
            $toolchain = Join-Path $path "scripts\buildsystems\vcpkg.cmake"
            if (Test-Path $toolchain) {
                $CmakeArgs += "-DCMAKE_TOOLCHAIN_FILE=$toolchain"
                Write-Host "Found vcpkg at: $path" -ForegroundColor Green
                break
            }
        }
    }
    
    # Run CMake configure
    $CmakeResult = & cmake @CmakeArgs
    if ($LASTEXITCODE -ne 0) {
        Write-Host "CMake configuration failed!" -ForegroundColor Red
        Write-Host $CmakeResult
        exit 1
    }
    
    # Build the project
    Write-Host "Building project..." -ForegroundColor Green
    $BuildResult = & cmake --build . --config $BuildType --parallel
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Build failed!" -ForegroundColor Red
        Write-Host $BuildResult
        exit 1
    }
    
    Write-Host "Build completed successfully!" -ForegroundColor Green
    
    # Install if requested
    if ($Install) {
        Write-Host "Installing..." -ForegroundColor Green
        $InstallResult = & cmake --install . --config $BuildType
        if ($LASTEXITCODE -ne 0) {
            Write-Host "Installation failed!" -ForegroundColor Red
            Write-Host $InstallResult
            exit 1
        }
        Write-Host "Installation completed!" -ForegroundColor Green
    }
    
    # Run tests if requested
    if ($Test) {
        Write-Host "Running tests..." -ForegroundColor Green
        $TestResult = & ctest --verbose --output-on-failure
        if ($LASTEXITCODE -ne 0) {
            Write-Host "Tests failed!" -ForegroundColor Red
            Write-Host $TestResult
            exit 1
        }
        Write-Host "All tests passed!" -ForegroundColor Green
    }
    
    # Show build summary
    Write-Host "`n=== Build Summary ===" -ForegroundColor Green
    Write-Host "Build Type: $BuildType" -ForegroundColor White
    Write-Host "Output Directory: $(Resolve-Path .)" -ForegroundColor White
    
    # List built executables
    $ExeFiles = Get-ChildItem -Path ".\$BuildType" -Filter "*.exe" -Recurse -ErrorAction SilentlyContinue
    if ($ExeFiles) {
        Write-Host "`nBuilt Executables:" -ForegroundColor Cyan
        foreach ($exe in $ExeFiles) {
            Write-Host "  - $($exe.Name)" -ForegroundColor White
        }
    }
    
    # List built libraries
    $LibFiles = Get-ChildItem -Path ".\$BuildType" -Filter "*.dll" -Recurse -ErrorAction SilentlyContinue
    if ($LibFiles) {
        Write-Host "`nBuilt Libraries:" -ForegroundColor Cyan
        foreach ($lib in $LibFiles) {
            Write-Host "  - $($lib.Name)" -ForegroundColor White
        }
    }
    
} catch {
    Write-Host "Build script failed with error: $($_.Exception.Message)" -ForegroundColor Red
    exit 1
} finally {
    # Return to original directory
    Pop-Location
}

Write-Host "`nBuild script completed successfully!" -ForegroundColor Green 
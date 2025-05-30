# Sequential build and test script
param(
    [switch]$Clean,
    [switch]$Test,
    [string]$BuildType = "Debug",
    [switch]$Verbose
)

$ErrorActionPreference = "Stop"

# Component order (dependencies first)
$Components = @(
    "utilities",
    "core_engine", 
    "memory_management",
    "rendering",
    "game_logic",
    "camera",
    "ui"
)

# Build directory
$BuildDir = "build"

Write-Host "Starting sequential build process..." -ForegroundColor Green

# Clean if requested
if ($Clean -and (Test-Path $BuildDir)) {
    Write-Host "Cleaning build directory..." -ForegroundColor Yellow
    Remove-Item -Recurse -Force $BuildDir
}

# Create build directory
if (!(Test-Path $BuildDir)) {
    New-Item -ItemType Directory -Path $BuildDir | Out-Null
}

# Configure CMake
Write-Host "Configuring CMake..." -ForegroundColor Cyan
Set-Location $BuildDir

$ConfigureArgs = @(
    "..",
    "-DCMAKE_BUILD_TYPE=$BuildType"
)

if ($Test) {
    $ConfigureArgs += "-DBUILD_TESTING=ON"
}

if ($Verbose) {
    $ConfigureArgs += "--verbose"
}

& cmake @ConfigureArgs
if ($LASTEXITCODE -ne 0) {
    throw "CMake configuration failed"
}

# Build and test each component sequentially
foreach ($Component in $Components) {
    Write-Host "\n=== Building Component: $Component ===" -ForegroundColor Magenta
    
    # Build component
    Write-Host "Building $Component..." -ForegroundColor Yellow
    & cmake --build . --target $Component
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Failed to build component: $Component"
        exit 1
    }
    
    Write-Host "✓ $Component built successfully" -ForegroundColor Green
    
    # Test component if testing is enabled
    if ($Test) {
        Write-Host "Testing $Component..." -ForegroundColor Yellow
        
        # Run component-specific tests
        $TestPattern = "*$Component*"
        & ctest -R $TestPattern --output-on-failure
        
        if ($LASTEXITCODE -eq 0) {
            Write-Host "✓ $Component tests passed" -ForegroundColor Green
        } else {
            Write-Warning "Some tests failed for $Component, but continuing..."
        }
    }
    
    Write-Host "Component $Component completed" -ForegroundColor Cyan
}

# Build main executable
Write-Host "\n=== Building Main Executable ===" -ForegroundColor Magenta
& cmake --build . --target 3DGameOfLife-Vulkan-Edition
if ($LASTEXITCODE -ne 0) {
    Write-Error "Failed to build main executable"
    exit 1
}

Write-Host "✓ Main executable built successfully" -ForegroundColor Green

# Run integration tests if requested
if ($Test) {
    Write-Host "\n=== Running Integration Tests ===" -ForegroundColor Magenta
    & ctest -L integration --output-on-failure
    if ($LASTEXITCODE -eq 0) {
        Write-Host "✓ Integration tests passed" -ForegroundColor Green
    } else {
        Write-Warning "Some integration tests failed"
    }
}

Set-Location ..
Write-Host "\n🎉 Sequential build completed successfully!" -ForegroundColor Green
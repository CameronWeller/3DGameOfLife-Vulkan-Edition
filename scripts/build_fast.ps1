param(
    [string]$Config = "Release",
    [string]$BuildDirectory = "build",
    [switch]$Rebuild,
    [switch]$RunAfterBuild
)

$ErrorActionPreference = "Stop"

Write-Host "Fast Build Script for 3D Game of Life" -ForegroundColor Cyan
Write-Host "Configuration: $Config" -ForegroundColor Yellow
Write-Host "Build Directory: $BuildDirectory" -ForegroundColor Yellow

# Enable build time optimization
$env:CL = "/MP"  # Multi-processor compilation
$env:LINK = "/CGTHREADS:8"  # Parallel linking

# Check if build directory exists
if (-not (Test-Path $BuildDirectory)) {
    Write-Host "Build directory not found. Running initial configuration..." -ForegroundColor Yellow
    cmake -B $BuildDirectory -S . `
        -G "Ninja" `
        -DCMAKE_BUILD_TYPE=$Config `
        -DCMAKE_C_COMPILER_LAUNCHER=ccache `
        -DCMAKE_CXX_COMPILER_LAUNCHER=ccache `
        -DCMAKE_UNITY_BUILD=ON `
        -DCMAKE_UNITY_BUILD_BATCH_SIZE=16
} elseif ($Rebuild) {
    Write-Host "Cleaning build directory..." -ForegroundColor Yellow
    Remove-Item $BuildDirectory -Recurse -Force
    cmake -B $BuildDirectory -S . `
        -G "Ninja" `
        -DCMAKE_BUILD_TYPE=$Config `
        -DCMAKE_C_COMPILER_LAUNCHER=ccache `
        -DCMAKE_CXX_COMPILER_LAUNCHER=ccache `
        -DCMAKE_UNITY_BUILD=ON `
        -DCMAKE_UNITY_BUILD_BATCH_SIZE=16
}

# Build
$startTime = Get-Date
Write-Host "`nBuilding project..." -ForegroundColor Green
cmake --build $BuildDirectory --config $Config --parallel

$endTime = Get-Date
$buildTime = $endTime - $startTime
Write-Host "`nBuild completed in $($buildTime.TotalSeconds.ToString('F2')) seconds" -ForegroundColor Green

# Run if requested
if ($RunAfterBuild -and $LASTEXITCODE -eq 0) {
    Write-Host "`nRunning application..." -ForegroundColor Cyan
    & "$BuildDirectory\3DGameOfLife-Vulkan-Edition.exe"
}

exit $LASTEXITCODE 
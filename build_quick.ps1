param(
    [string]$Config = "Release"
)

$ErrorActionPreference = "Stop"

Write-Host "Quick Build Script for 3D Game of Life" -ForegroundColor Cyan
Write-Host "Configuration: $Config" -ForegroundColor Yellow

# Configure
Write-Host "`nConfiguring CMake..." -ForegroundColor Green
cmake -B build -S . `
    -DCMAKE_BUILD_TYPE=$Config `
    -G "Visual Studio 17 2022" `
    -DCMAKE_TOOLCHAIN_FILE="vcpkg/scripts/buildsystems/vcpkg.cmake"

if ($LASTEXITCODE -ne 0) {
    Write-Error "CMake configuration failed"
    exit 1
}

# Build
Write-Host "`nBuilding project..." -ForegroundColor Green
cmake --build build --config $Config --parallel

if ($LASTEXITCODE -ne 0) {
    Write-Error "Build failed"
    exit 1
}

Write-Host "`nBuild completed successfully!" -ForegroundColor Green
Write-Host "Executable location: build\$Config\minimal_vulkan_app.exe" -ForegroundColor Cyan 
# Install vcpkg
$ErrorActionPreference = "Stop"

Write-Host "Installing vcpkg..."

# Check if git is installed
try {
    git --version
} catch {
    Write-Error "Git is not installed. Please install Git first."
    exit 1
}

# Create a unique directory name using timestamp
$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$vcpkgDir = "C:\vcpkg_new_$timestamp"
if (Test-Path $vcpkgDir) {
    Write-Host "Removing existing temporary directory..."
    Remove-Item -Path $vcpkgDir -Recurse -Force
}

# Clone vcpkg repository
Write-Host "Cloning vcpkg repository..."
git clone https://github.com/microsoft/vcpkg.git $vcpkgDir

# Set VCPKG_ROOT environment variable
Write-Host "Setting VCPKG_ROOT environment variable..."
[Environment]::SetEnvironmentVariable("VCPKG_ROOT", $vcpkgDir, "Machine")

# Add vcpkg to PATH
Write-Host "Adding vcpkg to PATH..."
$currentPath = [Environment]::GetEnvironmentVariable("Path", "Machine")
if (-not $currentPath.Contains($vcpkgDir)) {
    [Environment]::SetEnvironmentVariable("Path", "$currentPath;$vcpkgDir", "Machine")
}

# Download pre-built vcpkg executable
Write-Host "Downloading vcpkg executable..."
$vcpkgUrl = "https://github.com/microsoft/vcpkg-tool/releases/latest/download/vcpkg.exe"
$vcpkgPath = Join-Path $vcpkgDir "vcpkg.exe"

try {
    $webClient = New-Object System.Net.WebClient
    $webClient.Headers.Add("User-Agent", "PowerShell Script")
    $webClient.DownloadFile($vcpkgUrl, $vcpkgPath)
} catch {
    Write-Host "First download attempt failed, trying alternative URL..."
    $vcpkgUrl = "https://github.com/microsoft/vcpkg-tool/releases/download/2024-02-14/vcpkg.exe"
    $webClient.DownloadFile($vcpkgUrl, $vcpkgPath)
}

# Verify the download
if (-not (Test-Path $vcpkgPath)) {
    Write-Error "Failed to download vcpkg.exe"
    exit 1
}

Write-Host "Download completed successfully"

# Create vcpkg-configuration.json with correct baseline
Write-Host "Creating vcpkg configuration..."
$configPath = Join-Path $vcpkgDir "vcpkg-configuration.json"
@{
    "default-registry" = @{
        "kind" = "git"
        "repository" = "https://github.com/microsoft/vcpkg"
        "baseline" = "d9ccd77bb554e67137f3f754a2e2f11f4188c82c"
    }
} | ConvertTo-Json | Set-Content $configPath

# Install required packages
Write-Host "Installing required packages..."
Set-Location -Path $vcpkgDir
& $vcpkgPath integrate install
& $vcpkgPath install --triplet x64-windows glfw3 vulkan glm imgui spdlog vulkan-memory-allocator

Write-Host "`nvcpkg installation completed at $vcpkgDir"
Write-Host "Please update your CMake command to use: -DCMAKE_TOOLCHAIN_FILE=$vcpkgDir\scripts\buildsystems\vcpkg.cmake"

# Create a build script
$buildScript = @"
@echo off
cd C:\cpp-vulkan-hip-engine
if not exist build mkdir build
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=$vcpkgDir\scripts\buildsystems\vcpkg.cmake
"@
$buildScriptPath = Join-Path $vcpkgDir "build.bat"
$buildScript | Set-Content $buildScriptPath

Write-Host "`nA build script has been created at: $buildScriptPath"
Write-Host "You can run it to build your project with the correct CMake settings."
Read-Host "Press Enter to continue"
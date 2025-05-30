# Configure development environment for 3D Game of Life - Vulkan Edition

# Check if running as administrator
$isAdmin = ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) {
    Write-Host "Please run this script as Administrator" -ForegroundColor Red
    exit 1
}

# Function to check if a command exists
function Test-Command {
    param ($command)
    $oldPreference = $ErrorActionPreference
    $ErrorActionPreference = 'stop'
    try { if (Get-Command $command) { return $true } }
    catch { return $false }
    finally { $ErrorActionPreference = $oldPreference }
}

# Function to add to PATH if not already present
function Add-ToPath {
    param (
        [string]$path
    )
    $currentPath = [Environment]::GetEnvironmentVariable("Path", "Machine")
    if ($currentPath -notlike "*$path*") {
        [Environment]::SetEnvironmentVariable("Path", "$currentPath;$path", "Machine")
        Write-Host "Added $path to PATH" -ForegroundColor Green
    }
    else {
        Write-Host "$path already in PATH" -ForegroundColor Yellow
    }
}

# Check and install required software
Write-Host "Checking and installing required software..." -ForegroundColor Cyan

# Check for Visual Studio 2022
if (-not (Test-Command "cl.exe")) {
    Write-Host "Visual Studio 2022 not found. Installing..." -ForegroundColor Yellow
    winget install Microsoft.VisualStudio.2022.Community
}

# Check for Vulkan SDK
if (-not (Test-Command "glslc.exe")) {
    Write-Host "Vulkan SDK not found. Installing..." -ForegroundColor Yellow
    winget install LunarG.VulkanSDK
    
    # Set Vulkan SDK environment variables
    $vulkanSdkPath = "C:\VulkanSDK\1.3.261.1"
    [Environment]::SetEnvironmentVariable("VULKAN_SDK", $vulkanSdkPath, "Machine")
    Add-ToPath "$vulkanSdkPath\bin"
}

# Check for CMake
if (-not (Test-Command "cmake.exe")) {
    Write-Host "CMake not found. Installing..." -ForegroundColor Yellow
    winget install Kitware.CMake
}

# Check for Ninja
if (-not (Test-Command "ninja.exe")) {
    Write-Host "Ninja not found. Installing..." -ForegroundColor Yellow
    winget install Ninja-build.Ninja
}

# Verify installations
Write-Host "`nVerifying installations..." -ForegroundColor Cyan

# Check Visual Studio
if (Test-Command "cl.exe") {
    Write-Host "Visual Studio 2022: OK" -ForegroundColor Green
}
else {
    Write-Host "Visual Studio 2022: Not found" -ForegroundColor Red
}

# Check Vulkan SDK
if (Test-Command "glslc.exe") {
    Write-Host "Vulkan SDK: OK" -ForegroundColor Green
    $glslcVersion = & glslc --version
    Write-Host "  Version: $glslcVersion" -ForegroundColor Green
}
else {
    Write-Host "Vulkan SDK: Not found" -ForegroundColor Red
}

# Check CMake
if (Test-Command "cmake.exe") {
    Write-Host "CMake: OK" -ForegroundColor Green
    $cmakeVersion = & cmake --version
    Write-Host "  Version: $cmakeVersion" -ForegroundColor Green
}
else {
    Write-Host "CMake: Not found" -ForegroundColor Red
}

# Check Ninja
if (Test-Command "ninja.exe") {
    Write-Host "Ninja: OK" -ForegroundColor Green
    $ninjaVersion = & ninja --version
    Write-Host "  Version: $ninjaVersion" -ForegroundColor Green
}
else {
    Write-Host "Ninja: Not found" -ForegroundColor Red
}

# Create build directory and configure project
Write-Host "`nConfiguring project..." -ForegroundColor Cyan

# Create build directory if it doesn't exist
if (-not (Test-Path "build")) {
    New-Item -ItemType Directory -Path "build"
}

# Change to build directory
Set-Location "build"

# Configure with CMake
Write-Host "Running CMake configuration..." -ForegroundColor Yellow
cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug -DENABLE_STATIC_ANALYSIS=ON ..

# Check if CMake configuration was successful
if ($LASTEXITCODE -eq 0) {
    Write-Host "`nEnvironment setup completed successfully!" -ForegroundColor Green
    Write-Host "You can now build the project using:" -ForegroundColor Cyan
    Write-Host "  cmake --build ." -ForegroundColor White
}
else {
    Write-Host "`nCMake configuration failed. Please check the error messages above." -ForegroundColor Red
    exit 1
} 
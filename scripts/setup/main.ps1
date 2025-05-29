# Main setup script for the C++ Vulkan/HIP Engine
# This script provides a unified interface for setting up the development environment

param(
    [switch]$Admin,
    [switch]$Clean,
    [switch]$Help
)

function Show-Help {
    Write-Host "C++ Vulkan/HIP Engine Setup Script"
    Write-Host "Usage: .\main.ps1 [options]"
    Write-Host ""
    Write-Host "Options:"
    Write-Host "  -Admin    Run setup with administrator privileges"
    Write-Host "  -Clean    Clean build directories before setup"
    Write-Host "  -Help     Show this help message"
    Write-Host ""
    Write-Host "Examples:"
    Write-Host "  .\main.ps1              # Normal setup"
    Write-Host "  .\main.ps1 -Admin       # Setup with admin privileges"
    Write-Host "  .\main.ps1 -Clean       # Clean and setup"
}

if ($Help) {
    Show-Help
    exit 0
}

# Check if running as admin
$isAdmin = ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)

if ($Admin -and -not $isAdmin) {
    Write-Host "Restarting script with administrator privileges..."
    Start-Process powershell.exe -Verb RunAs -ArgumentList "-NoProfile -ExecutionPolicy Bypass -File `"$PSCommandPath`" -Admin"
    exit
}

# Clean build directories if requested
if ($Clean) {
    Write-Host "Cleaning build directories..."
    if (Test-Path "build") {
        Remove-Item -Recurse -Force "build"
    }
    if (Test-Path "vcpkg_installed") {
        Remove-Item -Recurse -Force "vcpkg_installed"
    }
}

# Run the setup scripts in order
Write-Host "Starting setup process..."

$success = $true

# Bootstrap vcpkg
Write-Host "Setting up vcpkg..."
try {
    & "$PSScriptRoot\bootstrap.ps1"
    if ($LASTEXITCODE -ne 0) {
        throw "Bootstrap script failed with exit code $LASTEXITCODE"
    }
}
catch {
    Write-Error "Failed to bootstrap vcpkg: $_"
    $success = $false
}

# Install dependencies
if ($success) {
    Write-Host "Installing dependencies..."
    try {
        & "$PSScriptRoot\install_dependencies.ps1"
        if ($LASTEXITCODE -ne 0) {
            throw "Dependencies installation failed with exit code $LASTEXITCODE"
        }
    }
    catch {
        Write-Error "Failed to install dependencies: $_"
        $success = $false
    }
}

if ($success) {
    Write-Host "Setup completed successfully!" -ForegroundColor Green
    Write-Host "You can now build the project using CMake."
}
else {
    Write-Error "Setup failed. Please check the errors above and try again."
    exit 1
} 
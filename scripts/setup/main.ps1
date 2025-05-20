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

# Bootstrap vcpkg
Write-Host "Setting up vcpkg..."
& "$PSScriptRoot\bootstrap_vcpkg.ps1"

# Install dependencies
Write-Host "Installing dependencies..."
& "$PSScriptRoot\install_vcpkg.ps1"

# Fix PATH if needed
Write-Host "Configuring environment..."
& "$PSScriptRoot\fix_path.ps1"

Write-Host "Setup completed successfully!"
Write-Host "You can now build the project using CMake." 
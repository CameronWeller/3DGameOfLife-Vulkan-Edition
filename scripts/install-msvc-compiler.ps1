#!/usr/bin/env pwsh
# Script to install Visual Studio Build Tools with C++ components for Vulkan development

$ErrorActionPreference = "Stop"

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "MSVC Compiler Installation Script" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "This script will install Visual Studio Build Tools 2022" -ForegroundColor Yellow
Write-Host "with C++ development components required for Vulkan development." -ForegroundColor Yellow
Write-Host ""

# Check if running as administrator
$isAdmin = ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole] "Administrator")
if (-not $isAdmin) {
    Write-Warning "This script should be run as Administrator for best results."
    Write-Host "You can continue, but some features might not install properly." -ForegroundColor Yellow
    $response = Read-Host "Continue anyway? (Y/N)"
    if ($response -ne 'Y' -and $response -ne 'y') {
        exit 0
    }
}

# Download Visual Studio Installer
$vsInstallerUrl = "https://aka.ms/vs/17/release/vs_buildtools.exe"
$vsInstallerPath = Join-Path $env:TEMP "vs_buildtools.exe"

Write-Host "Downloading Visual Studio Build Tools installer..." -ForegroundColor Yellow
try {
    Invoke-WebRequest -Uri $vsInstallerUrl -OutFile $vsInstallerPath -UseBasicParsing
} catch {
    Write-Error "Failed to download Visual Studio installer: $_"
    exit 1
}

Write-Host "✓ Installer downloaded" -ForegroundColor Green

# Define the workloads and components we need for Vulkan development
$workloads = @(
    "--add", "Microsoft.VisualStudio.Workload.VCTools",  # C++ core desktop features
    "--add", "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",  # MSVC compiler
    "--add", "Microsoft.VisualStudio.Component.Windows11SDK.26100",  # Latest Windows SDK
    "--add", "Microsoft.VisualStudio.Component.VC.CMake.Project",  # CMake support
    "--add", "Microsoft.VisualStudio.Component.VC.CoreBuildTools",  # Core build tools
    "--add", "Microsoft.VisualStudio.Component.VC.Redist.14.Latest",  # Latest redistributables
    "--add", "Microsoft.VisualStudio.Component.Windows10SDK",  # Windows 10 SDK (fallback)
    "--add", "Microsoft.VisualStudio.Component.VC.ATL",  # ATL support
    "--add", "Microsoft.VisualStudio.Component.VC.ATLMFC"  # MFC support (sometimes needed)
)

# Build the installation command
$installArgs = @(
    "--quiet",
    "--wait",
    "--norestart"
) + $workloads

Write-Host ""
Write-Host "Installing Visual Studio Build Tools with C++ components..." -ForegroundColor Yellow
Write-Host "This may take 10-30 minutes depending on your internet connection." -ForegroundColor Yellow
Write-Host ""

# Start the installation
$process = Start-Process -FilePath $vsInstallerPath -ArgumentList $installArgs -PassThru -Wait

if ($process.ExitCode -eq 0) {
    Write-Host "✓ Visual Studio Build Tools installed successfully!" -ForegroundColor Green
} elseif ($process.ExitCode -eq 3010) {
    Write-Host "✓ Visual Studio Build Tools installed successfully!" -ForegroundColor Green
    Write-Host "Note: A system restart is recommended." -ForegroundColor Yellow
} else {
    Write-Error "Installation failed with exit code: $($process.ExitCode)"
    exit 1
}

# Clean up
Remove-Item $vsInstallerPath -ErrorAction SilentlyContinue

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Post-Installation Steps:" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "1. Close and reopen your terminal/PowerShell window" -ForegroundColor Yellow
Write-Host "2. Run the diagnostic script to verify installation:" -ForegroundColor Yellow
Write-Host "   .\scripts\diagnose-build.ps1" -ForegroundColor White
Write-Host "3. Build your project:" -ForegroundColor Yellow
Write-Host "   .\scripts\build.ps1" -ForegroundColor White
Write-Host ""

# Check if restart is needed
if ($process.ExitCode -eq 3010) {
    Write-Host "A system restart is recommended. Restart now? (Y/N)" -ForegroundColor Yellow
    $response = Read-Host
    if ($response -eq 'Y' -or $response -eq 'y') {
        Write-Host "Restarting in 10 seconds..." -ForegroundColor Yellow
        Start-Sleep -Seconds 10
        Restart-Computer
    }
} 
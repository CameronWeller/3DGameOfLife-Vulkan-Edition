#!/usr/bin/env pwsh
# Simple script to guide through MSVC compiler installation

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "MSVC Compiler Installation Guide" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# Check if MSVC is already installed
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$hasCompiler = $false

if (Test-Path $vswhere) {
    $vsInstalls = & $vswhere -all -format json | ConvertFrom-Json
    foreach ($vs in $vsInstalls) {
        $hasVC = $vs.packages | Where-Object { $_.id -like "*VC.Tools*" }
        if ($hasVC) {
            $hasCompiler = $true
            Write-Host "✓ Found MSVC compiler in: $($vs.displayName)" -ForegroundColor Green
            break
        }
    }
}

if ($hasCompiler) {
    Write-Host ""
    Write-Host "MSVC compiler is already installed!" -ForegroundColor Green
    Write-Host "You can now build the project using: .\scripts\build.ps1" -ForegroundColor Yellow
    exit 0
}

Write-Host "MSVC compiler is not installed." -ForegroundColor Yellow
Write-Host ""
Write-Host "You have two options to install it:" -ForegroundColor Cyan
Write-Host ""

Write-Host "Option 1: Automated Installation (Recommended)" -ForegroundColor Yellow
Write-Host "------------------------------------------------" -ForegroundColor DarkGray
Write-Host "Run the automated installer with administrator privileges:" -ForegroundColor White
Write-Host ""
Write-Host "  1. Open PowerShell as Administrator" -ForegroundColor Gray
Write-Host "  2. Navigate to project directory: cd C:\cpp-vulkan-hip-engine" -ForegroundColor Gray
Write-Host "  3. Run: .\scripts\install-msvc-compiler.ps1" -ForegroundColor White
Write-Host ""

Write-Host "Option 2: Manual Installation via Visual Studio Installer" -ForegroundColor Yellow
Write-Host "------------------------------------------------" -ForegroundColor DarkGray
Write-Host "1. Download Visual Studio Installer from:" -ForegroundColor White
Write-Host "   https://visualstudio.microsoft.com/downloads/" -ForegroundColor Cyan
Write-Host ""
Write-Host "2. Choose one of these editions:" -ForegroundColor White
Write-Host "   - Visual Studio 2022 Community (Free, full IDE)" -ForegroundColor Gray
Write-Host "   - Visual Studio 2022 Build Tools (Free, command-line only)" -ForegroundColor Gray
Write-Host ""
Write-Host "3. During installation, select these workloads:" -ForegroundColor White
Write-Host "   ✓ Desktop development with C++" -ForegroundColor Green
Write-Host ""
Write-Host "4. Make sure these components are selected:" -ForegroundColor White
Write-Host "   ✓ MSVC v143 - VS 2022 C++ x64/x86 build tools" -ForegroundColor Green
Write-Host "   ✓ Windows 11 SDK (or Windows 10 SDK)" -ForegroundColor Green
Write-Host "   ✓ CMake tools for Windows" -ForegroundColor Green
Write-Host ""

Write-Host "After installation:" -ForegroundColor Cyan
Write-Host "1. Close and reopen PowerShell" -ForegroundColor White
Write-Host "2. Run: .\scripts\diagnose-build.ps1 (to verify installation)" -ForegroundColor White
Write-Host "3. Run: .\scripts\build.ps1 (to build the project)" -ForegroundColor White
Write-Host ""

# Offer to open the download page
Write-Host "Would you like to open the Visual Studio download page? (Y/N)" -ForegroundColor Yellow
$response = Read-Host
if ($response -eq 'Y' -or $response -eq 'y') {
    Start-Process "https://visualstudio.microsoft.com/downloads/"
} 
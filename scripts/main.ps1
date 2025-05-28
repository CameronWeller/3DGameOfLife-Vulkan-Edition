# C++ Vulkan/HIP Engine - Main Script
# This script provides a unified interface for all project operations

param(
    [Parameter(Position = 0)]
    [ValidateSet("build", "clean", "setup", "test", "docker", "quality", "probe", "help")]
    [string]$Command = "help",
    
    [Parameter()]
    [ValidateSet("Debug", "Release")]
    [string]$BuildConfig = "Debug",
    
    [Parameter()]
    [ValidateSet("x64", "x86")]
    [string]$BuildArch = "x64",
    
    [switch]$Verbose,
    [switch]$Force,
    [switch]$Admin
)

$ErrorActionPreference = "Stop"

# Get script directory and project root
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$projectRoot = Split-Path -Parent $scriptDir

# Import common functions
. "$scriptDir\common\functions.ps1"

# Show help
function Show-Help {
    Write-Host "C++ Vulkan/HIP Engine - Main Script" -ForegroundColor Cyan
    Write-Host "=====================================" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "Usage: .\main.ps1 <command> [options]" -ForegroundColor Yellow
    Write-Host ""
    Write-Host "Commands:" -ForegroundColor Green
    Write-Host "  build     - Build the project"
    Write-Host "  clean     - Clean build artifacts"
    Write-Host "  setup     - Setup development environment"
    Write-Host "  test      - Run tests"
    Write-Host "  docker    - Build Docker image"
    Write-Host "  quality   - Run code quality checks"
    Write-Host "  probe     - Probe and cache system environment"
    Write-Host "  help      - Show this help message"
    Write-Host ""
    Write-Host "Options:" -ForegroundColor Green
    Write-Host "  -BuildConfig <Debug|Release>  - Build configuration (default: Debug)"
    Write-Host "  -BuildArch <x64|x86>         - Build architecture (default: x64)"
    Write-Host "  -Verbose                     - Enable verbose output"
    Write-Host "  -Force                       - Force operation (skip confirmations)"
    Write-Host "  -Admin                       - Run with administrator privileges"
    Write-Host ""
    Write-Host "Examples:" -ForegroundColor Green
    Write-Host "  .\main.ps1 build"
    Write-Host "  .\main.ps1 build -BuildConfig Release"
    Write-Host "  .\main.ps1 setup -Admin"
    Write-Host "  .\main.ps1 clean -Force"
}

# Check if running as admin when required
if ($Admin) {
    $isAdmin = ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
    if (-not $isAdmin) {
        Write-Host "Restarting script with administrator privileges..." -ForegroundColor Yellow
        $arguments = @("-NoProfile", "-ExecutionPolicy", "Bypass", "-File", "`"$PSCommandPath`"", $Command)
        if ($BuildConfig) { $arguments += "-BuildConfig", $BuildConfig }
        if ($BuildArch) { $arguments += "-BuildArch", $BuildArch }
        if ($Verbose) { $arguments += "-Verbose" }
        if ($Force) { $arguments += "-Force" }
        $arguments += "-Admin"
        Start-Process powershell.exe -Verb RunAs -ArgumentList $arguments
        exit
    }
}

# Execute command
switch ($Command) {
    "build" {
        & "$scriptDir\commands\build.ps1" -BuildConfig $BuildConfig -BuildArch $BuildArch -Verbose:$Verbose
    }
    "clean" {
        & "$scriptDir\commands\clean.ps1" -Force:$Force -Verbose:$Verbose
    }
    "setup" {
        & "$scriptDir\commands\setup.ps1" -Verbose:$Verbose
    }
    "test" {
        & "$scriptDir\commands\test.ps1" -BuildConfig $BuildConfig -Verbose:$Verbose
    }
    "docker" {
        & "$scriptDir\commands\docker.ps1" -Verbose:$Verbose
    }
    "quality" {
        & "$scriptDir\commands\quality.ps1" -Verbose:$Verbose
    }
    "probe" {
        & "$scriptDir\commands\probe.ps1" -Verbose:$Verbose
    }
    "help" {
        Show-Help
    }
    default {
        Write-Host "Unknown command: $Command" -ForegroundColor Red
        Write-Host ""
        Show-Help
        exit 1
    }
} 
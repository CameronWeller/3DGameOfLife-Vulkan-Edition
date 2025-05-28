# Clean command for C++ Vulkan/HIP Engine

param(
    [switch]$Force,
    [switch]$Verbose
)

$ErrorActionPreference = "Stop"

# Get script directory and project root
$scriptDir = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$projectRoot = Split-Path -Parent $scriptDir

# Import common functions
. "$scriptDir\common\functions.ps1"

Write-InfoOutput "Starting clean process..."

# Directories to clean
$dirsToClean = @(
    "build",
    "vcpkg_installed",
    ".vs",
    "out",
    "x64",
    "Debug",
    "Release"
)

# Files to clean
$filesToClean = @(
    "*.user",
    "*.suo",
    "*.ncb",
    "*.sdf",
    "*.opensdf",
    "*.VC.db",
    "*.VC.opendb"
)

# Confirm before cleaning unless Force is specified
if (-not $Force) {
    Write-WarningOutput "This will remove the following directories:"
    foreach ($dir in $dirsToClean) {
        $fullPath = Join-Path $projectRoot $dir
        if (Test-Path $fullPath) {
            Write-Host "  - $fullPath"
        }
    }
    
    Write-Host ""
    $response = Read-Host "Are you sure you want to continue? (y/N)"
    if ($response -ne 'y' -and $response -ne 'Y') {
        Write-InfoOutput "Clean operation cancelled."
        exit 0
    }
}

# Clean directories
foreach ($dir in $dirsToClean) {
    $fullPath = Join-Path $projectRoot $dir
    if (Test-Path $fullPath) {
        Write-VerboseOutput "Removing directory: $fullPath"
        try {
            Remove-Item -Path $fullPath -Recurse -Force -ErrorAction Stop
            Write-VerboseOutput "Removed: $fullPath"
        }
        catch {
            Write-WarningOutput "Failed to remove $fullPath : $($_.Exception.Message)"
        }
    }
}

# Clean files
foreach ($pattern in $filesToClean) {
    $files = Get-ChildItem -Path $projectRoot -Filter $pattern -Recurse -ErrorAction SilentlyContinue
    foreach ($file in $files) {
        Write-VerboseOutput "Removing file: $($file.FullName)"
        try {
            Remove-Item -Path $file.FullName -Force -ErrorAction Stop
            Write-VerboseOutput "Removed: $($file.FullName)"
        }
        catch {
            Write-WarningOutput "Failed to remove $($file.FullName): $($_.Exception.Message)"
        }
    }
}

# Clean shader build artifacts
$shaderBuildDir = Join-Path $projectRoot "shaders\build"
if (Test-Path $shaderBuildDir) {
    Write-VerboseOutput "Removing shader build directory: $shaderBuildDir"
    try {
        Remove-Item -Path $shaderBuildDir -Recurse -Force -ErrorAction Stop
        Write-VerboseOutput "Removed: $shaderBuildDir"
    }
    catch {
        Write-WarningOutput "Failed to remove $shaderBuildDir : $($_.Exception.Message)"
    }
}

# Clean compiled shaders
$compiledShaders = Get-ChildItem -Path (Join-Path $projectRoot "shaders") -Filter "*.spv" -ErrorAction SilentlyContinue
foreach ($shader in $compiledShaders) {
    Write-VerboseOutput "Removing compiled shader: $($shader.FullName)"
    try {
        Remove-Item -Path $shader.FullName -Force -ErrorAction Stop
        Write-VerboseOutput "Removed: $($shader.FullName)"
    }
    catch {
        Write-WarningOutput "Failed to remove $($shader.FullName): $($_.Exception.Message)"
    }
}

Write-SuccessOutput "Clean completed successfully!" 
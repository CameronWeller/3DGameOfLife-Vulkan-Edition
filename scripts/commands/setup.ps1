# Setup command for C++ Vulkan/HIP Engine

param(
    [switch]$Verbose,
    [switch]$SkipDependencies,
    [switch]$SkipEnvironment
)

$ErrorActionPreference = "Stop"

# Get script directory and project root
$scriptDir = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$projectRoot = Split-Path -Parent $scriptDir

# Import common functions
. "$scriptDir\common\functions.ps1"

Write-InfoOutput "Starting setup process..."

# Run environment probe first to gather system information
Write-InfoOutput "Probing system environment..."
& "$scriptDir\probe.ps1" -Force -Verbose:$Verbose

# Check if running as administrator (recommended for some operations)
$isAdmin = ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) {
    Write-WarningOutput "Running without administrator privileges. Some operations may fail."
    Write-WarningOutput "Consider running with -Admin flag from main.ps1"
}

# Step 1: Check PowerShell version
Write-InfoOutput "Checking PowerShell version..."
$psVersion = $PSVersionTable.PSVersion
if ($psVersion.Major -lt 7) {
    Write-WarningOutput "PowerShell 7 or later is recommended. Current version: $psVersion"
    Write-InfoOutput "You can install PowerShell 7 from: https://aka.ms/powershell"
}

# Step 2: Check and install required tools
if (-not $SkipDependencies) {
    Write-InfoOutput "Checking required tools..."
    
    # Check Git
    if (-not (Test-Command "git")) {
        Write-ErrorOutput "Git is not installed. Please install Git from: https://git-scm.com/"
        exit 1
    }
    
    # Check CMake
    if (-not (Test-Command "cmake")) {
        Write-ErrorOutput "CMake is not installed. Please install CMake from: https://cmake.org/"
        exit 1
    }
    
    # Check Visual Studio
    $vsPath = Find-VisualStudio
    if (-not $vsPath) {
        Write-ErrorOutput "Visual Studio 2022 with C++ development tools is required."
        Write-InfoOutput "Please install from: https://visualstudio.microsoft.com/"
        exit 1
    }
    
    # Check Vulkan SDK
    if (-not (Test-VulkanSdk)) {
        Write-ErrorOutput "Vulkan SDK is not installed."
        Write-InfoOutput "Please install from: https://vulkan.lunarg.com/"
        exit 1
    }
    
    # Check HIP SDK (optional)
    if (-not (Test-HipSdk)) {
        Write-WarningOutput "HIP SDK is not installed. HIP support will be disabled."
        Write-InfoOutput "To enable HIP support, install from: https://rocm.docs.amd.com/"
    }
}

# Step 3: Setup vcpkg
Write-InfoOutput "Setting up vcpkg..."
$vcpkgDir = Join-Path $projectRoot "vcpkg"

if (-not (Test-Path $vcpkgDir)) {
    Write-InfoOutput "Cloning vcpkg repository..."
    Push-Location $projectRoot
    try {
        git clone https://github.com/Microsoft/vcpkg.git
        if ($LASTEXITCODE -ne 0) {
            throw "Failed to clone vcpkg repository"
        }
    }
    catch {
        Write-ErrorOutput $_.Exception.Message
        Pop-Location
        exit 1
    }
    finally {
        Pop-Location
    }
}

# Initialize vcpkg
$vcpkgRoot = Initialize-Vcpkg -ProjectRoot $projectRoot
if (-not $vcpkgRoot) {
    exit 1
}

# Step 4: Setup environment variables
if (-not $SkipEnvironment) {
    Write-InfoOutput "Setting up environment variables..."
    
    # Add vcpkg to PATH if not already there
    $userPath = [Environment]::GetEnvironmentVariable("Path", "User")
    if (-not $userPath.Contains($vcpkgRoot)) {
        Write-InfoOutput "Adding vcpkg to user PATH..."
        $newPath = "$vcpkgRoot;$userPath"
        [Environment]::SetEnvironmentVariable("Path", $newPath, "User")
        $env:Path = "$vcpkgRoot;$env:Path"
    }
    
    # Set VCPKG_ROOT environment variable
    [Environment]::SetEnvironmentVariable("VCPKG_ROOT", $vcpkgRoot, "User")
    $env:VCPKG_ROOT = $vcpkgRoot
}

# Step 5: Install vcpkg dependencies
Write-InfoOutput "Installing vcpkg dependencies..."
$manifestFile = Join-Path $projectRoot "vcpkg.json"
if (Test-Path $manifestFile) {
    Push-Location $projectRoot
    try {
        Write-VerboseOutput "Running vcpkg install..."
        & vcpkg install --triplet x64-windows
        if ($LASTEXITCODE -ne 0) {
            throw "Failed to install vcpkg dependencies"
        }
    }
    catch {
        Write-ErrorOutput $_.Exception.Message
        Pop-Location
        exit 1
    }
    finally {
        Pop-Location
    }
} else {
    Write-WarningOutput "vcpkg.json not found. Skipping dependency installation."
}

# Step 6: Setup Git hooks (optional)
$gitHooksDir = Join-Path $projectRoot ".git\hooks"
if (Test-Path $gitHooksDir) {
    Write-InfoOutput "Setting up Git hooks..."
    
    # Create pre-commit hook for code formatting
    $preCommitHook = @"
#!/bin/sh
# Pre-commit hook for code formatting
echo "Running pre-commit checks..."

# Run clang-format on changed files
git diff --cached --name-only --diff-filter=ACM | grep -E '\.(cpp|hpp|h|c)$' | while read file; do
    echo "Formatting: \$file"
    clang-format -i "\$file"
    git add "\$file"
done

exit 0
"@
    
    $preCommitPath = Join-Path $gitHooksDir "pre-commit"
    Set-Content -Path $preCommitPath -Value $preCommitHook -Encoding UTF8
    Write-VerboseOutput "Created pre-commit hook"
}

# Step 7: Create initial build directory
$buildDir = Join-Path $projectRoot "build"
if (-not (Test-Path $buildDir)) {
    Write-InfoOutput "Creating build directory..."
    New-Item -ItemType Directory -Path $buildDir | Out-Null
}

# Step 8: Verify setup
Write-InfoOutput "Verifying setup..."
$setupValid = $true

# Check all required components
$components = @{
    "Git" = (Test-Command "git")
    "CMake" = (Test-Command "cmake")
    "Visual Studio" = ($null -ne (Find-VisualStudio))
    "vcpkg" = (Test-Command "vcpkg")
    "Vulkan SDK" = (Test-VulkanSdk)
    "Ninja" = (Test-Path (Join-Path $projectRoot "tools\ninja\ninja.exe"))
}

Write-Host ""
Write-Host "Setup Status:" -ForegroundColor Cyan
Write-Host "=============" -ForegroundColor Cyan
foreach ($component in $components.Keys) {
    $status = if ($components[$component]) { "[OK]" } else { "[MISSING]" }
    $color = if ($components[$component]) { "Green" } else { "Red" }
    Write-Host "$component : $status" -ForegroundColor $color
}

# Optional components
Write-Host ""
Write-Host "Optional Components:" -ForegroundColor Cyan
Write-Host "===================" -ForegroundColor Cyan
$hipStatus = if (Test-HipSdk) { "[OK]" } else { "[NOT INSTALLED]" }
$hipColor = if (Test-HipSdk) { "Green" } else { "Yellow" }
Write-Host "HIP SDK : $hipStatus" -ForegroundColor $hipColor

if ($components.Values -contains $false) {
    Write-ErrorOutput "Setup incomplete. Please install missing components."
    exit 1
}

Write-Host ""
Write-SuccessOutput "Setup completed successfully!"
Write-InfoOutput "You can now build the project using: .\main.ps1 build" 
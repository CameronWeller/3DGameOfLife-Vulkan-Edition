[CmdletBinding()]
param()

# Set strict mode for better error handling
Set-StrictMode -Version Latest

# Function to check if a command exists
function Test-CommandExists {
    param ($command)
    $oldPreference = $ErrorActionPreference
    $ErrorActionPreference = 'stop'
    try { if (Get-Command $command) { return $true } }
    catch { return $false }
    finally { $ErrorActionPreference = $oldPreference }
}

# Function to validate environment
function Test-Environment {
    Write-Host "Validating environment..."
    
    # Check PowerShell version
    $psVersion = $PSVersionTable.PSVersion.Major
    if ($psVersion -lt 5) {
        throw "PowerShell 5.0 or higher is required. Current version: $psVersion"
    }
    
    # Check if running as administrator
    $isAdmin = ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
    if (-not $isAdmin) {
        throw "This script must be run as Administrator. Please run the main setup script with -Admin flag."
    }
    
    # Check required commands
    $requiredCommands = @('git', 'cmake')
    foreach ($cmd in $requiredCommands) {
        if (-not (Test-CommandExists $cmd)) {
            throw "Required command '$cmd' not found. Please ensure it is installed and in your PATH"
        }
    }
    
    Write-Host "Environment validation successful" -ForegroundColor Green
}

# Main execution
try {
    # Validate environment
    Test-Environment
    
    # Get project root directory
    $projectRoot = Split-Path -Parent (Split-Path -Parent $script:MyInvocation.MyCommand.Definition)
    $vcpkgRoot = Join-Path $projectRoot "vcpkg"
    
    if (-not (Test-Path $vcpkgRoot)) {
        throw "vcpkg directory not found at: $vcpkgRoot. Please run bootstrap.ps1 first."
    }
    
    # Set VCPKG_ROOT environment variable
    [Environment]::SetEnvironmentVariable("VCPKG_ROOT", $vcpkgRoot, [EnvironmentVariableTarget]::Machine)
    $env:VCPKG_ROOT = $vcpkgRoot
    
    Write-Host "Installing core dependencies..."
    
    # Install core dependencies using manifest mode with validation layers feature
    & "$vcpkgRoot\vcpkg.exe" install --triplet x64-windows --feature-flags=manifests,versions --x-feature=vulkan-validationlayers
    
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to install dependencies"
    }
    
    Write-Host "Dependencies installed successfully" -ForegroundColor Green
    
    # Create build directory if it doesn't exist
    $buildDir = Join-Path $projectRoot "build"
    if (-not (Test-Path $buildDir)) {
        New-Item -ItemType Directory -Path $buildDir | Out-Null
    }
    
    # Configure CMake
    Write-Host "Configuring CMake..."
    Set-Location $buildDir
    & cmake .. -DCMAKE_TOOLCHAIN_FILE="$vcpkgRoot/scripts/buildsystems/vcpkg.cmake" -DCMAKE_BUILD_TYPE=Debug
    
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to configure CMake"
    }
    
    Write-Host "CMake configuration completed successfully" -ForegroundColor Green
    Write-Host "You can now build the project using: cmake --build ." -ForegroundColor Green
    exit 0
}
catch {
    Write-Error "Installation failed: $_"
    exit 1
} 
# Setup script for cpp-vulkan-hip-engine
# This script handles both initial setup and dependency installation

# Enable verbose output and strict mode
$VerbosePreference = "Continue"
Set-StrictMode -Version Latest

# Configuration
$config = @{
    VcpkgRoot = "C:\vcpkg"
    BuildType = "Release"
    Triplet = "x64-windows"
    Dependencies = @(
        "vulkan",
        "glfw3",
        "glm",
        "imgui",
        "spdlog",
        "vulkan-memory-allocator"
    )
    VSVersion = "2019"
    BuildToolsPath = "C:\Program Files (x86)\Microsoft Visual Studio\$VSVersion\BuildTools"
}

# Function to write to log file
function Write-Log {
    param(
        [string]$Message,
        [string]$Level = "INFO"
    )
    $timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
    $logMessage = "[$timestamp] [$Level] $Message"
    Write-Host $logMessage
    Add-Content -Path "setup.log" -Value $logMessage
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

# Function to download a file
function Download-File {
    param (
        [string]$url,
        [string]$output
    )
    Write-Log "Downloading $url to $output..."
    try {
        Invoke-WebRequest -Uri $url -OutFile $output
        Write-Log "Download completed successfully"
    }
    catch {
        Write-Log "Failed to download file: $_" -Level "ERROR"
        throw
    }
}

# Function to extract a zip file
function Extract-Zip {
    param (
        [string]$zipFile,
        [string]$destination
    )
    Write-Log "Extracting $zipFile to $destination..."
    try {
        Expand-Archive -Path $zipFile -DestinationPath $destination -Force
        Write-Log "Extraction completed successfully"
    }
    catch {
        Write-Log "Failed to extract file: $_" -Level "ERROR"
        throw
    }
}

# Function to install Chocolatey
function Install-Chocolatey {
    Write-Log "Installing Chocolatey..."
    try {
        Set-ExecutionPolicy Bypass -Scope Process -Force
        [System.Net.ServicePointManager]::SecurityProtocol = [System.Net.ServicePointManager]::SecurityProtocol -bor 3072
        Invoke-Expression ((New-Object System.Net.WebClient).DownloadString('https://chocolatey.org/install.ps1'))
        $env:Path = "$env:Path;$env:ChocolateyInstall\bin"
        Write-Log "Chocolatey installed successfully"
    }
    catch {
        Write-Log "Failed to install Chocolatey: $_" -Level "ERROR"
        throw
    }
}

# Function to install vcpkg
function Install-Vcpkg {
    Write-Log "Installing vcpkg..."
    try {
        if (-not (Test-Path $config.VcpkgRoot)) {
            git clone https://github.com/Microsoft/vcpkg.git $config.VcpkgRoot
        }
        Push-Location $config.VcpkgRoot
        .\bootstrap-vcpkg.bat
        .\vcpkg integrate install
        Pop-Location
        Write-Log "vcpkg installed successfully"
    }
    catch {
        Write-Log "Failed to install vcpkg: $_" -Level "ERROR"
        throw
    }
}

# Function to install dependencies
function Install-Dependencies {
    Write-Log "Installing project dependencies..."
    try {
        foreach ($dep in $config.Dependencies) {
            Write-Log "Installing $dep..."
            & "$($config.VcpkgRoot)\vcpkg.exe" install "$($dep):$($config.Triplet)" --recurse
        }
        Write-Log "Project dependencies installed successfully"
    }
    catch {
        Write-Log "Failed to install dependencies: $_" -Level "ERROR"
        throw
    }
}

# Function to build the project
function Build-Project {
    Write-Log "Building project..."
    try {
        if (-not (Test-Path "build")) {
            New-Item -ItemType Directory -Path "build"
        }
        Push-Location build
        cmake .. -DCMAKE_TOOLCHAIN_FILE="$($config.VcpkgRoot)\scripts\buildsystems\vcpkg.cmake"
        cmake --build . --config $config.BuildType
        Pop-Location
        Write-Log "Project built successfully"
    }
    catch {
        Write-Log "Failed to build project: $_" -Level "ERROR"
        throw
    }
}

# Main execution
try {
    # Check if running as administrator
    $isAdmin = ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
    if (-not $isAdmin) {
        Write-Host "Please run this script as Administrator"
        exit
    }

    # Check if PowerShell is installed
    $psVersion = $PSVersionTable.PSVersion
    Write-Host "PowerShell version: $psVersion"

    # Initialize log file
    if (Test-Path "setup.log") {
        Remove-Item "setup.log"
    }
    New-Item -ItemType File -Path "setup.log"

    Write-Log "Starting setup process..."

    # Install Chocolatey if not present
    if (-not (Test-Command choco)) {
        Install-Chocolatey
    }

    # Install Git if not present
    if (-not (Test-Command git)) {
        Write-Log "Installing Git..."
        choco install git -y
        $env:Path = "$env:Path;$env:ProgramFiles\Git\cmd"
    }

    # Install CMake if not present
    if (-not (Test-Command cmake)) {
        Write-Log "Installing CMake..."
        choco install cmake -y
        $env:Path = "$env:Path;$env:ProgramFiles\CMake\bin"
    }

    # Install Visual Studio Build Tools if not present
    if (-not (Test-Path $config.BuildToolsPath)) {
        Write-Log "Installing Visual Studio Build Tools..."
        choco install visualstudio2019buildtools -y
        choco install visualstudio2019-workload-vctools -y
    }

    # Install vcpkg
    Install-Vcpkg

    # Add vcpkg to PATH
    $env:Path = "$env:Path;$($config.VcpkgRoot)"

    # Install dependencies
    Install-Dependencies

    # Build the project
    Build-Project

    Write-Log "Setup completed successfully!"
}
catch {
    Write-Log "Setup failed: $_" -Level "ERROR"
    exit 1
} 
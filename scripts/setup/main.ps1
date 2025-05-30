# Main Setup Script
# This script sets up the development environment for the project

param(
    [switch]$Admin,
    [switch]$Clean,
    [string]$VulkanSDKPath
)

# Function to check if running as admin
function Test-Admin {
    $currentUser = New-Object Security.Principal.WindowsPrincipal([Security.Principal.WindowsIdentity]::GetCurrent())
    return $currentUser.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

# Function to clean build directory
function Clear-BuildDirectory {
    if (Test-Path "build") {
        Write-Host "Cleaning build directory..."
        Remove-Item -Path "build" -Recurse -Force
    }
}

# Function to verify Visual Studio installation
function Test-VisualStudio {
    $vsPath = "${env:ProgramFiles}\Microsoft Visual Studio\2022\Community"
    if (-not (Test-Path $vsPath)) {
        Write-Error "Visual Studio 2022 not found. Please install Visual Studio 2022 with C++ development tools."
        exit 1
    }
}

# Function to verify CMake installation
function Test-CMake {
    try {
        $cmakeVersion = (cmake --version) -match "cmake version (\d+\.\d+\.\d+)"
        if ($cmakeVersion) {
            $version = [Version]$Matches[1]
            if ($version -lt [Version]"3.15.0") {
                Write-Error "CMake version 3.15 or higher is required. Current version: $version"
                exit 1
            }
        } else {
            Write-Error "CMake not found. Please install CMake 3.15 or higher."
            exit 1
        }
    } catch {
        Write-Error "CMake not found. Please install CMake 3.15 or higher."
        exit 1
    }
}

# Main script execution
try {
    # Check if running as admin when required
    if ($Admin -and -not (Test-Admin)) {
        Write-Error "This script requires administrator privileges. Please run as administrator."
        exit 1
    }

    # Clean if requested
    if ($Clean) {
        Clear-BuildDirectory
    }

    # Verify prerequisites
    Write-Host "Verifying prerequisites..."
    Test-VisualStudio
    Test-CMake

    # Configure Vulkan SDK
    Write-Host "`nConfiguring Vulkan SDK..."
    $vulkanScript = Join-Path $PSScriptRoot "configure_vulkan.ps1"
    if ($VulkanSDKPath) {
        & $vulkanScript -Admin:$Admin -VulkanSDKPath $VulkanSDKPath
    } else {
        & $vulkanScript -Admin:$Admin
    }

    # Create build directory
    if (-not (Test-Path "build")) {
        Write-Host "`nCreating build directory..."
        New-Item -ItemType Directory -Path "build" | Out-Null
    }

    # Configure CMake
    Write-Host "`nConfiguring CMake..."
    Set-Location build
    cmake .. -DCMAKE_BUILD_TYPE=Debug

    # Build the project
    Write-Host "`nBuilding the project..."
    cmake --build . --config Debug

    Write-Host "`nSetup complete!"
    Write-Host "You can now run the project from the build directory."
} catch {
    Write-Error "An error occurred: $_"
    exit 1
} finally {
    # Return to original directory
    Set-Location $PSScriptRoot/../..
} 
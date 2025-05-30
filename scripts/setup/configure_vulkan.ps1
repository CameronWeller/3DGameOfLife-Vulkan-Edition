# Vulkan SDK Configuration Script
# This script helps set up the Vulkan SDK environment variables

param(
    [switch]$Admin,
    [string]$VulkanSDKPath
)

# Function to check if running as admin
function Test-Admin {
    $currentUser = New-Object Security.Principal.WindowsPrincipal([Security.Principal.WindowsIdentity]::GetCurrent())
    return $currentUser.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

# Function to find Vulkan SDK installation
function Find-VulkanSDK {
    $possiblePaths = @(
        "C:\VulkanSDK",
        "${env:ProgramFiles}\VulkanSDK",
        "${env:ProgramFiles(x86)}\VulkanSDK"
    )

    foreach ($basePath in $possiblePaths) {
        if (Test-Path $basePath) {
            $versions = Get-ChildItem -Path $basePath -Directory | Where-Object { $_.Name -match '^\d+\.\d+\.\d+\.\d+$' }
            if ($versions) {
                return $versions | Sort-Object Name -Descending | Select-Object -First 1
            }
        }
    }
    return $null
}

# Function to set environment variables
function Set-VulkanEnvironment {
    param(
        [string]$SDKPath
    )

    # Set VULKAN_SDK environment variable
    [Environment]::SetEnvironmentVariable("VULKAN_SDK", $SDKPath, "User")
    $env:VULKAN_SDK = $SDKPath

    # Add Vulkan SDK's bin directory to PATH
    $binPath = Join-Path $SDKPath "Bin"
    $currentPath = [Environment]::GetEnvironmentVariable("Path", "User")
    if ($currentPath -notlike "*$binPath*") {
        [Environment]::SetEnvironmentVariable("Path", "$currentPath;$binPath", "User")
        $env:Path += ";$binPath"
    }

    Write-Host "Vulkan SDK environment variables configured:"
    Write-Host "VULKAN_SDK = $env:VULKAN_SDK"
    Write-Host "Added to PATH: $binPath"
}

# Main script execution
try {
    # Check if running as admin when required
    if ($Admin -and -not (Test-Admin)) {
        Write-Error "This script requires administrator privileges. Please run as administrator."
        exit 1
    }

    # Find or use provided Vulkan SDK path
    if ($VulkanSDKPath) {
        if (-not (Test-Path $VulkanSDKPath)) {
            Write-Error "Provided Vulkan SDK path does not exist: $VulkanSDKPath"
            exit 1
        }
        $sdkPath = $VulkanSDKPath
    } else {
        $sdkPath = Find-VulkanSDK
        if (-not $sdkPath) {
            Write-Error "Vulkan SDK not found. Please install it from https://vulkan.lunarg.com/sdk/home#windows"
            exit 1
        }
        $sdkPath = $sdkPath.FullName
    }

    # Configure environment
    Set-VulkanEnvironment -SDKPath $sdkPath

    # Verify configuration
    $glslcPath = Join-Path $sdkPath "Bin\glslc.exe"
    if (Test-Path $glslcPath) {
        Write-Host "`nVerification successful:"
        Write-Host "glslc compiler found at: $glslcPath"
    } else {
        Write-Warning "glslc compiler not found. Please verify your Vulkan SDK installation."
    }

    Write-Host "`nVulkan SDK configuration complete!"
    Write-Host "You may need to restart your terminal for changes to take effect."
} catch {
    Write-Error "An error occurred: $_"
    exit 1
} 
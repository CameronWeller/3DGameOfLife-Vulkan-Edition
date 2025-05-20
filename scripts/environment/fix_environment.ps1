# Fix environment and build issues script
$ErrorActionPreference = "Stop"

# Function to check if a command exists
function Test-Command($cmdname) {
    return [bool](Get-Command -Name $cmdname -ErrorAction SilentlyContinue)
}

# Function to add to PATH if not exists
function Add-ToPath($path) {
    if (-not $env:Path.Contains($path)) {
        $env:Path = "$path;$env:Path"
    }
}

Write-Host "Checking and fixing environment variables..."

# Check for Visual Studio
if (-not (Test-Command "cl.exe")) {
    Write-Host "Visual Studio not found in PATH. Attempting to find it..."
    $vsPath = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
    if (Test-Path $vsPath) {
        Write-Host "Found Visual Studio 2022. Adding to PATH..."
        cmd /c "call `"$vsPath`" && set > %temp%\vcvars.txt"
        Get-Content "$env:temp\vcvars.txt" | ForEach-Object {
            if ($_ -match "^(.*?)=(.*)$") {
                $name = $matches[1]
                $value = $matches[2]
                [System.Environment]::SetEnvironmentVariable($name, $value, [System.EnvironmentVariableTarget]::Process)
            }
        }
    } else {
        Write-Error "Visual Studio not found. Please install Visual Studio 2022 with C++ development tools."
        exit 1
    }
}

# Check for Vulkan SDK
$vulkanSdkPath = $env:VULKAN_SDK
if (-not $vulkanSdkPath) {
    Write-Host "Vulkan SDK not found in environment. Attempting to find it..."
    $possiblePaths = @(
        "C:\VulkanSDK",
        "${env:ProgramFiles}\VulkanSDK",
        "${env:ProgramFiles(x86)}\VulkanSDK"
    )
    
    foreach ($path in $possiblePaths) {
        if (Test-Path $path) {
            $latestVersion = Get-ChildItem $path | Sort-Object -Descending | Select-Object -First 1
            if ($latestVersion) {
                $vulkanSdkPath = $latestVersion.FullName
                [System.Environment]::SetEnvironmentVariable("VULKAN_SDK", $vulkanSdkPath, [System.EnvironmentVariableTarget]::Process)
                Add-ToPath "$vulkanSdkPath\Bin"
                break
            }
        }
    }
    
    if (-not $vulkanSdkPath) {
        Write-Error "Vulkan SDK not found. Please install the Vulkan SDK."
        exit 1
    }
}

# Check for vcpkg
$vcpkgRoot = $env:VCPKG_ROOT
if (-not $vcpkgRoot) {
    Write-Host "vcpkg not found in environment. Attempting to find it..."
    $possiblePaths = @(
        "C:\vcpkg",
        "${env:ProgramFiles}\vcpkg",
        "${env:USERPROFILE}\vcpkg"
    )
    
    foreach ($path in $possiblePaths) {
        if (Test-Path $path) {
            $vcpkgRoot = $path
            [System.Environment]::SetEnvironmentVariable("VCPKG_ROOT", $vcpkgRoot, [System.EnvironmentVariableTarget]::Process)
            Add-ToPath "$vcpkgRoot"
            break
        }
    }
    
    if (-not $vcpkgRoot) {
        Write-Error "vcpkg not found. Please install vcpkg."
        exit 1
    }
}

# Create build directory if it doesn't exist
if (-not (Test-Path "build")) {
    New-Item -ItemType Directory -Path "build"
}

# Run CMake configuration
Write-Host "Configuring CMake..."
Set-Location "build"
cmake .. -DCMAKE_TOOLCHAIN_FILE="$vcpkgRoot/scripts/buildsystems/vcpkg.cmake" -DCMAKE_BUILD_TYPE=Debug

# Build the project
Write-Host "Building the project..."
cmake --build . --config Debug

Write-Host "Environment setup complete!" 
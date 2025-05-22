# Build script for cpp-vulkan-hip-engine
param(
    [Parameter()]
    [ValidateSet("Debug", "Release")]
    [string]$BuildConfig = "Debug",
    
    [Parameter()]
    [ValidateSet("x64", "x86")]
    [string]$BuildArch = "x64",
    
    [Parameter()]
    [switch]$Verbose
)

$ErrorActionPreference = "Stop"

# Get script directory and project root
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$projectRoot = Split-Path -Parent $scriptDir

# Function to write verbose output
function Write-VerboseOutput {
    param([string]$Message)
    if ($Verbose) {
        Write-Host $Message
    }
}

# Function to check if a command exists
function Test-Command($cmdname) {
    return [bool](Get-Command -Name $cmdname -ErrorAction SilentlyContinue)
}

# Function to get command version
function Get-CommandVersion($cmdname) {
    $cmd = Get-Command $cmdname -ErrorAction SilentlyContinue
    if ($cmd) {
        return $cmd.Version
    }
    return $null
}

# Function to clean up temporary files
function Cleanup-TempFiles {
    param([string[]]$tempFiles)
    foreach ($file in $tempFiles) {
        if (Test-Path $file) {
            Remove-Item $file -Force -ErrorAction SilentlyContinue
        }
    }
}

# Function to download and setup Ninja
function Setup-Ninja {
    $ninjaVersion = "1.12.1"
    $ninjaUrl = "https://github.com/ninja-build/ninja/releases/download/v$ninjaVersion/ninja-win.zip"
    $ninjaDir = Join-Path $projectRoot "tools\ninja"
    $ninjaExe = Join-Path $ninjaDir "ninja.exe"
    
    if (-not (Test-Path $ninjaExe)) {
        Write-VerboseOutput "Downloading Ninja..."
        if (-not (Test-Path $ninjaDir)) {
            New-Item -ItemType Directory -Path $ninjaDir -Force | Out-Null
        }
        
        $tempZip = Join-Path $env:TEMP "ninja.zip"
        try {
            $webClient = New-Object System.Net.WebClient
            $webClient.DownloadFile($ninjaUrl, $tempZip)
            
            Write-VerboseOutput "Extracting Ninja..."
            Expand-Archive -Path $tempZip -DestinationPath $ninjaDir -Force
            
            # Add ninja to PATH for this session
            $env:Path = "$ninjaDir;$env:Path"
        }
        catch {
            Write-Error "Failed to download or extract Ninja: $($_.Exception.Message)"
            exit 1
        }
        finally {
            Cleanup-TempFiles @($tempZip)
        }
    }
    
    return $ninjaExe
}

# Function to find Visual Studio installation
function Find-VisualStudio {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere)) {
        Write-Error "vswhere not found. Please install Visual Studio 2022 with C++ development tools."
        return $null
    }

    # Try to find Visual Studio installation
    $vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $vsPath) {
        # Try to find BuildTools
        $vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath -prerelease
    }

    if (-not $vsPath) {
        Write-Error "Visual Studio with C++ tools not found. Please install Visual Studio 2022 with C++ development tools or Build Tools."
        return $null
    }

    # Check Visual Studio version
    $vsVersion = & $vswhere -latest -products * -property catalog_productLineVersion
    if ($vsVersion -lt "2022") {
        Write-Error "Visual Studio 2022 or later is required. Found version: $vsVersion"
        return $null
    }

    return $vsPath
}

# Function to setup Visual Studio environment
function Setup-VisualStudioEnvironment {
    param([string]$vsPath)

    $vcvarsPath = Join-Path $vsPath "VC\Auxiliary\Build\vcvarsall.bat"
    if (-not (Test-Path $vcvarsPath)) {
        # Try alternative path for BuildTools
        $vcvarsPath = Join-Path "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\BuildTools" "VC\Auxiliary\Build\vcvarsall.bat"
        if (-not (Test-Path $vcvarsPath)) {
            Write-Error "Could not find vcvarsall.bat. Please ensure Visual Studio or Build Tools is properly installed."
            return $false
        }
    }

    Write-VerboseOutput "Setting up Visual Studio environment..."
    Write-VerboseOutput "Using Visual Studio at: $vsPath"
    Write-VerboseOutput "Using vcvarsall.bat at: $vcvarsPath"

    # Get the Visual Studio installation directory
    $vsInstallDir = $vsPath
    $vcInstallDir = Join-Path $vsInstallDir "VC"

    # Set up environment variables
    $env:VSINSTALLDIR = $vsInstallDir
    $env:VCINSTALLDIR = $vcInstallDir
    $env:VCPKG_VISUAL_STUDIO_PATH = $vsInstallDir

    # Set up Visual Studio environment using PowerShell
    $vsDevCmd = Join-Path $vsPath "Common7\Tools\Launch-VsDevShell.ps1"
    $tempFile = [System.IO.Path]::GetTempFileName()
    try {
        if (Test-Path $vsDevCmd) {
            Write-VerboseOutput "Using Visual Studio Developer Shell..."
            & $vsDevCmd -Arch amd64 -HostArch amd64 -SkipAutomaticLocation
            if ($LASTEXITCODE -ne 0) {
                throw "Failed to initialize Visual Studio Developer Shell"
            }
        } else {
            Write-VerboseOutput "Using vcvarsall.bat..."
            # Fallback to using vcvarsall.bat through PowerShell
            $cmdLine = "`"$vcvarsPath`" x64 > `"$tempFile`" 2>&1"
            $process = Start-Process -FilePath "cmd.exe" -ArgumentList "/c $cmdLine" -NoNewWindow -Wait -PassThru
            if ($process.ExitCode -ne 0) {
                $errorOutput = Get-Content $tempFile
                throw "Failed to set up Visual Studio environment: $errorOutput"
            }
        }
    }
    catch {
        Write-Error $_.Exception.Message
        return $false
    }
    finally {
        Cleanup-TempFiles @($tempFile)
    }

    return $true
}

# Function to check HIP SDK installation
function Test-HipSdk {
    $hipSdk = $env:HIP_PATH
    if (-not $hipSdk) {
        Write-Error "HIP_PATH environment variable not set. Please install HIP SDK."
        return $false
    }

    $hipInclude = Join-Path $hipSdk "include"
    if (-not (Test-Path $hipInclude)) {
        Write-Error "HIP SDK installation appears to be incomplete. Missing: $hipInclude"
        return $false
    }

    Write-VerboseOutput "HIP SDK found at: $hipSdk"
    return $true
}

# Function to validate vcpkg manifest
function Test-VcpkgManifest {
    param([string]$manifestPath)
    
    if (-not (Test-Path $manifestPath)) {
        Write-Error "vcpkg.json manifest file not found at: $manifestPath"
        return $false
    }
    
    try {
        $manifest = Get-Content $manifestPath -Raw | ConvertFrom-Json
        if (-not $manifest.name) {
            Write-Error "vcpkg.json is missing required 'name' field"
            return $false
        }
        if (-not $manifest.version) {
            Write-Error "vcpkg.json is missing required 'version' field"
            return $false
        }
        if (-not $manifest.dependencies) {
            Write-Error "vcpkg.json is missing required 'dependencies' field"
            return $false
        }
        return $true
    }
    catch {
        Write-Error "Failed to parse vcpkg.json: $($_.Exception.Message)"
        return $false
    }
}

# Function to clean build directory
function Clean-BuildDirectory {
    param([string]$buildDir)
    
    if (Test-Path $buildDir) {
        Write-VerboseOutput "Cleaning build directory..."
        try {
            Remove-Item -Path $buildDir -Recurse -Force
            New-Item -ItemType Directory -Path $buildDir | Out-Null
        }
        catch {
            Write-Error "Failed to clean build directory: $($_.Exception.Message)"
            return $false
        }
    }
    return $true
}

# Check for required tools and versions
$requiredTools = @{
    "cmake" = "3.20.0"
    "git" = "2.0.0"
}
$missingTools = @()
$outdatedTools = @()

foreach ($tool in $requiredTools.Keys) {
    if (-not (Test-Command $tool)) {
        $missingTools += $tool
    } else {
        $version = Get-CommandVersion $tool
        if ($version -and $version -lt [Version]$requiredTools[$tool]) {
            $outdatedTools += "$tool (found: $version, required: $($requiredTools[$tool]))"
        }
    }
}

if ($missingTools.Count -gt 0) {
    Write-Error "Missing required tools: $($missingTools -join ', ')"
    exit 1
}

if ($outdatedTools.Count -gt 0) {
    Write-Error "Outdated tools: $($outdatedTools -join ', ')"
    exit 1
}

# Setup Ninja
$ninjaExe = Setup-Ninja
Write-VerboseOutput "Using Ninja from: $ninjaExe"

# Find and setup Visual Studio
$vsPath = Find-VisualStudio
if (-not $vsPath) {
    exit 1
}

if (-not (Setup-VisualStudioEnvironment $vsPath)) {
    exit 1
}

Write-VerboseOutput "Visual Studio environment set up successfully"

# Verify Visual Studio environment
$clExe = Get-Command cl.exe -ErrorAction SilentlyContinue
if (-not $clExe) {
    Write-Error "Visual Studio environment not properly set up. cl.exe not found in PATH."
    exit 1
}

# Check for vcpkg
$vcpkgRoot = Join-Path $projectRoot "vcpkg"
if (-not (Test-Path $vcpkgRoot)) {
    Write-Error "vcpkg directory not found at: $vcpkgRoot"
    Write-Host "Please ensure vcpkg is cloned in the project root directory."
    exit 1
}

# Set up vcpkg environment
$env:VCPKG_DEFAULT_TRIPLET = "$BuildArch-windows"
$env:VCPKG_ROOT = $vcpkgRoot

# Ensure vcpkg is bootstrapped
$vcpkgExe = Join-Path $vcpkgRoot "vcpkg.exe"
if (-not (Test-Path $vcpkgExe)) {
    Write-VerboseOutput "Bootstrapping vcpkg..."
    Push-Location $vcpkgRoot
    try {
        $bootstrapOutput = & {
            $ErrorActionPreference = "Continue"
            & .\bootstrap-vcpkg.bat 2>&1
            $bootstrapResult = $LASTEXITCODE
            return $bootstrapResult, $bootstrapOutput
        }
        
        if ($bootstrapOutput[0] -ne 0) {
            throw "Failed to bootstrap vcpkg: $($bootstrapOutput[1])"
        }
        
        # Verify vcpkg executable was created
        if (-not (Test-Path $vcpkgExe)) {
            throw "vcpkg.exe was not created after bootstrapping"
        }
    }
    catch {
        Write-Error $_.Exception.Message
        Pop-Location
        exit 1
    }
    finally {
        Pop-Location
    }
}

# Verify vcpkg installation
if (-not (Test-Command vcpkg)) {
    Write-Error "vcpkg not found in PATH after bootstrapping"
    exit 1
}

# Check for Vulkan SDK
$vulkanSDK = $env:VULKAN_SDK
if (-not $vulkanSDK) {
    Write-Error "VULKAN_SDK environment variable not set. Please install Vulkan SDK."
    exit 1
}

# Verify Vulkan SDK installation
$vulkanInclude = Join-Path $vulkanSDK "Include\vulkan"
if (-not (Test-Path $vulkanInclude)) {
    Write-Error "Vulkan SDK installation appears to be incomplete. Missing: $vulkanInclude"
    exit 1
}

# Check for HIP SDK
if (-not (Test-HipSdk)) {
    exit 1
}

# Configure CMake
Write-VerboseOutput "Configuring CMake..."
$toolchainFile = Join-Path $vcpkgRoot "scripts/buildsystems/vcpkg.cmake"
if (-not (Test-Path $toolchainFile)) {
    Write-Error "vcpkg toolchain file not found at: $toolchainFile"
    exit 1
}

# Verify vcpkg manifest
$manifestFile = Join-Path $projectRoot "vcpkg.json"
if (-not (Test-VcpkgManifest $manifestFile)) {
    exit 1
}

# Set CMake variables for Visual Studio
$cmakeVars = @(
    "-DCMAKE_BUILD_TYPE=$BuildConfig",
    "-DCMAKE_TOOLCHAIN_FILE=`"$toolchainFile`"",
    "-DVCPKG_TARGET_TRIPLET=$BuildArch-windows",
    "-DVCPKG_HOST_TRIPLET=$BuildArch-windows",
    "-DVCPKG_VISUAL_STUDIO_PATH=`"$vsPath`"",
    "-DVCPKG_ROOT=`"$vcpkgRoot`"",
    "-DVCPKG_MANIFEST_DIR=`"$projectRoot`""
)

# Create and clean build directory
$buildDir = Join-Path $projectRoot "build"
if (-not (Clean-BuildDirectory $buildDir)) {
    exit 1
}

# Run CMake configuration with detailed error output
Write-VerboseOutput "Configuring CMake..."
$cmakeOutput = & {
    $ErrorActionPreference = "Continue"
    Push-Location $projectRoot
    $output = cmake -B build -S . -G "Ninja" $cmakeVars 2>&1
    $result = $LASTEXITCODE
    Pop-Location
    return $result, $output
}

if ($cmakeOutput[0] -ne 0) {
    Write-Error "CMake configuration failed. Full output:"
    Write-Error $cmakeOutput[1]
    exit 1
}

# Build the project
Write-VerboseOutput "Building project..."
$buildOutput = & {
    $ErrorActionPreference = "Continue"
    Push-Location $projectRoot
    $output = cmake --build build --config $BuildConfig 2>&1
    $result = $LASTEXITCODE
    Pop-Location
    return $result, $output
}

if ($buildOutput[0] -ne 0) {
    Write-Error "Build failed. Full output:"
    Write-Error $buildOutput[1]
    exit 1
}

Write-Host "Build completed successfully!"
$exePath = Join-Path $buildDir "$BuildConfig\cpp-vulkan-hip-engine.exe"
if (Test-Path $exePath) {
    Write-Host "Executable location: $exePath"
} else {
    Write-Host "Warning: Could not find the executable at the expected location."
} 
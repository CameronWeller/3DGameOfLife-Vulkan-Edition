# Simple build script for cpp-vulkan-hip-engine
param(
    [ValidateSet("Debug", "Release")]
    [string]$BuildConfig = "Debug"
)

Write-Host "Bob the Builder says: Can we fix it? Yes we can!" -ForegroundColor Yellow
Write-Host ""

# Get project root
$projectRoot = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
Set-Location $projectRoot

# Setup Visual Studio environment
Write-Host "🔧 Setting up Visual Studio environment..." -ForegroundColor Cyan

# Find Visual Studio installation
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (Test-Path $vswhere) {
    $vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if ($vsPath) {
        Write-Host "✓ Found Visual Studio at: $vsPath" -ForegroundColor Green
        
        # Import Visual Studio command prompt module if available
        $vsDevShell = Join-Path $vsPath "Common7\Tools\Microsoft.VisualStudio.DevShell.dll"
        if (Test-Path $vsDevShell) {
            Import-Module $vsDevShell
            Enter-VsDevShell -VsInstallPath $vsPath -SkipAutomaticLocation -DevCmdArguments "-arch=x64"
            Write-Host "✓ Visual Studio environment loaded" -ForegroundColor Green
        } else {
            # Fallback: manually set up paths
            $vcTools = Join-Path $vsPath "VC\Tools\MSVC"
            if (Test-Path $vcTools) {
                $vcVersion = Get-ChildItem $vcTools | Sort-Object -Descending | Select-Object -First 1
                $vcBin = Join-Path $vcVersion.FullName "bin\Hostx64\x64"
                $env:Path = "$vcBin;$env:Path"
                Write-Host "✓ Added VC tools to PATH" -ForegroundColor Green
            }
        }
    }
} else {
    Write-Warning "Visual Studio installer not found. Make sure Visual Studio is installed."
}

# Check prerequisites
Write-Host "🔨 Checking tools..." -ForegroundColor Cyan

# Check CMake
if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    Write-Error "CMake not found! Please install CMake and add it to PATH"
    exit 1
}
Write-Host "✓ CMake found" -ForegroundColor Green

# Check Git
if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
    Write-Error "Git not found! Please install Git"
    exit 1
}
Write-Host "✓ Git found" -ForegroundColor Green

# Check Vulkan SDK
if (-not $env:VULKAN_SDK) {
    # Try to find it in common locations
    $vulkanPath = "C:\VulkanSDK\1.4.313.0"
    if (Test-Path $vulkanPath) {
        $env:VULKAN_SDK = $vulkanPath
        Write-Host "✓ Set VULKAN_SDK to $vulkanPath" -ForegroundColor Green
    } else {
        Write-Error "VULKAN_SDK not set and not found at $vulkanPath"
        exit 1
    }
} else {
    Write-Host "✓ VULKAN_SDK found at $env:VULKAN_SDK" -ForegroundColor Green
}

# Setup vcpkg
Write-Host ""
Write-Host "🔧 Setting up vcpkg..." -ForegroundColor Cyan

$vcpkgRoot = Join-Path $projectRoot "vcpkg"
$vcpkgExe = Join-Path $vcpkgRoot "vcpkg.exe"

if (-not (Test-Path $vcpkgExe)) {
    Write-Host "Bootstrapping vcpkg..." -ForegroundColor Yellow
    Push-Location $vcpkgRoot
    try {
        & .\bootstrap-vcpkg.bat
        if ($LASTEXITCODE -ne 0) {
            throw "Failed to bootstrap vcpkg"
        }
    } finally {
        Pop-Location
    }
}
Write-Host "✓ vcpkg is ready" -ForegroundColor Green

# Set vcpkg environment
$env:VCPKG_ROOT = $vcpkgRoot

# Setup Ninja (optional but faster)
Write-Host ""
Write-Host "🥷 Setting up Ninja..." -ForegroundColor Cyan

$ninjaDir = Join-Path $projectRoot "tools\ninja"
$ninjaExe = Join-Path $ninjaDir "ninja.exe"

if (-not (Test-Path $ninjaExe)) {
    Write-Host "Downloading Ninja..." -ForegroundColor Yellow
    $ninjaUrl = "https://github.com/ninja-build/ninja/releases/download/v1.12.1/ninja-win.zip"
    $tempZip = Join-Path $env:TEMP "ninja.zip"
    
    if (-not (Test-Path $ninjaDir)) {
        New-Item -ItemType Directory -Path $ninjaDir -Force | Out-Null
    }
    
    try {
        Invoke-WebRequest -Uri $ninjaUrl -OutFile $tempZip
        Expand-Archive -Path $tempZip -DestinationPath $ninjaDir -Force
        Remove-Item $tempZip
    } catch {
        Write-Warning "Failed to download Ninja. Will use default generator."
    }
}

if (Test-Path $ninjaExe) {
    $env:Path = "$ninjaDir;$env:Path"
    Write-Host "✓ Ninja is ready" -ForegroundColor Green
    $generator = "Ninja"
} else {
    Write-Host "→ Using default CMake generator" -ForegroundColor Yellow
    $generator = ""
}

# Configure build
Write-Host ""
Write-Host "🏗️ Configuring build..." -ForegroundColor Cyan

$buildDir = Join-Path $projectRoot "build"
$toolchainFile = Join-Path $vcpkgRoot "scripts\buildsystems\vcpkg.cmake"

# Clean build directory
if (Test-Path $buildDir) {
    Write-Host "Cleaning build directory..." -ForegroundColor Yellow
    Remove-Item -Path $buildDir -Recurse -Force -ErrorAction SilentlyContinue
}
New-Item -ItemType Directory -Path $buildDir -Force | Out-Null

# CMake configuration
$cmakeArgs = @(
    "-S", ".",
    "-B", "build",
    "-DCMAKE_BUILD_TYPE=$BuildConfig",
    "-DCMAKE_TOOLCHAIN_FILE=$toolchainFile",
    "-DVCPKG_TARGET_TRIPLET=x64-windows"
)

if ($generator) {
    $cmakeArgs += "-G", $generator
}

Write-Host "Running: cmake $($cmakeArgs -join ' ')" -ForegroundColor DarkGray
& cmake $cmakeArgs

if ($LASTEXITCODE -ne 0) {
    Write-Error "CMake configuration failed!"
    exit 1
}
Write-Host "✓ Configuration complete" -ForegroundColor Green

# Build the project
Write-Host ""
Write-Host "🚀 Building project..." -ForegroundColor Cyan

& cmake --build build --config $BuildConfig --parallel

if ($LASTEXITCODE -ne 0) {
    Write-Error "Build failed!"
    exit 1
}

Write-Host ""
Write-Host "✨ Build completed successfully! ✨" -ForegroundColor Green
Write-Host ""

# Check for executable
$exeName = "cpp-vulkan-hip-engine.exe"
$possiblePaths = @(
    (Join-Path $buildDir $exeName),
    (Join-Path $buildDir "$BuildConfig\$exeName"),
    (Join-Path $buildDir "src\$exeName"),
    (Join-Path $buildDir "src\$BuildConfig\$exeName")
)

$exePath = $null
foreach ($path in $possiblePaths) {
    if (Test-Path $path) {
        $exePath = $path
        break
    }
}

if ($exePath) {
    Write-Host "🎯 Executable found at: $exePath" -ForegroundColor Green
    Write-Host ""
    Write-Host "To run the program:" -ForegroundColor Cyan
    Write-Host "  & `"$exePath`"" -ForegroundColor White
} else {
    Write-Warning "Could not find the executable. Check the build output above."
}

Write-Host ""
Write-Host "Bob says: Job's done! 👷‍♂️" -ForegroundColor Yellow 
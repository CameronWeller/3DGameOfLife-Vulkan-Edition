#!/usr/bin/env pwsh
# Comprehensive build diagnostics script

$ErrorActionPreference = "Stop"

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Build Environment Diagnostics" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# Get script directory and project root
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$projectRoot = Split-Path -Parent $scriptDir

Write-Host "Project Root: $projectRoot" -ForegroundColor Yellow
Write-Host ""

# Function to check if a command exists
function Test-Command($cmdname) {
    return [bool](Get-Command -Name $cmdname -ErrorAction SilentlyContinue)
}

# Function to check environment variable
function Check-EnvVar($varName, $required = $false) {
    $value = [Environment]::GetEnvironmentVariable($varName)
    if ($value) {
        Write-Host "✓ $varName = $value" -ForegroundColor Green
        return $true
    } else {
        if ($required) {
            Write-Host "✗ $varName (NOT SET - REQUIRED)" -ForegroundColor Red
        } else {
            Write-Host "- $varName (NOT SET - optional)" -ForegroundColor Gray
        }
        return $false
    }
}

# 1. Check Required Tools
Write-Host "1. Checking Required Tools:" -ForegroundColor Yellow
Write-Host "----------------------------" -ForegroundColor DarkGray

$tools = @{
    "cmake" = $true
    "git" = $true
    "ninja" = $false
    "cl" = $false
    "vcpkg" = $false
}

$toolsFound = @{}
foreach ($tool in $tools.Keys) {
    $cmd = Get-Command $tool -ErrorAction SilentlyContinue
    if ($cmd) {
        $toolsFound[$tool] = $true
        Write-Host "✓ $tool found at: $($cmd.Source)" -ForegroundColor Green
        if ($tool -eq "cmake") {
            $version = & cmake --version | Select-Object -First 1
            Write-Host "  Version: $version" -ForegroundColor DarkGray
        }
    } else {
        $toolsFound[$tool] = $false
        if ($tools[$tool]) {
            Write-Host "✗ $tool NOT FOUND (REQUIRED)" -ForegroundColor Red
        } else {
            Write-Host "- $tool not found (optional)" -ForegroundColor Gray
        }
    }
}
Write-Host ""

# 2. Check Visual Studio Installation
Write-Host "2. Checking Visual Studio:" -ForegroundColor Yellow
Write-Host "----------------------------" -ForegroundColor DarkGray

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (Test-Path $vswhere) {
    Write-Host "✓ vswhere found" -ForegroundColor Green
    
    # Get VS installations
    $vsInstalls = & $vswhere -all -format json | ConvertFrom-Json
    
    if ($vsInstalls.Count -gt 0) {
        Write-Host "✓ Visual Studio installations found:" -ForegroundColor Green
        foreach ($vs in $vsInstalls) {
            Write-Host "  - $($vs.displayName) at $($vs.installationPath)" -ForegroundColor DarkGray
            
            # Check for C++ workload
            $hasVC = $vs.packages | Where-Object { $_.id -like "*VC.Tools*" }
            if ($hasVC) {
                Write-Host "    ✓ C++ tools installed" -ForegroundColor Green
            } else {
                Write-Host "    ✗ C++ tools NOT installed" -ForegroundColor Red
            }
        }
    } else {
        Write-Host "✗ No Visual Studio installations found" -ForegroundColor Red
    }
} else {
    Write-Host "✗ vswhere not found - Visual Studio may not be installed" -ForegroundColor Red
}
Write-Host ""

# 3. Check Environment Variables
Write-Host "3. Checking Environment Variables:" -ForegroundColor Yellow
Write-Host "----------------------------" -ForegroundColor DarkGray

Check-EnvVar "VULKAN_SDK" $true
Check-EnvVar "HIP_PATH" $false
Check-EnvVar "VCPKG_ROOT" $false
Check-EnvVar "VSINSTALLDIR" $false
Check-EnvVar "VCINSTALLDIR" $false
Write-Host ""

# 4. Check Vulkan SDK
Write-Host "4. Checking Vulkan SDK:" -ForegroundColor Yellow
Write-Host "----------------------------" -ForegroundColor DarkGray

$vulkanSDK = $env:VULKAN_SDK
if ($vulkanSDK -and (Test-Path $vulkanSDK)) {
    Write-Host "✓ Vulkan SDK found at: $vulkanSDK" -ForegroundColor Green
    
    # Check for glslc
    $glslc = Join-Path $vulkanSDK "Bin\glslc.exe"
    if (Test-Path $glslc) {
        Write-Host "✓ glslc compiler found" -ForegroundColor Green
    } else {
        Write-Host "✗ glslc compiler NOT found" -ForegroundColor Red
    }
    
    # Check for include files
    $vulkanInclude = Join-Path $vulkanSDK "Include\vulkan"
    if (Test-Path $vulkanInclude) {
        Write-Host "✓ Vulkan headers found" -ForegroundColor Green
    } else {
        Write-Host "✗ Vulkan headers NOT found" -ForegroundColor Red
    }
} else {
    Write-Host "✗ Vulkan SDK not properly installed" -ForegroundColor Red
}
Write-Host ""

# 5. Check vcpkg
Write-Host "5. Checking vcpkg:" -ForegroundColor Yellow
Write-Host "----------------------------" -ForegroundColor DarkGray

$vcpkgRoot = Join-Path $projectRoot "vcpkg"
if (Test-Path $vcpkgRoot) {
    Write-Host "✓ vcpkg directory found at: $vcpkgRoot" -ForegroundColor Green
    
    $vcpkgExe = Join-Path $vcpkgRoot "vcpkg.exe"
    if (Test-Path $vcpkgExe) {
        Write-Host "✓ vcpkg.exe found" -ForegroundColor Green
        
        # Check vcpkg version
        Push-Location $vcpkgRoot
        $vcpkgVersion = & $vcpkgExe version 2>&1 | Select-Object -First 3
        Pop-Location
        Write-Host "  Version info:" -ForegroundColor DarkGray
        $vcpkgVersion | ForEach-Object { Write-Host "    $_" -ForegroundColor DarkGray }
    } else {
        Write-Host "✗ vcpkg.exe NOT found - needs bootstrapping" -ForegroundColor Red
    }
    
    # Check for vcpkg.json
    $manifestFile = Join-Path $projectRoot "vcpkg.json"
    if (Test-Path $manifestFile) {
        Write-Host "✓ vcpkg.json manifest found" -ForegroundColor Green
    } else {
        Write-Host "✗ vcpkg.json manifest NOT found" -ForegroundColor Red
    }
} else {
    Write-Host "✗ vcpkg directory NOT found" -ForegroundColor Red
}
Write-Host ""

# 6. Check Windows SDK
Write-Host "6. Checking Windows SDK:" -ForegroundColor Yellow
Write-Host "----------------------------" -ForegroundColor DarkGray

$windowsKitsRoot = "${env:ProgramFiles(x86)}\Windows Kits\10"
if (Test-Path $windowsKitsRoot) {
    Write-Host "✓ Windows Kits found at: $windowsKitsRoot" -ForegroundColor Green
    
    # Find SDK versions
    $sdkBinPath = Join-Path $windowsKitsRoot "bin"
    if (Test-Path $sdkBinPath) {
        $sdkVersions = Get-ChildItem $sdkBinPath -Directory | Where-Object { $_.Name -match '^\d+\.\d+\.\d+\.\d+$' }
        if ($sdkVersions.Count -gt 0) {
            Write-Host "✓ Windows SDK versions found:" -ForegroundColor Green
            foreach ($version in $sdkVersions) {
                Write-Host "  - $($version.Name)" -ForegroundColor DarkGray
                
                # Check for specific tools
                $mtExe = Join-Path $version.FullName "x64\mt.exe"
                $rcExe = Join-Path $version.FullName "x64\rc.exe"
                
                if ((Test-Path $mtExe) -and (Test-Path $rcExe)) {
                    Write-Host "    ✓ SDK tools present" -ForegroundColor Green
                } else {
                    Write-Host "    ✗ SDK tools missing" -ForegroundColor Red
                }
            }
        } else {
            Write-Host "✗ No Windows SDK versions found" -ForegroundColor Red
        }
    }
} else {
    Write-Host "✗ Windows Kits NOT found" -ForegroundColor Red
}
Write-Host ""

# 7. Check Project Files
Write-Host "7. Checking Project Files:" -ForegroundColor Yellow
Write-Host "----------------------------" -ForegroundColor DarkGray

$requiredFiles = @(
    "CMakeLists.txt",
    "vcpkg.json",
    "src\main.cpp"
)

foreach ($file in $requiredFiles) {
    $filePath = Join-Path $projectRoot $file
    if (Test-Path $filePath) {
        Write-Host "✓ $file found" -ForegroundColor Green
    } else {
        Write-Host "✗ $file NOT found" -ForegroundColor Red
    }
}
Write-Host ""

# 8. Summary
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Summary:" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan

$issues = @()

if (-not $toolsFound["cmake"]) {
    $issues += "CMake is not installed"
}

if (-not $env:VULKAN_SDK) {
    $issues += "Vulkan SDK is not installed or VULKAN_SDK environment variable not set"
}

if (-not (Test-Path "${env:ProgramFiles(x86)}\Windows Kits\10")) {
    $issues += "Windows SDK is not installed"
}

if (-not (Test-Path (Join-Path $projectRoot "vcpkg"))) {
    $issues += "vcpkg is not cloned in the project directory"
}

if ($issues.Count -eq 0) {
    Write-Host "✓ All prerequisites appear to be satisfied!" -ForegroundColor Green
    Write-Host ""
    Write-Host "Next steps:" -ForegroundColor Yellow
    Write-Host "1. If vcpkg.exe is missing, run: cd vcpkg && .\bootstrap-vcpkg.bat" -ForegroundColor White
    Write-Host "2. Run the build script: .\scripts\build.ps1" -ForegroundColor White
} else {
    Write-Host "✗ Found the following issues:" -ForegroundColor Red
    foreach ($issue in $issues) {
        Write-Host "  - $issue" -ForegroundColor Red
    }
    Write-Host ""
    Write-Host "Please resolve these issues before attempting to build." -ForegroundColor Yellow
} 
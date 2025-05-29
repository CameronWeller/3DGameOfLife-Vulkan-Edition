# Script to set up Visual Studio development environment
[CmdletBinding()]
param()

# Function to find Visual Studio installation using vswhere
function Find-VisualStudio {
    # First try to find vswhere in Program Files
    $vswherePaths = @(
        "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe",
        "${env:ProgramFiles}\Microsoft Visual Studio\Installer\vswhere.exe"
    )
    
    $vswhere = $null
    foreach ($path in $vswherePaths) {
        if (Test-Path $path) {
            $vswhere = $path
            break
        }
    }
    
    if ($null -eq $vswhere) {
        Write-Host "vswhere.exe not found in standard locations. Checking common installation paths..." -ForegroundColor Yellow
        
        # Fallback to checking common installation paths
        $vsPaths = @(
            "C:\Program Files\Microsoft Visual Studio\2022",
            "C:\Program Files (x86)\Microsoft Visual Studio\2022",
            "${env:ProgramFiles}\Microsoft Visual Studio\2022",
            "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022"
        )
        
        foreach ($path in $vsPaths) {
            if (Test-Path $path) {
                $editions = Get-ChildItem $path -Directory
                foreach ($edition in $editions) {
                    $vsWhere = Join-Path $edition.FullName "Common7\IDE\vswhere.exe"
                    if (Test-Path $vsWhere) {
                        return $edition.FullName
                    }
                }
            }
        }
    }
    else {
        # Use vswhere to find Visual Studio installation
        Write-Host "Using vswhere to find Visual Studio installation..." -ForegroundColor Yellow
        $vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        if ($vsPath) {
            return $vsPath
        }
    }
    
    return $null
}

# Function to set up Visual Studio environment
function Set-VisualStudioEnvironment {
    param (
        [string]$vsPath
    )
    
    $vsDevCmd = Join-Path $vsPath "Common7\Tools\Launch-VsDevShell.ps1"
    if (Test-Path $vsDevCmd) {
        Write-Host "Setting up Visual Studio environment..." -ForegroundColor Yellow
        & $vsDevCmd -Arch amd64 -HostArch amd64 -SkipAutomaticLocation
        return $true
    }
    return $false
}

# Main execution
try {
    Write-Host "Looking for Visual Studio 2022 installation..." -ForegroundColor Yellow
    $vsPath = Find-VisualStudio
    
    if ($null -eq $vsPath) {
        Write-Error "Visual Studio 2022 not found. Please install Visual Studio 2022 with C++ development tools."
        Write-Host "Download from: https://visualstudio.microsoft.com/vs/community/" -ForegroundColor Yellow
        Write-Host "Required components:" -ForegroundColor Yellow
        Write-Host "  - Desktop development with C++" -ForegroundColor Yellow
        Write-Host "  - Windows 10/11 SDK" -ForegroundColor Yellow
        Write-Host "  - C++ CMake tools for Windows" -ForegroundColor Yellow
        exit 1
    }
    
    Write-Host "Found Visual Studio at: $vsPath" -ForegroundColor Green
    
    if (Set-VisualStudioEnvironment $vsPath) {
        Write-Host "Visual Studio environment set up successfully!" -ForegroundColor Green
        Write-Host "You can now run the setup script again:" -ForegroundColor Green
        Write-Host ".\scripts\setup\main.ps1 -Admin" -ForegroundColor Green
    }
    else {
        Write-Error "Failed to set up Visual Studio environment"
        exit 1
    }
}
catch {
    Write-Error "Setup failed: $_"
    exit 1
} 
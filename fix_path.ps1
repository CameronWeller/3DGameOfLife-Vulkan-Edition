# Function to find a program in PATH
function Find-Program {
    param (
        [string]$programName
    )
    
    $paths = $env:Path -split ';'
    foreach ($path in $paths) {
        $fullPath = Join-Path $path $programName
        if (Test-Path $fullPath) {
            return $fullPath
        }
    }
    return $null
}

# Function to verify and get PowerShell path
function Get-PowerShellPath {
    $possiblePaths = @(
        "C:\Program Files\PowerShell\7\pwsh.exe",
        "C:\Program Files\PowerShell\7\powershell.exe",
        "C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe"
    )
    
    foreach ($path in $possiblePaths) {
        if (Test-Path $path) {
            return $path
        }
    }
    
    # Try to find PowerShell in PATH
    $psPath = Find-Program "pwsh.exe"
    if ($psPath) { return $psPath }
    
    $psPath = Find-Program "powershell.exe"
    if ($psPath) { return $psPath }
    
    Write-Host "Error: Could not find PowerShell installation"
    return $null
}

# Function to verify and get Git path
function Get-GitPath {
    $gitPath = Find-Program "git.exe"
    if (-not $gitPath) {
        Write-Host "Error: Git is not installed or not in PATH"
        return $null
    }
    return $gitPath
}

# Function to verify and get CMake path
function Get-CMakePath {
    $cmakePath = Find-Program "cmake.exe"
    if (-not $cmakePath) {
        Write-Host "Error: CMake is not installed or not in PATH"
        return $null
    }
    return $cmakePath
}

# Main script execution
Write-Host "Verifying required tools..."

# Check PowerShell
$psPath = Get-PowerShellPath
if (-not $psPath) {
    Write-Host "Please install PowerShell 7 from: https://aka.ms/install-powershell"
    exit 1
}
Write-Host "Found PowerShell at: $psPath"

# Check Git
$gitPath = Get-GitPath
if (-not $gitPath) {
    Write-Host "Please install Git from: https://git-scm.com/download/win"
    exit 1
}
Write-Host "Found Git at: $gitPath"

# Check CMake
$cmakePath = Get-CMakePath
if (-not $cmakePath) {
    Write-Host "Please install CMake from: https://cmake.org/download/"
    exit 1
}
Write-Host "Found CMake at: $cmakePath"

# Verify vcpkg installation
$vcpkgPath = "C:\vcpkg"
if (-not (Test-Path $vcpkgPath)) {
    Write-Host "Installing vcpkg..."
    & $gitPath clone https://github.com/Microsoft/vcpkg.git $vcpkgPath
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Error: Failed to clone vcpkg repository"
        exit 1
    }
}

# Verify vcpkg bootstrap script exists
$bootstrapPath = Join-Path $vcpkgPath "bootstrap-vcpkg.bat"
if (-not (Test-Path $bootstrapPath)) {
    Write-Host "Error: vcpkg bootstrap script not found at: $bootstrapPath"
    exit 1
}

# Create bootstrap script
$bootstrapContent = @"
# Bootstrap vcpkg
Set-ExecutionPolicy Bypass -Scope Process -Force
& '$bootstrapPath'
"@

$bootstrapScriptPath = Join-Path $PSScriptRoot "bootstrap_vcpkg.ps1"
$bootstrapContent | Out-File -FilePath $bootstrapScriptPath -Encoding ASCII

Write-Host "`nSetup completed successfully!"
Write-Host "To bootstrap vcpkg, run: .\bootstrap_vcpkg.ps1" 
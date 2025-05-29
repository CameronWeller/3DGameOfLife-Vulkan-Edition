[CmdletBinding()]
param(
    [Parameter(Mandatory=$False)][switch]$disableMetrics = $false
)

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

# Function to find vcpkg root directory
function Get-VcpkgRoot {
    $projectRoot = Split-Path -Parent (Split-Path -Parent $script:MyInvocation.MyCommand.Definition)
    $vcpkgRootDir = Join-Path $projectRoot "vcpkg"
    
    if (-not (Test-Path $vcpkgRootDir)) {
        Write-Host "vcpkg directory not found. Cloning vcpkg..." -ForegroundColor Yellow
        git clone https://github.com/Microsoft/vcpkg.git $vcpkgRootDir
        if ($LASTEXITCODE -ne 0) {
            throw "Failed to clone vcpkg repository"
        }
    }
    
    if (-not (Test-Path (Join-Path $vcpkgRootDir ".vcpkg-root"))) {
        Write-Host "Bootstrapping vcpkg..." -ForegroundColor Yellow
        Push-Location $vcpkgRootDir
        & .\bootstrap-vcpkg.bat
        if ($LASTEXITCODE -ne 0) {
            Pop-Location
            throw "Failed to bootstrap vcpkg"
        }
        Pop-Location
    }
    
    return $vcpkgRootDir
}

# Main execution
try {
    # Validate environment
    Test-Environment
    
    # Get vcpkg root directory
    $vcpkgRootDir = Get-VcpkgRoot
    Write-Host "Found vcpkg root at: $vcpkgRootDir"
    
    # Read the vcpkg-tool config file
    $configPath = Join-Path $vcpkgRootDir "scripts\vcpkg-tool-metadata.txt"
    if (-not (Test-Path $configPath)) {
        throw "vcpkg-tool-metadata.txt not found at: $configPath"
    }
    
    $Config = ConvertFrom-StringData (Get-Content $configPath -Raw)
    $versionDate = $Config.VCPKG_TOOL_RELEASE_TAG
    
    # Determine architecture and download appropriate version
    $isArm64 = $env:PROCESSOR_ARCHITECTURE -eq 'ARM64' -or $env:PROCESSOR_IDENTIFIER -match "ARMv[8,9] \(64-bit\)"
    $downloadScript = if ($isArm64) { "tls12-download-arm64.exe" } else { "tls12-download.exe" }
    $downloadScriptPath = Join-Path $vcpkgRootDir "scripts\$downloadScript"
    
    if (-not (Test-Path $downloadScriptPath)) {
        throw "Download script not found at: $downloadScriptPath"
    }
    
    Write-Host "Downloading vcpkg executable..."
    & $downloadScriptPath github.com "/microsoft/vcpkg-tool/releases/download/$versionDate/vcpkg.exe" "$vcpkgRootDir\vcpkg.exe"
    
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to download vcpkg.exe. Please check your internet connection or download manually from https://github.com/microsoft/vcpkg-tool"
    }
    
    # Verify the download
    if (-not (Test-Path "$vcpkgRootDir\vcpkg.exe")) {
        throw "vcpkg.exe was not downloaded successfully"
    }
    
    # Run vcpkg version check
    Write-Host "Verifying vcpkg installation..."
    & "$vcpkgRootDir\vcpkg.exe" version --disable-metrics
    
    # Handle metrics
    if ($disableMetrics) {
        Set-Content -Value "" -Path "$vcpkgRootDir\vcpkg.disable-metrics" -Force
        Write-Host "Metrics disabled" -ForegroundColor Green
    }
    elseif (-not (Test-Path "$vcpkgRootDir\vcpkg.disable-metrics")) {
        Write-Host @"
Telemetry
---------
vcpkg collects usage data in order to help us improve your experience.
The data collected by Microsoft is anonymous.
You can opt-out of telemetry by re-running the bootstrap-vcpkg script with -disableMetrics,
passing --disable-metrics to vcpkg on the command line,
or by setting the VCPKG_DISABLE_METRICS environment variable.

Read more about vcpkg telemetry at docs/about/privacy.md
"@
    }
    
    Write-Host "vcpkg bootstrap completed successfully" -ForegroundColor Green
    exit 0
}
catch {
    Write-Error "Bootstrap failed: $_"
    exit 1
} 
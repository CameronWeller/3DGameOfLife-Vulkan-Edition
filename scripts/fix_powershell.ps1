# Run as Administrator
if (-NOT ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole] "Administrator")) {
    Write-Warning "Please run this script as Administrator!"
    exit
}

# Get script directory and project root
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$projectRoot = Split-Path -Parent $scriptDir

# Stop any running PowerShell processes
Write-Host "Stopping existing PowerShell processes..."
Get-Process pwsh -ErrorAction SilentlyContinue | Stop-Process -Force

# Uninstall existing PowerShell 7
Write-Host "Uninstalling existing PowerShell 7..."
$programFiles = [Environment]::GetFolderPath("ProgramFiles")
$powershellPath = Join-Path $programFiles "PowerShell\7"
$uninstaller = Get-ChildItem (Join-Path $powershellPath "unins*.exe") -ErrorAction SilentlyContinue
if ($uninstaller) {
    Start-Process -FilePath $uninstaller.FullName -ArgumentList "/VERYSILENT /SUPPRESSMSGBOXES" -Wait
}

# Download latest PowerShell 7
Write-Host "Downloading latest PowerShell 7..."
$url = "https://aka.ms/install-powershell.ps1"
$installer = Join-Path $env:TEMP "install-powershell.ps1"
try {
    Invoke-WebRequest -Uri $url -OutFile $installer -ErrorAction Stop
}
catch {
    Write-Error "Failed to download PowerShell installer: $($_.Exception.Message)"
    exit 1
}

# Install PowerShell 7
Write-Host "Installing PowerShell 7..."
try {
    & $installer -Quiet -UseMSI
    if ($LASTEXITCODE -ne 0) {
        throw "PowerShell installation failed with exit code $LASTEXITCODE"
    }
}
catch {
    Write-Error "Failed to install PowerShell: $($_.Exception.Message)"
    Remove-Item $installer -Force -ErrorAction SilentlyContinue
    exit 1
}

# Clean up
Remove-Item $installer -Force -ErrorAction SilentlyContinue

# Add to PATH
Write-Host "Setting up PATH..."
$currentPath = [Environment]::GetEnvironmentVariable("Path", "Machine")
if (-not $currentPath.Contains($powershellPath)) {
    $newPath = $currentPath + ";" + $powershellPath
    [Environment]::SetEnvironmentVariable("Path", $newPath, "Machine")
}

# Verify installation
Write-Host "Verifying installation..."
$pwsh = Get-Command pwsh -ErrorAction SilentlyContinue
if ($pwsh) {
    Write-Host "PowerShell 7 installed successfully!"
    Write-Host "Version: $($pwsh.Version)"
} else {
    Write-Error "Installation failed. Please try manual installation."
    exit 1
}

# Run bootstrap script
Write-Host "Running vcpkg bootstrap..."
$vcpkgPath = Join-Path $projectRoot "vcpkg"
if (Test-Path $vcpkgPath) {
    Set-Location -Path $vcpkgPath
    try {
        & (Join-Path $powershellPath "pwsh.exe") -Command ".\bootstrap-vcpkg.bat"
        if ($LASTEXITCODE -ne 0) {
            throw "vcpkg bootstrap failed with exit code $LASTEXITCODE"
        }
    }
    catch {
        Write-Error "Failed to bootstrap vcpkg: $($_.Exception.Message)"
        Set-Location -Path $projectRoot
        exit 1
    }
    Set-Location -Path $projectRoot
} else {
    Write-Warning "vcpkg directory not found at: $vcpkgPath"
}

Write-Host "PowerShell installation and setup complete!" 
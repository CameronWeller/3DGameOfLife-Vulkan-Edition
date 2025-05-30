# Install and Update Chocolatey
Write-Host "Setting up Chocolatey..." -ForegroundColor Green

# Check if running as administrator
$isAdmin = ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) {
    Write-Error "This script must be run as Administrator. Please restart PowerShell as Administrator."
    exit 1
}

# Function to test if a command exists
function Test-Command($cmdname) {
    return [bool](Get-Command -Name $cmdname -ErrorAction SilentlyContinue)
}

# Check if Chocolatey is already installed
$chocoPath = "C:\ProgramData\chocolatey\bin\choco.exe"
if (Test-Path $chocoPath) {
    Write-Host "Chocolatey is already installed. Updating..." -ForegroundColor Yellow
    try {
        & $chocoPath upgrade chocolatey -y
        if ($LASTEXITCODE -ne 0) {
            Write-Warning "Failed to update Chocolatey. Attempting reinstall..."
            Remove-Item -Path "C:\ProgramData\chocolatey" -Recurse -Force -ErrorAction SilentlyContinue
        } else {
            Write-Host "Chocolatey updated successfully!" -ForegroundColor Green
            exit 0
        }
    } catch {
        Write-Warning "Error updating Chocolatey: $_"
        Write-Host "Attempting fresh installation..." -ForegroundColor Yellow
    }
}

# Set execution policy
Write-Host "Setting execution policy..." -ForegroundColor Cyan
Set-ExecutionPolicy Bypass -Scope Process -Force
Set-ExecutionPolicy Bypass -Scope CurrentUser -Force

# Set TLS to 1.2
Write-Host "Setting TLS to 1.2..." -ForegroundColor Cyan
[System.Net.ServicePointManager]::SecurityProtocol = [System.Net.ServicePointManager]::SecurityProtocol -bor 3072

# Install Chocolatey
Write-Host "Installing Chocolatey..." -ForegroundColor Cyan
try {
    iex ((New-Object System.Net.WebClient).DownloadString('https://chocolatey.org/install.ps1'))
    if ($LASTEXITCODE -ne 0) {
        throw "Chocolatey installation failed with exit code $LASTEXITCODE"
    }
} catch {
    Write-Error "Failed to install Chocolatey: $_"
    exit 1
}

# Refresh environment variables
Write-Host "Refreshing environment variables..." -ForegroundColor Cyan
$env:Path = [System.Environment]::GetEnvironmentVariable("Path","Machine") + ";" + [System.Environment]::GetEnvironmentVariable("Path","User")

# Verify installation
if (Test-Command "choco") {
    Write-Host "Chocolatey installed successfully!" -ForegroundColor Green
    Write-Host "Version: $(choco --version)" -ForegroundColor Gray
} else {
    Write-Error "Chocolatey installation failed. Please check the error messages above."
    exit 1
}

# Configure Chocolatey
Write-Host "`nConfiguring Chocolatey..." -ForegroundColor Cyan
choco feature enable -n=exitOnRebootDetected
choco feature enable -n=useRememberedArgumentsForUpgrades
choco feature enable -n=useFipsCompliantChecksums
choco feature enable -n=useEnhancedExitCodes

# Update all packages
Write-Host "`nUpdating all packages..." -ForegroundColor Cyan
choco upgrade all -y

Write-Host "`nChocolatey setup completed!" -ForegroundColor Green
Write-Host "Note: You may need to restart your terminal for all changes to take effect." -ForegroundColor Yellow 
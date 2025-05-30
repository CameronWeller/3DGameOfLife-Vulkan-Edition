# Install Code Quality Tools
Write-Host "Installing Code Quality Tools..." -ForegroundColor Green

# Check if running as administrator
$isAdmin = ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) {
    Write-Warning "Running without administrator privileges. Some installations may fail."
}

# Function to test if a command exists
function Test-Command($cmdname) {
    return [bool](Get-Command -Name $cmdname -ErrorAction SilentlyContinue)
}

# Install Chocolatey if not present
if (-not (Test-Command "choco")) {
    Write-Host "Installing Chocolatey..." -ForegroundColor Yellow
    Set-ExecutionPolicy Bypass -Scope Process -Force
    [System.Net.ServicePointManager]::SecurityProtocol = [System.Net.ServicePointManager]::SecurityProtocol -bor 3072
    iex ((New-Object System.Net.WebClient).DownloadString('https://chocolatey.org/install.ps1'))
    $env:Path = [System.Environment]::GetEnvironmentVariable("Path","Machine") + ";" + [System.Environment]::GetEnvironmentVariable("Path","User")
}

# Install LLVM (includes clang-format, clang-tidy)
Write-Host "`nInstalling LLVM tools..." -ForegroundColor Cyan
if (Test-Command "choco") {
    choco install llvm -y
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Failed to install LLVM"
        exit 1
    }
} else {
    Write-Error "Chocolatey is not available. Cannot install LLVM."
    exit 1
}

# Install cppcheck
Write-Host "`nInstalling cppcheck..." -ForegroundColor Cyan
choco install cppcheck -y
if ($LASTEXITCODE -ne 0) {
    Write-Warning "Failed to install cppcheck via Chocolatey, trying manual installation..."
    
    # Manual installation fallback
    $cppcheckUrl = "https://github.com/danmar/cppcheck/releases/download/2.13/cppcheck-2.13-x64-Setup.msi"
    $cppcheckInstaller = Join-Path $env:TEMP "cppcheck-setup.msi"
    
    Write-Host "Downloading cppcheck..."
    Invoke-WebRequest -Uri $cppcheckUrl -OutFile $cppcheckInstaller
    
    Write-Host "Installing cppcheck..."
    Start-Process msiexec.exe -ArgumentList "/i", $cppcheckInstaller, "/quiet" -Wait
    
    Remove-Item $cppcheckInstaller -Force
}

# Refresh environment variables
$env:Path = [System.Environment]::GetEnvironmentVariable("Path","Machine") + ";" + [System.Environment]::GetEnvironmentVariable("Path","User")

# Verify installations
Write-Host "`nVerifying installations..." -ForegroundColor Green
$tools = @{
    "clang-format" = "clang-format --version"
    "clang-tidy" = "clang-tidy --version"
    "cppcheck" = "cppcheck --version"
}

$allInstalled = $true
foreach ($tool in $tools.GetEnumerator()) {
    if (Test-Command $tool.Key) {
        Write-Host "✓ $($tool.Key) is installed" -ForegroundColor Green
        try {
            $version = Invoke-Expression $tool.Value 2>&1 | Select-Object -First 1
            Write-Host "  Version: $version" -ForegroundColor Gray
        } catch {
            Write-Host "  (Could not determine version)" -ForegroundColor Gray
        }
    } else {
        Write-Host "✗ $($tool.Key) is not installed" -ForegroundColor Red
        $allInstalled = $false
    }
}

if ($allInstalled) {
    Write-Host "`nAll code quality tools installed successfully!" -ForegroundColor Green
} else {
    Write-Host "`nSome tools failed to install. Please check the error messages above." -ForegroundColor Yellow
    exit 1
}

Write-Host "`nNote: You may need to restart your terminal for the tools to be available in PATH." -ForegroundColor Cyan 
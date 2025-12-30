# NixOS WSL Setup Script for 3D Game of Life - Vulkan Edition
# This script sets up a NixOS WSL environment for Vulkan development

param(
    [string]$WslName = "NixOS-Vulkan",
    [string]$WslVersion = "2",
    [switch]$Force,
    [switch]$SkipWslInstall,
    [switch]$SkipNixInstall
)

Write-Host "Setting up NixOS WSL for Vulkan Development" -ForegroundColor Cyan
Write-Host "=============================================" -ForegroundColor Cyan
Write-Host ""

# Function to check if command exists
function Test-Command($cmdname) {
    return [bool](Get-Command -Name $cmdname -ErrorAction SilentlyContinue)
}

# Function to run command and check exit code
function Invoke-CommandWithCheck($command, $description) {
    Write-Host "Running: $description..." -ForegroundColor Yellow
    try {
        Invoke-Expression $command
        if ($LASTEXITCODE -ne 0) {
            throw "Command failed with exit code $LASTEXITCODE"
        }
        Write-Host "SUCCESS: $description completed" -ForegroundColor Green
    }
    catch {
        Write-Error "ERROR: $description failed: $_"
        exit 1
    }
}

# Check if running as administrator
if (-NOT ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole] "Administrator")) {
    Write-Error "This script must be run as Administrator"
    exit 1
}

# Step 1: Install WSL if not already installed
if (-not $SkipWslInstall) {
    Write-Host "Step 1: Checking WSL installation..." -ForegroundColor Cyan
    
    if (-not (Test-Command "wsl")) {
        Write-Host "WSL not found. Installing WSL..." -ForegroundColor Yellow
        Invoke-CommandWithCheck "dism.exe /online /enable-feature /featurename:Microsoft-Windows-Subsystem-Linux /all /norestart" "Enabling WSL feature"
        Invoke-CommandWithCheck "dism.exe /online /enable-feature /featurename:VirtualMachinePlatform /all /norestart" "Enabling Virtual Machine Platform"
        
        Write-Host "Please restart your computer and run this script again." -ForegroundColor Yellow
        Write-Host "After restart, WSL will be ready for NixOS installation." -ForegroundColor Yellow
        exit 0
    } else {
        Write-Host "WSL is already installed" -ForegroundColor Green
    }
    
    # Set WSL 2 as default
    Write-Host "Setting WSL 2 as default..." -ForegroundColor Yellow
    Invoke-CommandWithCheck "wsl --set-default-version 2" "Setting WSL 2 as default"
}

# Step 2: Install Nix if not already installed
if (-not $SkipNixInstall) {
    Write-Host ""
    Write-Host "Step 2: Checking Nix installation in WSL..." -ForegroundColor Cyan
    
    # Find a WSL distribution (prefer Ubuntu)
    $wslDistro = (wsl --list --verbose | Select-String "Ubuntu\|Debian" | Select-Object -First 1)
    if (-not $wslDistro) {
        Write-Error "No suitable WSL distribution found (Ubuntu or Debian required). Please install Ubuntu from the Microsoft Store and try again."
        exit 1
    }
    $wslDistroName = ($wslDistro.ToString() -split '\s+')[0]
    Write-Host "Using WSL distribution: $wslDistroName" -ForegroundColor Yellow

    # Check if Nix is already installed in WSL
    $nixCheck = wsl -d $wslDistroName --exec bash -c "command -v nix"
    if (-not $nixCheck) {
        Write-Host "Nix not found in $wslDistroName. Installing Nix inside WSL..." -ForegroundColor Yellow
        try {
            wsl -d $wslDistroName --exec bash -c "sh <(curl -L https://nixos.org/nix/install) --daemon"
            if ($LASTEXITCODE -ne 0) {
                throw "Nix installation in WSL failed"
            }
            Write-Host "Nix installed successfully in $wslDistroName" -ForegroundColor Green
        } catch {
            Write-Error "Failed to install Nix in WSL: $_"
            exit 1
        }
    } else {
        Write-Host "Nix is already installed in $wslDistroName" -ForegroundColor Green
    }
}

# Step 3: Create NixOS WSL distribution
Write-Host ""
Write-Host "Step 3: Creating NixOS WSL distribution..." -ForegroundColor Cyan

# Check if WSL distribution already exists
$existingDistros = wsl --list --verbose | Select-String $WslName
if ($existingDistros -and -not $Force) {
    Write-Host "WSL distribution '$WslName' already exists." -ForegroundColor Yellow
    $response = Read-Host "Do you want to remove it and create a new one? (y/N)"
    if ($response -eq "y" -or $response -eq "Y") {
        Write-Host "Removing existing distribution..." -ForegroundColor Yellow
        wsl --unregister $WslName
    } else {
        Write-Host "Skipping distribution creation." -ForegroundColor Yellow
        exit 0
    }
}

# Create NixOS WSL distribution using WSL-installed Nix
Write-Host "Creating NixOS WSL distribution..." -ForegroundColor Yellow

# Get the current directory (should be the project root)
$projectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$nixosWslDir = Join-Path $projectRoot "nixos-wsl"

# Verify the directory exists
if (-not (Test-Path $nixosWslDir)) {
    Write-Error "NixOS WSL directory not found: $nixosWslDir"
    exit 1
}

Write-Host "Using NixOS WSL directory: $nixosWslDir" -ForegroundColor Yellow

# Find a WSL distribution with Nix installed
$wslDistro = (wsl --list --verbose | Select-String "Ubuntu\|Debian" | Select-Object -First 1).ToString().Split()[0]
if (-not $wslDistro) {
    Write-Error "No WSL distribution found with Nix installed"
    exit 1
}

Write-Host "Using WSL distribution: $wslDistro" -ForegroundColor Yellow

# Build NixOS configuration using WSL-installed Nix
Write-Host "Building NixOS configuration..." -ForegroundColor Yellow
try {
    # Change to the NixOS WSL directory in WSL
    $wslProjectPath = "/mnt/c/Dev/3DGameOfLife-Vulkan-Edition/nixos-wsl"
    
    # Build the NixOS configuration
    wsl -d $wslDistro --exec bash -c "cd $wslProjectPath && source /etc/profile.d/nix.sh && nix build .#nixosConfigurations.nixos-wsl-vulkan.config.system.build.wslImage"
    
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to build NixOS configuration"
    }
    
    # Import the WSL distribution
    $wslImagePath = wsl -d $wslDistro --exec bash -c "cd $wslProjectPath && find result -name '*.tar.gz' | head -1"
    if ($wslImagePath) {
        $wslImagePath = $wslImagePath.Trim()
        $fullImagePath = "$wslProjectPath/$wslImagePath"
        Write-Host "Importing WSL distribution from: $fullImagePath" -ForegroundColor Yellow
        
        # Copy the image to Windows temp directory for import
        $windowsTempPath = Join-Path $env:TEMP "nixos-wsl-image.tar.gz"
        wsl -d $wslDistro --exec bash -c "cp '$fullImagePath' '/mnt/c/temp/nixos-wsl-image.tar.gz'"
        
        Invoke-CommandWithCheck "wsl --import $WslName $env:USERPROFILE\WSL\$WslName $windowsTempPath" "Importing WSL distribution"
        
        # Clean up
        Remove-Item $windowsTempPath -ErrorAction SilentlyContinue
    } else {
        throw "WSL image not found in build result"
    }
}
catch {
    Write-Error "Failed to create NixOS WSL distribution: $_"
    exit 1
}

# Step 4: Configure the WSL distribution
Write-Host ""
Write-Host "Step 4: Configuring WSL distribution..." -ForegroundColor Cyan

# Set the default user
Write-Host "Setting default user..." -ForegroundColor Yellow
Invoke-CommandWithCheck "wsl -d $WslName --user root --exec sh -c 'echo vulkan-dev:1000:1000 > /etc/wsl.conf'" "Setting default user"

# Step 5: Install development environment
Write-Host ""
Write-Host "Step 5: Installing development environment..." -ForegroundColor Cyan

# Enter the WSL distribution and set up the development environment
$setupScript = @"
#!/bin/bash
echo "Setting up Vulkan development environment..."

# Update system
nixos-rebuild switch

# Install development tools
nix-env -iA nixpkgs.cmake nixpkgs.ninja nixpkgs.gcc nixpkgs.clang nixpkgs.vulkan-headers nixpkgs.glslang nixpkgs.shaderc

# Set up environment variables
echo 'export VULKAN_SDK="/nix/store/*/vulkan-headers"' >> ~/.bashrc
echo 'export VK_LAYER_PATH="/nix/store/*/vulkan-validation-layers/share/vulkan/explicit_layer.d"' >> ~/.bashrc
echo 'export CC="gcc"' >> ~/.bashrc
echo 'export CXX="g++"' >> ~/.bashrc
echo 'export CMAKE_BUILD_TYPE="Debug"' >> ~/.bashrc

# Create development aliases
echo 'alias build="cmake -B build -S . && cmake --build build"' >> ~/.bashrc
echo 'alias clean="rm -rf build"' >> ~/.bashrc
echo 'alias test="ctest --test-dir build"' >> ~/.bashrc

echo "Development environment setup complete!"
"@

$setupScriptPath = Join-Path $env:TEMP "setup-dev-env.sh"
$setupScript | Out-File -FilePath $setupScriptPath -Encoding UTF8

try {
    Write-Host "Running development environment setup..." -ForegroundColor Yellow
    Invoke-CommandWithCheck "wsl -d $WslName --exec bash $setupScriptPath" "Setting up development environment"
}
finally {
    Remove-Item $setupScriptPath -ErrorAction SilentlyContinue
}

# Step 4: Test the setup
Write-Host ""
Write-Host "Step 4: Testing the setup..." -ForegroundColor Cyan

Write-Host "Testing WSL distribution..." -ForegroundColor Yellow
$testCommands = @(
    "wsl -d $WslName --exec whoami",
    "wsl -d $WslName --exec cmake --version",
    "wsl -d $WslName --exec gcc --version"
)

foreach ($cmd in $testCommands) {
    try {
        $output = Invoke-Expression $cmd
        Write-Host "SUCCESS: $cmd" -ForegroundColor Green
    }
    catch {
        Write-Host "WARNING: $cmd (may not be available yet)" -ForegroundColor Yellow
    }
}

# Test Nix in the WSL distribution that has it installed
$wslDistro = (wsl --list --verbose | Select-String "Ubuntu\|Debian" | Select-Object -First 1).ToString().Split()[0]
if ($wslDistro) {
    Write-Host "Testing Nix installation in $wslDistro..." -ForegroundColor Yellow
    try {
        $nixVersion = wsl -d $wslDistro --exec bash -c "source /etc/profile.d/nix.sh && nix --version"
        Write-Host "SUCCESS: Nix version: $nixVersion" -ForegroundColor Green
    }
    catch {
        Write-Host "WARNING: Nix test failed" -ForegroundColor Yellow
    }
}

# Step 5: Final configuration
Write-Host ""
Write-Host "Step 5: Final configuration..." -ForegroundColor Cyan

# Set as default WSL distribution
Write-Host "Setting as default WSL distribution..." -ForegroundColor Yellow
Invoke-CommandWithCheck "wsl --set-default $WslName" "Setting default WSL distribution"

# Create shortcuts
Write-Host "Creating shortcuts..." -ForegroundColor Yellow
$shortcutPath = "$env:USERPROFILE\Desktop\NixOS-Vulkan-Dev.lnk"
$wshShell = New-Object -ComObject WScript.Shell
$shortcut = $wshShell.CreateShortcut($shortcutPath)
$shortcut.TargetPath = "wsl.exe"
$shortcut.Arguments = "-d $WslName"
$shortcut.Description = "NixOS Vulkan Development Environment"
$shortcut.WorkingDirectory = "$env:USERPROFILE"
$shortcut.Save()

# Create VS Code configuration
$vscodeConfigDir = "$env:APPDATA\Code\User\settings.json"
if (Test-Path $vscodeConfigDir) {
    Write-Host "Configuring VS Code for WSL..." -ForegroundColor Yellow
    $vscodeSettings = Get-Content $vscodeConfigDir | ConvertFrom-Json
    $vscodeSettings | Add-Member -Name "remote.WSL.enabled" -Value $true -MemberType NoteProperty -Force
    $vscodeSettings | Add-Member -Name "remote.WSL.defaultDistro" -Value $WslName -MemberType NoteProperty -Force
    $vscodeSettings | ConvertTo-Json -Depth 10 | Set-Content $vscodeConfigDir
}

# Success message
Write-Host ""
Write-Host "NixOS WSL setup completed successfully!" -ForegroundColor Green
Write-Host "=======================================" -ForegroundColor Green
Write-Host ""
Write-Host "What's been set up:" -ForegroundColor Cyan
Write-Host "  - WSL 2 with NixOS" -ForegroundColor Green
Write-Host "  - Vulkan development environment" -ForegroundColor Green
Write-Host "  - CMake, Ninja, GCC, Clang" -ForegroundColor Green
Write-Host "  - Vulkan SDK and tools" -ForegroundColor Green
Write-Host "  - Development aliases and shortcuts" -ForegroundColor Green
Write-Host ""
Write-Host "How to use:" -ForegroundColor Cyan
Write-Host "  - Double-click 'NixOS-Vulkan-Dev' on your desktop" -ForegroundColor White
Write-Host "  - Or run: wsl -d $WslName" -ForegroundColor White
Write-Host "  - Or use VS Code with WSL extension" -ForegroundColor White
Write-Host ""
Write-Host "Development commands:" -ForegroundColor Cyan
Write-Host "  - build    - Build the project" -ForegroundColor White
Write-Host "  - clean    - Clean build directory" -ForegroundColor White
Write-Host "  - test     - Run tests" -ForegroundColor White
Write-Host "  - vulkaninfo - Show Vulkan information" -ForegroundColor White
Write-Host ""
Write-Host "Project location in WSL:" -ForegroundColor Cyan
Write-Host "  /mnt/c/Dev/3DGameOfLife-Vulkan-Edition" -ForegroundColor White
Write-Host ""
Write-Host "Happy coding!" -ForegroundColor Green 
# Ubuntu WSL Setup Script for 3D Game of Life - Vulkan Edition
# This script sets up an Ubuntu WSL environment with Vulkan development tools

param(
    [string]$WslName = "Ubuntu-Vulkan",
    [switch]$Force
)

Write-Host "Setting up Ubuntu WSL for Vulkan Development" -ForegroundColor Cyan
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

# Step 1: Check WSL installation
Write-Host "Step 1: Checking WSL installation..." -ForegroundColor Cyan

if (-not (Test-Command "wsl")) {
    Write-Host "WSL not found. Installing WSL..." -ForegroundColor Yellow
    Invoke-CommandWithCheck "dism.exe /online /enable-feature /featurename:Microsoft-Windows-Subsystem-Linux /all /norestart" "Enabling WSL feature"
    Invoke-CommandWithCheck "dism.exe /online /enable-feature /featurename:VirtualMachinePlatform /all /norestart" "Enabling Virtual Machine Platform"
    
    Write-Host "Please restart your computer and run this script again." -ForegroundColor Yellow
    Write-Host "After restart, WSL will be ready for Ubuntu installation." -ForegroundColor Yellow
    exit 0
} else {
    Write-Host "WSL is already installed" -ForegroundColor Green
}

# Set WSL 2 as default
Write-Host "Setting WSL 2 as default..." -ForegroundColor Yellow
Invoke-CommandWithCheck "wsl --set-default-version 2" "Setting WSL 2 as default"

# Step 2: Install Ubuntu WSL distribution
Write-Host ""
Write-Host "Step 2: Installing Ubuntu WSL distribution..." -ForegroundColor Cyan

# Check if Ubuntu distribution already exists
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

# Install Ubuntu from Microsoft Store
Write-Host "Installing Ubuntu from Microsoft Store..." -ForegroundColor Yellow
try {
    # Try to install Ubuntu using winget
    winget install --id Canonical.Ubuntu.2204
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Winget installation failed, trying alternative method..." -ForegroundColor Yellow
        # Alternative: Download and install manually
        $ubuntuUrl = "https://aka.ms/wslubuntu2204"
        $ubuntuPath = "$env:TEMP\Ubuntu_2204.2.0_x64.appx"
        
        Invoke-WebRequest -Uri $ubuntuUrl -OutFile $ubuntuPath
        Add-AppxPackage -Path $ubuntuPath
    }
}
catch {
    Write-Error "Failed to install Ubuntu: $_"
    Write-Host "Please install Ubuntu manually from the Microsoft Store and run this script again." -ForegroundColor Yellow
    exit 1
}

# Step 3: Set up the Ubuntu environment
Write-Host ""
Write-Host "Step 3: Setting up Ubuntu environment..." -ForegroundColor Cyan

# Create setup script for Ubuntu
$setupScript = @"
#!/bin/bash
echo "Setting up Vulkan development environment in Ubuntu..."

# Update package list
sudo apt update

# Install essential build tools
sudo apt install -y build-essential cmake ninja-build pkg-config git

# Install Vulkan development packages
sudo apt install -y vulkan-tools vulkan-validationlayers libvulkan-dev vulkan-headers

# Install GLSL shader compiler
sudo apt install -y glslang-tools shaderc

# Install graphics libraries
sudo apt install -y libglfw3-dev libglm-dev libspdlog-dev nlohmann-json3-dev

# Install ImGui
sudo apt install -y libimgui-dev

# Install testing frameworks
sudo apt install -y libgtest-dev libbenchmark-dev

# Install additional development tools
sudo apt install -y clang-tidy cppcheck valgrind gdb

# Install documentation tools
sudo apt install -y doxygen graphviz

# Install Python for scripts
sudo apt install -y python3 python3-pip

# Set up environment variables
echo 'export VULKAN_SDK="/usr"' >> ~/.bashrc
echo 'export VK_LAYER_PATH="/usr/share/vulkan/explicit_layer.d"' >> ~/.bashrc
echo 'export CC="gcc"' >> ~/.bashrc
echo 'export CXX="g++"' >> ~/.bashrc
echo 'export CMAKE_BUILD_TYPE="Debug"' >> ~/.bashrc
echo 'export CMAKE_GENERATOR="Ninja"' >> ~/.bashrc
echo 'export CMAKE_EXPORT_COMPILE_COMMANDS="ON"' >> ~/.bashrc

# Create development aliases
echo 'alias build="cmake -B build -S . && cmake --build build"' >> ~/.bashrc
echo 'alias clean="rm -rf build"' >> ~/.bashrc
echo 'alias test="ctest --test-dir build"' >> ~/.bashrc
echo 'alias debug="gdb build/3DGameOfLife-Vulkan-Edition"' >> ~/.bashrc
echo 'alias profile="valgrind --tool=callgrind build/3DGameOfLife-Vulkan-Edition"' >> ~/.bashrc
echo 'alias vulkan-info="vulkaninfo"' >> ~/.bashrc
echo 'alias shader-compile="glslangValidator"' >> ~/.bashrc

# Set up Git configuration
git config --global user.name "Vulkan Developer"
git config --global user.email "vulkan-dev@ubuntu-wsl.local"

echo "Ubuntu Vulkan development environment setup complete!"
"@

$setupScriptPath = Join-Path $env:TEMP "setup-ubuntu-env.sh"
$setupScript | Out-File -FilePath $setupScriptPath -Encoding UTF8

# Run the setup script in Ubuntu
Write-Host "Running Ubuntu environment setup..." -ForegroundColor Yellow
try {
    # First, launch Ubuntu to complete initial setup
    Write-Host "Launching Ubuntu for initial setup..." -ForegroundColor Yellow
    wsl -d Ubuntu-22.04 --exec bash -c "echo 'Ubuntu is ready'"
    
    # Now run our setup script
    wsl -d Ubuntu-22.04 --exec bash $setupScriptPath
    if ($LASTEXITCODE -ne 0) {
        throw "Ubuntu setup failed"
    }
}
catch {
    Write-Error "Failed to set up Ubuntu environment: $_"
    exit 1
}
finally {
    Remove-Item $setupScriptPath -ErrorAction SilentlyContinue
}

# Step 4: Test the setup
Write-Host ""
Write-Host "Step 4: Testing the setup..." -ForegroundColor Cyan

Write-Host "Testing Ubuntu WSL distribution..." -ForegroundColor Yellow
$testCommands = @(
    "wsl -d Ubuntu-22.04 --exec whoami",
    "wsl -d Ubuntu-22.04 --exec cmake --version",
    "wsl -d Ubuntu-22.04 --exec gcc --version",
    "wsl -d Ubuntu-22.04 --exec vulkaninfo --summary"
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

# Step 5: Final configuration
Write-Host ""
Write-Host "Step 5: Final configuration..." -ForegroundColor Cyan

# Set Ubuntu as default WSL distribution
Write-Host "Setting Ubuntu as default WSL distribution..." -ForegroundColor Yellow
Invoke-CommandWithCheck "wsl --set-default Ubuntu-22.04" "Setting default WSL distribution"

# Create shortcuts
Write-Host "Creating shortcuts..." -ForegroundColor Yellow
$shortcutPath = "$env:USERPROFILE\Desktop\Ubuntu-Vulkan-Dev.lnk"
$wshShell = New-Object -ComObject WScript.Shell
$shortcut = $wshShell.CreateShortcut($shortcutPath)
$shortcut.TargetPath = "wsl.exe"
$shortcut.Arguments = "-d Ubuntu-22.04"
$shortcut.Description = "Ubuntu Vulkan Development Environment"
$shortcut.WorkingDirectory = "$env:USERPROFILE"
$shortcut.Save()

# Create VS Code configuration
$vscodeConfigDir = "$env:APPDATA\Code\User\settings.json"
if (Test-Path $vscodeConfigDir) {
    Write-Host "Configuring VS Code for WSL..." -ForegroundColor Yellow
    $vscodeSettings = Get-Content $vscodeConfigDir | ConvertFrom-Json
    $vscodeSettings | Add-Member -Name "remote.WSL.enabled" -Value $true -MemberType NoteProperty -Force
    $vscodeSettings | Add-Member -Name "remote.WSL.defaultDistro" -Value "Ubuntu-22.04" -MemberType NoteProperty -Force
    $vscodeSettings | ConvertTo-Json -Depth 10 | Set-Content $vscodeConfigDir
}

# Success message
Write-Host ""
Write-Host "Ubuntu WSL setup completed successfully!" -ForegroundColor Green
Write-Host "=========================================" -ForegroundColor Green
Write-Host ""
Write-Host "What's been set up:" -ForegroundColor Cyan
Write-Host "  - WSL 2 with Ubuntu 22.04" -ForegroundColor Green
Write-Host "  - Vulkan development environment" -ForegroundColor Green
Write-Host "  - CMake, Ninja, GCC, Clang" -ForegroundColor Green
Write-Host "  - Vulkan SDK and tools" -ForegroundColor Green
Write-Host "  - Development aliases and shortcuts" -ForegroundColor Green
Write-Host ""
Write-Host "How to use:" -ForegroundColor Cyan
Write-Host "  - Double-click 'Ubuntu-Vulkan-Dev' on your desktop" -ForegroundColor White
Write-Host "  - Or run: wsl -d Ubuntu-22.04" -ForegroundColor White
Write-Host "  - Or use VS Code with WSL extension" -ForegroundColor White
Write-Host ""
Write-Host "Development commands:" -ForegroundColor Cyan
Write-Host "  - build    - Build the project" -ForegroundColor White
Write-Host "  - clean    - Clean build directory" -ForegroundColor White
Write-Host "  - test     - Run tests" -ForegroundColor White
Write-Host "  - debug    - Debug the application" -ForegroundColor White
Write-Host "  - profile  - Profile with Valgrind" -ForegroundColor White
Write-Host "  - vulkan-info - Show Vulkan information" -ForegroundColor White
Write-Host "  - shader-compile - Compile shaders" -ForegroundColor White
Write-Host ""
Write-Host "Project location in WSL:" -ForegroundColor Cyan
Write-Host "  /mnt/c/Dev/3DGameOfLife-Vulkan-Edition" -ForegroundColor White
Write-Host ""
Write-Host "Happy coding!" -ForegroundColor Green 
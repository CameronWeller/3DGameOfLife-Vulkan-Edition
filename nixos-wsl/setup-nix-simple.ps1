# Simple Nix Setup for Existing Ubuntu WSL
# This script installs Nix in your existing Ubuntu WSL distribution

Write-Host "Setting up Nix in existing Ubuntu WSL" -ForegroundColor Cyan
Write-Host "=====================================" -ForegroundColor Cyan
Write-Host ""

# Check if Ubuntu WSL is available
$ubuntuWsl = wsl --list --verbose | Select-String "Ubuntu"
if (-not $ubuntuWsl) {
    Write-Error "Ubuntu WSL distribution not found. Please install Ubuntu WSL first."
    exit 1
}

# Extract the Ubuntu distribution name
$ubuntuDistroName = ($ubuntuWsl.ToString() -split '\s+')[0]
Write-Host "Found Ubuntu WSL distribution: $ubuntuDistroName" -ForegroundColor Green

# Create the Nix installation script
$nixInstallScript = @'
#!/bin/bash
echo "Installing Nix in Ubuntu WSL..."

# Update package list
sudo apt update

# Install required dependencies
sudo apt install -y curl xz-utils git

# Install Nix
echo "Installing Nix package manager..."
sh <(curl -L https://nixos.org/nix/install) --daemon

# Source Nix environment
. /etc/profile.d/nix.sh

# Enable flakes
mkdir -p ~/.config/nix
echo "experimental-features = nix-command flakes" > ~/.config/nix/nix.conf

# Install development tools via Nix
echo "Installing development tools via Nix..."
nix-env -iA nixpkgs.cmake nixpkgs.ninja nixpkgs.gcc nixpkgs.clang nixpkgs.pkg-config
nix-env -iA nixpkgs.vulkan-headers nixpkgs.vulkan-tools nixpkgs.vulkan-validation-layers
nix-env -iA nixpkgs.glslang nixpkgs.shaderc
nix-env -iA nixpkgs.glfw nixpkgs.glm nixpkgs.spdlog nixpkgs.nlohmann_json
nix-env -iA nixpkgs.imgui nixpkgs.gtest nixpkgs.benchmark
nix-env -iA nixpkgs.clang-tools nixpkgs.cppcheck nixpkgs.valgrind nixpkgs.gdb
nix-env -iA nixpkgs.doxygen nixpkgs.graphviz
nix-env -iA nixpkgs.python3 nixpkgs.python3Packages.pip

# Set up environment variables
echo 'export VULKAN_SDK="$(nix-build -A vulkan-headers <nixpkgs> --no-out-link)"' >> ~/.bashrc
echo 'export VK_LAYER_PATH="$(nix-build -A vulkan-validation-layers <nixpkgs> --no-out-link)/share/vulkan/explicit_layer.d"' >> ~/.bashrc
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
echo 'alias nix-shell-vulkan="nix-shell -p vulkan-headers vulkan-tools vulkan-validation-layers glslang shaderc glfw glm spdlog nlohmann_json imgui gtest benchmark"' >> ~/.bashrc

# Set up Git configuration
git config --global user.name "Vulkan Developer"
git config --global user.email "vulkan-dev@nix-ubuntu.local"

echo "Nix development environment setup complete!"
echo ""
echo "To use the development environment:"
echo "1. Enter WSL: wsl"
echo "2. Navigate to project: cd /mnt/c/Dev/3DGameOfLife-Vulkan-Edition"
echo "3. Use Nix shell: nix-shell nixos-wsl/shell.nix"
echo "4. Build: cmake -B build -S . && cmake --build build"
'@

# Save the script to a temporary file
$tempScriptPath = Join-Path $env:TEMP "install-nix-ubuntu.sh"
$nixInstallScript | Out-File -FilePath $tempScriptPath -Encoding UTF8

Write-Host "Installing Nix in Ubuntu WSL..." -ForegroundColor Yellow
Write-Host "This may take several minutes..." -ForegroundColor Yellow
Write-Host ""

try {
    # Run the installation script in Ubuntu WSL
    wsl -d $ubuntuDistroName --exec bash $tempScriptPath
    
    if ($LASTEXITCODE -eq 0) {
        Write-Host ""
        Write-Host "Nix installation completed successfully!" -ForegroundColor Green
        Write-Host "=======================================" -ForegroundColor Green
        Write-Host ""
        Write-Host "What's been set up:" -ForegroundColor Cyan
        Write-Host "  - Nix package manager in Ubuntu WSL" -ForegroundColor Green
        Write-Host "  - Vulkan development environment" -ForegroundColor Green
        Write-Host "  - CMake, Ninja, GCC, Clang" -ForegroundColor Green
        Write-Host "  - Vulkan SDK and tools" -ForegroundColor Green
        Write-Host "  - Development aliases and shortcuts" -ForegroundColor Green
        Write-Host ""
        Write-Host "How to use:" -ForegroundColor Cyan
        Write-Host "  - Enter WSL: wsl" -ForegroundColor White
        Write-Host "  - Navigate to project: cd /mnt/c/Dev/3DGameOfLife-Vulkan-Edition" -ForegroundColor White
        Write-Host "  - Use Nix shell: nix-shell nixos-wsl/shell.nix" -ForegroundColor White
        Write-Host "  - Build: cmake -B build -S . && cmake --build build" -ForegroundColor White
        Write-Host ""
        Write-Host "Development commands:" -ForegroundColor Cyan
        Write-Host "  - build    - Build the project" -ForegroundColor White
        Write-Host "  - clean    - Clean build directory" -ForegroundColor White
        Write-Host "  - test     - Run tests" -ForegroundColor White
        Write-Host "  - debug    - Debug the application" -ForegroundColor White
        Write-Host "  - profile  - Profile with Valgrind" -ForegroundColor White
        Write-Host "  - vulkan-info - Show Vulkan information" -ForegroundColor White
        Write-Host "  - shader-compile - Compile shaders" -ForegroundColor White
        Write-Host "  - nix-shell-vulkan - Enter Nix shell with Vulkan tools" -ForegroundColor White
        Write-Host ""
        Write-Host "Happy coding with Nix!" -ForegroundColor Green
    } else {
        throw "Nix installation failed with exit code $LASTEXITCODE"
    }
}
catch {
    Write-Error "Failed to install Nix: $_"
    Write-Host ""
    Write-Host "Troubleshooting:" -ForegroundColor Yellow
    Write-Host "1. Make sure Ubuntu WSL is working: wsl -d Ubuntu" -ForegroundColor White
    Write-Host "2. Try running the installation manually in WSL" -ForegroundColor White
    Write-Host "3. Check WSL logs: wsl --shutdown && wsl" -ForegroundColor White
}
finally {
    # Clean up
    Remove-Item $tempScriptPath -ErrorAction SilentlyContinue
} 
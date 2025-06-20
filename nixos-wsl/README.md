# Nix Package Management for 3D Game of Life - Vulkan Edition

This directory contains Nix-based build system configurations for reproducible Vulkan development environments.

## Overview

Nix provides:
- **Reproducible builds**: Same environment on any machine
- **Isolated dependencies**: No conflicts between projects
- **Declarative configuration**: All dependencies defined in code
- **Rollback capability**: Easy to switch between versions

## Setup Options

### Option 1: Full NixOS WSL (Advanced)
Use `setup-wsl.ps1` to create a complete NixOS WSL distribution with Vulkan development tools.

**Pros:**
- Complete NixOS environment
- Maximum reproducibility
- Full system-level control

**Cons:**
- More complex setup
- Longer build times
- Requires more disk space

### Option 2: Ubuntu WSL + Nix (Recommended)
Use `setup-nix-dev.ps1` to set up Ubuntu WSL with Nix package management.

**Pros:**
- Simpler setup
- Faster installation
- Best of both worlds (Ubuntu + Nix)
- Easier debugging

**Cons:**
- Mix of Ubuntu and Nix packages
- Slightly less reproducible than full NixOS

## Quick Start

### For Ubuntu + Nix (Recommended)

1. **Run the setup script:**
   ```powershell
   # Run as Administrator
   .\nixos-wsl\setup-nix-dev.ps1
   ```

2. **Enter the development environment:**
   ```bash
   # From Windows
   wsl -d Ubuntu-22.04
   
   # Or use the desktop shortcut: "Nix-Vulkan-Dev"
   ```

3. **Use Nix shell for development:**
   ```bash
   # Navigate to your project
   cd /mnt/c/Dev/3DGameOfLife-Vulkan-Edition
   
   # Enter Nix development shell
   nix-shell nixos-wsl/shell.nix
   ```

### For Full NixOS WSL

1. **Run the setup script:**
   ```powershell
   # Run as Administrator
   .\nixos-wsl\setup-wsl.ps1
   ```

2. **Enter the NixOS environment:**
   ```bash
   # From Windows
   wsl -d NixOS-Vulkan
   
   # Or use the desktop shortcut: "NixOS-Vulkan-Dev"
   ```

## Development Workflow

### Using Nix Shell (Recommended)

The `shell.nix` file provides a complete development environment:

```bash
# Enter the development shell
nix-shell nixos-wsl/shell.nix

# Build the project
cmake -B build -S .
cmake --build build

# Run tests
ctest --test-dir build

# Clean build
rm -rf build
```

### Available Tools

When using the Nix shell, you have access to:

**Build Tools:**
- `cmake` - Build system
- `ninja` - Build generator
- `gcc`, `clang` - Compilers
- `pkg-config` - Package configuration

**Vulkan Development:**
- `vulkaninfo` - Vulkan system information
- `glslangValidator` - Shader validation
- `shaderc` - Shader compilation

**Graphics Libraries:**
- `glfw` - Window management
- `glm` - Mathematics library
- `imgui` - Immediate mode GUI

**Development Tools:**
- `gdb` - Debugger
- `valgrind` - Memory profiler
- `cppcheck` - Static analysis
- `clang-tools` - Clang tooling

**Documentation:**
- `doxygen` - Documentation generator
- `graphviz` - Graph visualization

### Environment Variables

The Nix shell automatically sets:

```bash
VULKAN_SDK=/nix/store/.../vulkan-headers
VK_LAYER_PATH=/nix/store/.../vulkan-validation-layers/share/vulkan/explicit_layer.d
CC=gcc
CXX=g++
CMAKE_BUILD_TYPE=Debug
CMAKE_GENERATOR=Ninja
CMAKE_EXPORT_COMPILE_COMMANDS=ON
```

## Nix Commands Reference

### Package Management

```bash
# Install packages globally
nix-env -iA nixpkgs.package-name

# Remove packages
nix-env -e package-name

# List installed packages
nix-env -q

# Update packages
nix-env -u
```

### Development Shells

```bash
# Enter shell with specific packages
nix-shell -p cmake ninja gcc vulkan-headers

# Enter shell with our configuration
nix-shell nixos-wsl/shell.nix

# Enter shell and run command
nix-shell nixos-wsl/shell.nix --run "cmake --version"
```

### Building

```bash
# Build a Nix expression
nix-build nixos-wsl/shell.nix

# Build with specific attributes
nix-build -A vulkan-headers <nixpkgs>
```

## Troubleshooting

### Common Issues

1. **Nix command not found:**
   ```bash
   # Source Nix environment
   . /etc/profile.d/nix.sh
   ```

2. **Permission denied:**
   ```bash
   # Fix Nix store permissions
   sudo chown -R $USER:$USER /nix
   ```

3. **Out of disk space:**
   ```bash
   # Clean Nix store
   nix-collect-garbage -d
   ```

4. **WSL not starting:**
   ```powershell
   # Restart WSL service
   wsl --shutdown
   wsl
   ```

### Getting Help

- **Nix documentation:** https://nixos.org/guides/
- **NixOS manual:** https://nixos.org/manual/nixos/stable/
- **Nix package search:** https://search.nixos.org/packages

## Advanced Configuration

### Custom Nix Packages

You can add custom packages to `shell.nix`:

```nix
buildInputs = with pkgs; [
  # ... existing packages ...
  your-custom-package
];
```

### Flakes (Experimental)

For even better reproducibility, you can use Nix flakes:

```bash
# Enable flakes
mkdir -p ~/.config/nix
echo "experimental-features = nix-command flakes" > ~/.config/nix/nix.conf

# Use flake
nix develop
```

### Overlays

Add custom package versions:

```nix
{ pkgs ? import <nixpkgs> {} }:

let
  customOverlay = self: super: {
    vulkan-headers = super.vulkan-headers.overrideAttrs (oldAttrs: {
      version = "1.3.250";
    });
  };
  
  pkgsWithOverlay = import <nixpkgs> {
    overlays = [ customOverlay ];
  };
in

pkgsWithOverlay.mkShell {
  # ... rest of configuration
}
```

## Performance Tips

1. **Use Nix shell for development:** Avoid installing packages globally
2. **Enable binary cache:** Faster downloads
3. **Clean regularly:** Remove unused packages
4. **Use specific versions:** Pin dependencies for reproducibility

## Integration with IDEs

### VS Code

1. Install the "Remote - WSL" extension
2. Open folder in WSL: `\\wsl$\Ubuntu-22.04\mnt\c\Dev\3DGameOfLife-Vulkan-Edition`
3. Use the integrated terminal with Nix shell

### CLion

1. Configure WSL toolchain
2. Set CMake generator to Ninja
3. Use Nix shell for environment variables

## Contributing

When contributing to the project:

1. Use the Nix shell for development
2. Test with clean Nix environments
3. Update `shell.nix` when adding dependencies
4. Document any new tools or configurations

---

Happy coding with Nix! 🐧 
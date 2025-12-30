# NixOS Development Environment Setup

This guide provides instructions for setting up a complete, reproducible NixOS development environment for the 3DGameOfLife-Vulkan-Edition project.

This setup provides two things:
1.  A **Development Shell** (`nix develop`): A consistent shell environment with all necessary tools and libraries, usable on any Linux distribution (or macOS/WSL) with Nix installed.
2.  A **Full NixOS VM Configuration**: A complete, bootable NixOS system defined in the `flake.nix` and `nixos/` directory.

## Prerequisites

1.  **Nix Package Manager**: You must have Nix installed. If you don't, follow the official instructions at [nixos.org/download](https://nixos.org/download.html).
2.  **Direnv** (Recommended): For automatic shell environment activation. Install it with your system's package manager (e.g., `sudo apt-get install direnv`) and hook it into your shell.

## Part 1: Using the Development Shell

This is the quickest way to get a working development environment on your current OS (Linux/macOS/WSL).

### Setup

1.  **Allow the environment**:
    Navigate to the project root in your terminal and run:
    ```bash
    direnv allow
    ```
    This will trigger `nix develop` and build the environment for the first time. This may take a while as it downloads and builds all dependencies.

2.  **Enter the Shell**:
    The shell will be activated automatically. You can verify this by checking for the environment variables set in `flake.nix`:
    ```bash
    echo $VULKAN_SDK
    ```

### Building and Running the Project

Once inside the Nix shell, you can proceed with the standard build instructions for the project. The environment provides CMake, Ninja, a C++ compiler, and the Vulkan SDK.

## Part 2: Building and Running the Full NixOS VM

This will build a complete, bootable NixOS virtual machine from the configuration in this repository.

### 1. Build the VM

From the project root, run the following Nix command:

```bash
nix build .#nixosConfigurations.nixos.config.system.build.vm
```

This command builds the NixOS system and creates a script to run it in QEMU. The result will be a symlink named `result` in your current directory.

### 2. Run the VM

Execute the script created in the previous step to start the virtual machine:

```bash
./result/bin/run-nixos-vm
```

The VM will boot, and you will be automatically logged into a GNOME desktop environment as the `nix-dev` user.

### 3. Inside the VM

-   **Development Files**: This VM does not automatically share files with the host. You will need to clone the project repository inside the VM to start working.
-   **Terminal**: Open the GNOME Terminal.
-   **Nix Shell**: Navigate to the cloned project directory and use `direnv allow` as described in Part 1 to activate the development shell.

## Migrating from Windows/PowerShell

The Nix environment includes PowerShell 7 to aid in the transition. However, many scripts in `scripts/` may have Windows-specific paths or commands that will fail on Linux.

**Recommendation**:
Gradually migrate the functionality of the `.ps1` scripts to platform-agnostic shell scripts (`.sh`). The Nix environment provides a consistent set of tools (CMake, Ninja, etc.) that will make this migration easier. 
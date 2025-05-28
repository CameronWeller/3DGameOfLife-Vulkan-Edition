# C++ Vulkan/HIP Engine Scripts

This directory contains all build and development scripts for the C++ Vulkan/HIP Engine project.

## Overview

The scripts have been consolidated into a single, unified interface through `main.ps1`. This provides a consistent and easy-to-use command-line interface for all project operations.

## Usage

All operations are performed through the main script:

```powershell
.\scripts\main.ps1 <command> [options]
```

### Available Commands

- **build** - Build the project
- **clean** - Clean build artifacts
- **setup** - Setup development environment
- **test** - Run tests
- **docker** - Build Docker image
- **quality** - Run code quality checks
- **probe** - Probe and cache system environment information
- **help** - Show help message

### Common Options

- `-BuildConfig <Debug|Release>` - Build configuration (default: Debug)
- `-BuildArch <x64|x86>` - Build architecture (default: x64)
- `-Verbose` - Enable verbose output
- `-Force` - Force operation (skip confirmations)
- `-Admin` - Run with administrator privileges

## Examples

### Building the Project

```powershell
# Debug build (default)
.\scripts\main.ps1 build

# Release build
.\scripts\main.ps1 build -BuildConfig Release

# Verbose output
.\scripts\main.ps1 build -Verbose
```

### Setting Up Development Environment

```powershell
# Basic setup
.\scripts\main.ps1 setup

# Setup with admin privileges (recommended)
.\scripts\main.ps1 setup -Admin
```

### Cleaning Build Artifacts

```powershell
# Clean with confirmation
.\scripts\main.ps1 clean

# Force clean (no confirmation)
.\scripts\main.ps1 clean -Force
```

### Running Tests

```powershell
# Run all tests
.\scripts\main.ps1 test

# Run specific tests
.\scripts\main.ps1 test -Filter "*RenderTest*"

# Verbose test output
.\scripts\main.ps1 test -Verbose
```

### Code Quality Checks

```powershell
# Check code quality
.\scripts\main.ps1 quality

# Auto-fix formatting issues
.\scripts\main.ps1 quality -Fix
```

### Docker Operations

```powershell
# Build Docker image
.\scripts\main.ps1 docker

# Build and push to registry
.\scripts\main.ps1 docker -Push

# Custom tag
.\scripts\main.ps1 docker -Tag "v1.0.0"
```

### System Environment Probe

```powershell
# Probe and cache system information
.\scripts\main.ps1 probe

# Force refresh of cached data
.\scripts\main.ps1 probe -Force

# Show detailed probe output
.\scripts\main.ps1 probe -Verbose

# Show cached data only
.\scripts\main.ps1 probe -ShowOnly
```

## Directory Structure

```
scripts/
├── main.ps1              # Main entry point
├── README.md            # This file
├── common/              # Shared utilities
│   └── functions.ps1    # Common functions
├── commands/            # Command implementations
│   ├── build.ps1        # Build command
│   ├── clean.ps1        # Clean command
│   ├── setup.ps1        # Setup command
│   ├── test.ps1         # Test command
│   ├── docker.ps1       # Docker command
│   ├── quality.ps1      # Quality checks command
│   └── probe.ps1        # Environment probe command
└── cache/               # Cached environment data (git-ignored)
    └── environment.json # Cached system information
```

## Requirements

### Required Tools
- PowerShell 7.0+ (recommended)
- Git
- CMake 3.20+
- Visual Studio 2022 with C++ development tools
- Vulkan SDK

### Optional Tools
- HIP SDK (for AMD GPU support)
- Docker Desktop (for Docker operations)
- clang-format (for code formatting)
- clang-tidy (for static analysis)
- cppcheck (for additional static analysis)

## Features

### Unified Interface
All project operations are accessible through a single entry point with consistent command-line options.

### Environment Detection
The scripts automatically detect and validate required tools and SDKs.

### Colored Output
Clear, color-coded output for different message types:
- 🔵 INFO - Informational messages
- 🟢 SUCCESS - Successful operations
- 🟡 WARNING - Warnings and non-critical issues
- 🔴 ERROR - Errors and failures

### Verbose Mode
Detailed output available for debugging and troubleshooting.

### Cross-Platform Support
While optimized for Windows, the scripts are designed to be easily portable to other platforms.

### Environment Caching
The probe command automatically discovers and caches system information including:
- Visual Studio installations and versions
- SDK locations (Vulkan, HIP, Windows SDK)
- Development tool paths and versions
- System information and paths
- Project directory structure

This cached data is used by other commands for faster execution and consistent environment detection.

## Troubleshooting

### PowerShell Execution Policy
If you encounter execution policy errors:
```powershell
Set-ExecutionPolicy -ExecutionPolicy RemoteSigned -Scope CurrentUser
```

### Missing Dependencies
Run setup to check for missing dependencies:
```powershell
.\scripts\main.ps1 setup
```

### Build Failures
1. Clean the build directory: `.\scripts\main.ps1 clean -Force`
2. Re-run setup: `.\scripts\main.ps1 setup`
3. Try building again: `.\scripts\main.ps1 build -Verbose`

## Contributing

When adding new functionality:
1. Add new commands to the `commands/` directory
2. Update `main.ps1` to include the new command
3. Add common functions to `common/functions.ps1`
4. Update this README with usage examples 
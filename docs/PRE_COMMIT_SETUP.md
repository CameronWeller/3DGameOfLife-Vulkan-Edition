# Pre-commit Hooks Setup Guide

This guide explains how to set up and use the comprehensive pre-commit hooks for the 3D Game of Life - Vulkan Edition project.

## 🎯 Overview

Our pre-commit hooks ensure code quality, consistency, and catch common issues before they reach the repository. The hooks include:

- **Code Formatting**: Automatic C++ formatting with clang-format
- **Static Analysis**: CMake linting, markdown formatting
- **Custom Checks**: Vulkan API patterns, include guards, shader validation
- **Documentation**: Sync checks, copyright validation
- **Git Quality**: Conventional commit messages

## 📋 Prerequisites

### Required Tools

```bash
# Python and pip (for pre-commit)
python3 --version  # Should be 3.7+
pip3 --version

# Node.js (for markdownlint)
node --version     # Should be 14+
npm --version

# Development tools
clang-format --version  # For C++ formatting
cmake --version         # For CMake linting
```

### Platform-Specific Prerequisites

#### Windows
```powershell
# Install Python
winget install Python.Python.3

# Install Node.js
winget install OpenJS.NodeJS

# Install Git Bash (recommended for script execution)
winget install Git.Git

# Install LLVM (for clang-format)
winget install LLVM.LLVM
```

#### Linux (Ubuntu/Debian)
```bash
# Install dependencies
sudo apt update
sudo apt install python3 python3-pip nodejs npm clang-format cmake

# Optional: Install shellcheck for shell script linting
sudo apt install shellcheck
```

#### macOS
```bash
# Using Homebrew
brew install python3 node clang-format cmake shellcheck
```

## 🚀 Installation

### 1. Install pre-commit

```bash
# Install pre-commit globally
pip3 install pre-commit

# Or install in virtual environment
python3 -m venv venv
source venv/bin/activate  # On Windows: venv\Scripts\activate
pip install pre-commit
```

### 2. Install the hooks

```bash
# From the project root directory
cd 3DGameOfLife-Vulkan-Edition

# Install pre-commit hooks
pre-commit install

# Install commit message hooks
pre-commit install --hook-type commit-msg
```

### 3. Install additional dependencies

```bash
# Install markdownlint for markdown formatting
npm install -g markdownlint-cli

# Install cmake-format (optional, for better CMake formatting)
pip3 install cmake-format[yaml]
```

## 🔧 Configuration

### Environment Variables

Set these environment variables for optimal functionality:

```bash
# Windows (PowerShell)
$env:VULKAN_SDK = "C:\VulkanSDK\1.3.X.X"  # Your Vulkan SDK path

# Linux/macOS (Bash)
export VULKAN_SDK="/path/to/vulkan/sdk"
```

### Custom Settings

You can customize hook behavior by modifying `.pre-commit-config.yaml`:

```yaml
# Example: Skip specific hooks temporarily
repos:
  - repo: https://github.com/pre-commit/mirrors-clang-format
    rev: v17.0.6
    hooks:
      - id: clang-format
        # Skip clang-format for specific files
        exclude: ^(third_party/|legacy_code/)
```

## 🎮 Usage

### Automatic Execution

Hooks run automatically on:
- `git commit` - Runs all applicable hooks
- `git commit -m "message"` - Includes commit message validation

### Manual Execution

```bash
# Run all hooks on all files
pre-commit run --all-files

# Run specific hook
pre-commit run clang-format --all-files
pre-commit run validate-shaders --all-files

# Run hooks on specific files
pre-commit run --files src/main.cpp include/VulkanEngine.hpp

# Skip hooks for emergency commits (not recommended)
git commit --no-verify -m "emergency fix"
```

### Custom Hook Testing

```bash
# Test individual custom scripts
scripts/pre-commit/validate-shaders.sh shaders/compute/game_of_life.comp
scripts/pre-commit/check-vulkan-patterns.sh src/VulkanEngine.cpp
scripts/pre-commit/check-include-guards.sh include/VulkanEngine.hpp
```

## 🛠️ Hook Details

### Code Quality Hooks

#### clang-format
- **Purpose**: Automatic C++ code formatting
- **Files**: `*.cpp`, `*.hpp`, `*.c`, `*.h`
- **Configuration**: Uses `.clang-format` in project root
- **Auto-fix**: Yes

#### cmake-format
- **Purpose**: CMake file formatting and linting
- **Files**: `CMakeLists.txt`, `*.cmake`
- **Auto-fix**: Yes (formatting), warnings for linting

#### markdownlint
- **Purpose**: Markdown formatting and style consistency
- **Files**: `*.md`
- **Configuration**: `.markdownlint.json`
- **Auto-fix**: Yes

### Custom Project Hooks

#### validate-shaders
- **Purpose**: GLSL shader syntax validation
- **Files**: `*.vert`, `*.frag`, `*.comp`, `*.geom`, `*.glsl`
- **Requirements**: Vulkan SDK with glslangValidator or glslc
- **Auto-fix**: No

#### check-vulkan-patterns
- **Purpose**: Vulkan API best practices validation
- **Files**: C++ files containing Vulkan code
- **Checks**: Error handling, memory management, synchronization
- **Auto-fix**: No

#### check-include-guards
- **Purpose**: Ensure proper include guards in headers
- **Files**: `*.hpp`
- **Checks**: `#ifndef`/`#define`/`#endif` consistency
- **Auto-fix**: No

#### check-cmake-practices
- **Purpose**: CMake best practices validation
- **Files**: `CMakeLists.txt`, `*.cmake`
- **Checks**: Deprecated commands, modern patterns
- **Auto-fix**: No

#### validate-vcpkg
- **Purpose**: vcpkg.json manifest validation
- **Files**: `vcpkg.json`
- **Checks**: JSON syntax, required fields, dependencies
- **Auto-fix**: No

## 📝 Commit Message Format

We use [Conventional Commits](https://www.conventionalcommits.org/) format:

```
<type>[optional scope]: <description>

[optional body]

[optional footer(s)]
```

### Allowed Types
- `feat`: New features
- `fix`: Bug fixes
- `docs`: Documentation changes
- `style`: Code style changes (formatting, etc.)
- `refactor`: Code refactoring
- `perf`: Performance improvements
- `test`: Adding/updating tests
- `build`: Build system changes
- `ci`: CI configuration changes
- `chore`: Maintenance tasks
- `revert`: Reverting previous commits

### Examples
```bash
# Good commit messages
git commit -m "feat(vulkan): add compute shader validation"
git commit -m "fix(camera): resolve mouse sensitivity calculation"
git commit -m "docs: update shader compilation instructions"
git commit -m "refactor(memory): improve buffer allocation strategy"

# Bad commit messages (will be rejected)
git commit -m "fixed stuff"
git commit -m "WIP"
git commit -m "Update README"
```

## 🚨 Troubleshooting

### Common Issues

#### Hook installation fails
```bash
# Clear pre-commit cache
pre-commit clean

# Reinstall hooks
pre-commit install --overwrite
```

#### clang-format not found
```bash
# Windows: Add LLVM to PATH
# Linux: sudo apt install clang-format
# macOS: brew install clang-format
```

#### Shader validation fails
```bash
# Ensure Vulkan SDK is installed and VULKAN_SDK is set
echo $VULKAN_SDK  # Should show SDK path
glslangValidator --version  # Should work
```

#### Permission errors on Windows
```powershell
# Run as administrator or adjust execution policy
Set-ExecutionPolicy -ExecutionPolicy RemoteSigned -Scope CurrentUser
```

### Skipping Problematic Hooks

If a hook is causing issues, you can temporarily disable it:

```bash
# Skip specific hooks
SKIP=clang-format git commit -m "feat: emergency fix"

# Skip all hooks (not recommended)
git commit --no-verify -m "emergency commit"
```

### Updating Hooks

```bash
# Update all hooks to latest versions
pre-commit autoupdate

# Update specific hook
pre-commit autoupdate --repo https://github.com/pre-commit/mirrors-clang-format
```

## 📊 Integration with IDEs

### Visual Studio Code

1. Install extensions:
   - "Pre-commit Hook Manager"
   - "clang-format"
   - "CMake Tools"

2. Add to `.vscode/settings.json`:
```json
{
    "editor.formatOnSave": true,
    "C_Cpp.clang_format_style": "file",
    "markdownlint.config": {
        "extends": ".markdownlint.json"
    }
}
```

### CLion/IntelliJ

1. Install plugins:
   - "Pre-commit Hook Plugin"
   - "Markdown"

2. Configure code style to match `.clang-format`

## 🎯 Best Practices

### For Developers

1. **Run hooks locally**: `pre-commit run --all-files` before pushing
2. **Keep hooks updated**: `pre-commit autoupdate` regularly
3. **Fix issues early**: Don't accumulate formatting/style debt
4. **Test custom changes**: Validate hook modifications thoroughly

### For Project Maintenance

1. **Review hook failures**: Investigate why hooks fail in CI
2. **Update configurations**: Keep hook versions and configs current
3. **Document exceptions**: When skipping hooks, document why
4. **Monitor performance**: Ensure hooks don't slow development too much

## 📚 Additional Resources

- [Pre-commit Documentation](https://pre-commit.com/)
- [Conventional Commits](https://www.conventionalcommits.org/)
- [clang-format Documentation](https://clang.llvm.org/docs/ClangFormat.html)
- [Vulkan Best Practices](https://github.com/KhronosGroup/Vulkan-Guide)
- [CMake Best Practices](https://cliutils.gitlab.io/modern-cmake/)

---

💡 **Tip**: Start with a subset of hooks if the full setup is overwhelming, then gradually enable more as your team gets comfortable with the workflow. 
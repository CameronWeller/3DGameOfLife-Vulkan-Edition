#!/bin/bash
# Setup script for pre-commit hooks

set -e

echo "🚀 Setting up pre-commit hooks for 3D Game of Life - Vulkan Edition"
echo "=================================================================="

# Check if we're in the right directory
if [[ ! -f ".pre-commit-config.yaml" ]]; then
    echo "❌ Error: .pre-commit-config.yaml not found"
    echo "   Please run this script from the project root directory"
    exit 1
fi

# Check Python installation
if ! command -v python3 >/dev/null 2>&1; then
    echo "❌ Error: Python 3 is required but not installed"
    echo "   Please install Python 3.7+ and try again"
    exit 1
fi

echo "✅ Python 3 found: $(python3 --version)"

# Check pip installation
if ! command -v pip3 >/dev/null 2>&1; then
    echo "❌ Error: pip3 is required but not installed"
    echo "   Please install pip3 and try again"
    exit 1
fi

echo "✅ pip3 found: $(pip3 --version)"

# Install pre-commit
echo "📦 Installing pre-commit..."
if ! pip3 install pre-commit; then
    echo "❌ Failed to install pre-commit"
    echo "   Try: pip3 install --user pre-commit"
    exit 1
fi

echo "✅ pre-commit installed successfully"

# Install pre-commit hooks
echo "🔧 Installing pre-commit hooks..."
if ! pre-commit install; then
    echo "❌ Failed to install pre-commit hooks"
    exit 1
fi

echo "✅ Pre-commit hooks installed"

# Install commit message hooks
echo "📝 Installing commit message hooks..."
if ! pre-commit install --hook-type commit-msg; then
    echo "❌ Failed to install commit message hooks"
    exit 1
fi

echo "✅ Commit message hooks installed"

# Check for optional dependencies
echo "🔍 Checking optional dependencies..."

# Check Node.js for markdownlint
if command -v npm >/dev/null 2>&1; then
    echo "✅ Node.js/npm found - installing markdownlint..."
    if npm install -g markdownlint-cli 2>/dev/null; then
        echo "✅ markdownlint installed"
    else
        echo "⚠️  Failed to install markdownlint globally"
        echo "   You may need to run: sudo npm install -g markdownlint-cli"
    fi
else
    echo "⚠️  Node.js not found - markdownlint will be skipped"
    echo "   Install Node.js for markdown formatting support"
fi

# Check clang-format
if command -v clang-format >/dev/null 2>&1; then
    echo "✅ clang-format found: $(clang-format --version | head -n1)"
else
    echo "⚠️  clang-format not found"
    echo "   Install LLVM/clang for C++ formatting support"
fi

# Check cmake
if command -v cmake >/dev/null 2>&1; then
    echo "✅ CMake found: $(cmake --version | head -n1)"
else
    echo "⚠️  CMake not found"
    echo "   CMake is required for this project"
fi

# Check Vulkan SDK for shader validation
if [[ -n "$VULKAN_SDK" ]]; then
    echo "✅ VULKAN_SDK environment variable set: $VULKAN_SDK"
    
    if [[ -f "$VULKAN_SDK/bin/glslangValidator" ]]; then
        echo "✅ glslangValidator found in Vulkan SDK"
    elif command -v glslangValidator >/dev/null 2>&1; then
        echo "✅ glslangValidator found in PATH"
    else
        echo "⚠️  glslangValidator not found"
        echo "   Shader validation will be limited"
    fi
else
    echo "⚠️  VULKAN_SDK environment variable not set"
    echo "   Set VULKAN_SDK for optimal shader validation"
fi

# Make scripts executable
echo "🔧 Making scripts executable..."
chmod +x scripts/pre-commit/*.sh

# Test pre-commit installation
echo "🧪 Testing pre-commit installation..."
if pre-commit --version >/dev/null 2>&1; then
    echo "✅ Pre-commit is working: $(pre-commit --version)"
else
    echo "❌ Pre-commit test failed"
    exit 1
fi

# Optional: Run pre-commit on all files to check setup
echo ""
echo "🎯 Setup complete! Next steps:"
echo ""
echo "1. Optional: Run pre-commit on all files to test setup:"
echo "   pre-commit run --all-files"
echo ""
echo "2. Make your first commit to test the hooks:"
echo "   git add ."
echo "   git commit -m \"feat: setup pre-commit hooks\""
echo ""
echo "3. If you encounter issues, see docs/PRE_COMMIT_SETUP.md"
echo ""
echo "📚 Documentation: docs/PRE_COMMIT_SETUP.md"
echo "🔧 Configuration: .pre-commit-config.yaml"
echo ""
echo "Happy coding! 🚀" 
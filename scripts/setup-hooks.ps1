# Setup script for pre-commit hooks (Windows PowerShell)

$ErrorActionPreference = "Stop"

Write-Host "🚀 Setting up pre-commit hooks for 3D Game of Life - Vulkan Edition" -ForegroundColor Green
Write-Host "==================================================================" -ForegroundColor Green

# Check if we're in the right directory
if (!(Test-Path ".pre-commit-config.yaml")) {
    Write-Host "❌ Error: .pre-commit-config.yaml not found" -ForegroundColor Red
    Write-Host "   Please run this script from the project root directory" -ForegroundColor Yellow
    exit 1
}

# Check Python installation
try {
    $pythonVersion = python --version 2>&1
    Write-Host "✅ Python found: $pythonVersion" -ForegroundColor Green
} catch {
    Write-Host "❌ Error: Python is required but not installed" -ForegroundColor Red
    Write-Host "   Please install Python 3.7+ and try again" -ForegroundColor Yellow
    Write-Host "   Download from: https://python.org" -ForegroundColor Yellow
    exit 1
}

# Check pip installation
try {
    $pipVersion = pip --version 2>&1
    Write-Host "✅ pip found: $pipVersion" -ForegroundColor Green
} catch {
    Write-Host "❌ Error: pip is required but not installed" -ForegroundColor Red
    Write-Host "   Please install pip and try again" -ForegroundColor Yellow
    exit 1
}

# Install pre-commit
Write-Host "📦 Installing pre-commit..." -ForegroundColor Cyan
try {
    pip install pre-commit
    Write-Host "✅ pre-commit installed successfully" -ForegroundColor Green
} catch {
    Write-Host "❌ Failed to install pre-commit" -ForegroundColor Red
    Write-Host "   Try: pip install --user pre-commit" -ForegroundColor Yellow
    exit 1
}

# Install pre-commit hooks
Write-Host "🔧 Installing pre-commit hooks..." -ForegroundColor Cyan
try {
    pre-commit install
    Write-Host "✅ Pre-commit hooks installed" -ForegroundColor Green
} catch {
    Write-Host "❌ Failed to install pre-commit hooks" -ForegroundColor Red
    exit 1
}

# Install commit message hooks
Write-Host "📝 Installing commit message hooks..." -ForegroundColor Cyan
try {
    pre-commit install --hook-type commit-msg
    Write-Host "✅ Commit message hooks installed" -ForegroundColor Green
} catch {
    Write-Host "❌ Failed to install commit message hooks" -ForegroundColor Red
    exit 1
}

# Check for optional dependencies
Write-Host "🔍 Checking optional dependencies..." -ForegroundColor Cyan

# Check Node.js for markdownlint
try {
    $nodeVersion = node --version 2>&1
    Write-Host "✅ Node.js found: $nodeVersion" -ForegroundColor Green
    
    try {
        npm install -g markdownlint-cli
        Write-Host "✅ markdownlint installed" -ForegroundColor Green
    } catch {
        Write-Host "⚠️  Failed to install markdownlint globally" -ForegroundColor Yellow
        Write-Host "   You may need to run as administrator" -ForegroundColor Yellow
    }
} catch {
    Write-Host "⚠️  Node.js not found - markdownlint will be skipped" -ForegroundColor Yellow
    Write-Host "   Install Node.js for markdown formatting support" -ForegroundColor Yellow
    Write-Host "   Download from: https://nodejs.org" -ForegroundColor Yellow
}

# Check clang-format
try {
    $clangVersion = clang-format --version 2>&1
    Write-Host "✅ clang-format found: $clangVersion" -ForegroundColor Green
} catch {
    Write-Host "⚠️  clang-format not found" -ForegroundColor Yellow
    Write-Host "   Install LLVM for C++ formatting support" -ForegroundColor Yellow
    Write-Host "   Run: winget install LLVM.LLVM" -ForegroundColor Yellow
}

# Check cmake
try {
    $cmakeVersion = cmake --version 2>&1 | Select-Object -First 1
    Write-Host "✅ CMake found: $cmakeVersion" -ForegroundColor Green
} catch {
    Write-Host "⚠️  CMake not found" -ForegroundColor Yellow
    Write-Host "   CMake is required for this project" -ForegroundColor Yellow
    Write-Host "   Run: winget install Kitware.CMake" -ForegroundColor Yellow
}

# Check Vulkan SDK for shader validation
if ($env:VULKAN_SDK) {
    Write-Host "✅ VULKAN_SDK environment variable set: $env:VULKAN_SDK" -ForegroundColor Green
    
    $glslValidator = Join-Path $env:VULKAN_SDK "bin\glslangValidator.exe"
    if (Test-Path $glslValidator) {
        Write-Host "✅ glslangValidator found in Vulkan SDK" -ForegroundColor Green
    } else {
        try {
            glslangValidator --version | Out-Null
            Write-Host "✅ glslangValidator found in PATH" -ForegroundColor Green
        } catch {
            Write-Host "⚠️  glslangValidator not found" -ForegroundColor Yellow
            Write-Host "   Shader validation will be limited" -ForegroundColor Yellow
        }
    }
} else {
    Write-Host "⚠️  VULKAN_SDK environment variable not set" -ForegroundColor Yellow
    Write-Host "   Set VULKAN_SDK for optimal shader validation" -ForegroundColor Yellow
    Write-Host "   Download Vulkan SDK from: https://vulkan.lunarg.com" -ForegroundColor Yellow
}

# Test pre-commit installation
Write-Host "🧪 Testing pre-commit installation..." -ForegroundColor Cyan
try {
    $precommitVersion = pre-commit --version 2>&1
    Write-Host "✅ Pre-commit is working: $precommitVersion" -ForegroundColor Green
} catch {
    Write-Host "❌ Pre-commit test failed" -ForegroundColor Red
    exit 1
}

# Setup complete
Write-Host ""
Write-Host "🎯 Setup complete! Next steps:" -ForegroundColor Green
Write-Host ""
Write-Host "1. Optional: Run pre-commit on all files to test setup:" -ForegroundColor White
Write-Host "   pre-commit run --all-files" -ForegroundColor Cyan
Write-Host ""
Write-Host "2. Make your first commit to test the hooks:" -ForegroundColor White
Write-Host "   git add ." -ForegroundColor Cyan
Write-Host "   git commit -m `"feat: setup pre-commit hooks`"" -ForegroundColor Cyan
Write-Host ""
Write-Host "3. If you encounter issues, see docs/PRE_COMMIT_SETUP.md" -ForegroundColor White
Write-Host ""
Write-Host "📚 Documentation: docs/PRE_COMMIT_SETUP.md" -ForegroundColor Yellow
Write-Host "🔧 Configuration: .pre-commit-config.yaml" -ForegroundColor Yellow
Write-Host ""
Write-Host "Happy coding! 🚀" -ForegroundColor Green 
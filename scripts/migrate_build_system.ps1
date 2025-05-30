# Migrate Build System Script for Windows
# This script helps migrate from the original monolithic CMake build system to the new modular one

# Stop on first error
$ErrorActionPreference = "Stop"

# Define colors for output
function Write-ColorOutput($ForegroundColor) {
    $fc = $host.UI.RawUI.ForegroundColor
    $host.UI.RawUI.ForegroundColor = $ForegroundColor
    if ($args) {
        Write-Output $args
    }
    $host.UI.RawUI.ForegroundColor = $fc
}

function Write-Success($message) {
    Write-ColorOutput Green "[SUCCESS] $message"
}

function Write-Info($message) {
    Write-ColorOutput Cyan "[INFO] $message"
}

function Write-Warning($message) {
    Write-ColorOutput Yellow "[WARNING] $message"
}

function Write-Error($message) {
    Write-ColorOutput Red "[ERROR] $message"
    exit 1
}

# Check if running from the correct directory
if (-not (Test-Path "CMakeLists.txt")) {
    Write-Error "This script must be run from the root of the project directory."
}

# Backup current CMakeLists.txt
Write-Info "Backing up current CMakeLists.txt..."
if (Test-Path "CMakeLists.txt.backup") {
    Write-Warning "CMakeLists.txt.backup already exists. Overwriting..."
}
Copy-Item -Path "CMakeLists.txt" -Destination "CMakeLists.txt.backup" -Force
Write-Success "Backup created: CMakeLists.txt.backup"

# Check if modular CMakeLists.txt exists
if (-not (Test-Path "CMakeLists_modular.txt")) {
    Write-Error "CMakeLists_modular.txt not found. Make sure you have created it first."
}

# Replace CMakeLists.txt with modular version
Write-Info "Replacing CMakeLists.txt with modular version..."
Copy-Item -Path "CMakeLists_modular.txt" -Destination "CMakeLists.txt" -Force
Write-Success "CMakeLists.txt replaced with modular version"

# Create required directory structure
Write-Info "Creating required directory structure..."
if (-not (Test-Path "cmake\modules")) {
    New-Item -Path "cmake\modules" -ItemType Directory -Force | Out-Null
    Write-Success "Created cmake\modules directory"
} else {
    Write-Info "cmake\modules directory already exists"
}

# Check if all required module files exist
$requiredModules = @(
    "Options.cmake",
    "Dependencies.cmake",
    "Shaders.cmake",
    "Components.cmake",
    "Testing.cmake",
    "StaticAnalysis.cmake"
)

$missingModules = @()
foreach ($module in $requiredModules) {
    if (-not (Test-Path "cmake\modules\$module")) {
        $missingModules += $module
    }
}

if ($missingModules.Count -gt 0) {
    Write-Warning "The following module files are missing:"
    foreach ($module in $missingModules) {
        Write-Warning "  - $module"
    }
    Write-Warning "Please create these files before proceeding."
} else {
    Write-Success "All required module files are present"
}

# Clean build directory if it exists
if (Test-Path "build") {
    Write-Info "Cleaning build directory..."
    Remove-Item -Path "build" -Recurse -Force
    Write-Success "Build directory cleaned"
} else {
    Write-Info "No build directory found, skipping clean step"
}

# Configure with new build system
Write-Info "Configuring with new build system..."
try {
    cmake -B build -S .
    Write-Success "Configuration successful"
} catch {
    Write-Error "Configuration failed: $_"
}

# Build
Write-Info "Building project..."
try {
    cmake --build build
    Write-Success "Build successful"
} catch {
    Write-Error "Build failed: $_"
}

Write-Success "Migration complete! The project is now using the modular build system."
Write-Info "If you encounter any issues, you can revert to the original build system by running:"
Write-Info "  Copy-Item -Path 'CMakeLists.txt.backup' -Destination 'CMakeLists.txt' -Force"

Write-Info "For more information, see docs/build_system_migration_guide.md"
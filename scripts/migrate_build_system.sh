#!/bin/bash
# Migrate Build System Script for Linux/macOS
# This script helps migrate from the original monolithic CMake build system to the new modular one

# Stop on first error
set -e

# Define colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

function write_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

function write_info() {
    echo -e "${CYAN}[INFO]${NC} $1"
}

function write_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

function write_error() {
    echo -e "${RED}[ERROR]${NC} $1"
    exit 1
}

# Check if running from the correct directory
if [ ! -f "CMakeLists.txt" ]; then
    write_error "This script must be run from the root of the project directory."
fi

# Backup current CMakeLists.txt
write_info "Backing up current CMakeLists.txt..."
if [ -f "CMakeLists.txt.backup" ]; then
    write_warning "CMakeLists.txt.backup already exists. Overwriting..."
fi
cp CMakeLists.txt CMakeLists.txt.backup
write_success "Backup created: CMakeLists.txt.backup"

# Check if modular CMakeLists.txt exists
if [ ! -f "CMakeLists_modular.txt" ]; then
    write_error "CMakeLists_modular.txt not found. Make sure you have created it first."
fi

# Replace CMakeLists.txt with modular version
write_info "Replacing CMakeLists.txt with modular version..."
cp CMakeLists_modular.txt CMakeLists.txt
write_success "CMakeLists.txt replaced with modular version"

# Create required directory structure
write_info "Creating required directory structure..."
if [ ! -d "cmake/modules" ]; then
    mkdir -p cmake/modules
    write_success "Created cmake/modules directory"
else
    write_info "cmake/modules directory already exists"
fi

# Check if all required module files exist
required_modules=(
    "Options.cmake"
    "Dependencies.cmake"
    "Shaders.cmake"
    "Components.cmake"
    "Testing.cmake"
    "StaticAnalysis.cmake"
)

missing_modules=()
for module in "${required_modules[@]}"; do
    if [ ! -f "cmake/modules/$module" ]; then
        missing_modules+=("$module")
    fi
done

if [ ${#missing_modules[@]} -gt 0 ]; then
    write_warning "The following module files are missing:"
    for module in "${missing_modules[@]}"; do
        write_warning "  - $module"
    done
    write_warning "Please create these files before proceeding."
else
    write_success "All required module files are present"
fi

# Clean build directory if it exists
if [ -d "build" ]; then
    write_info "Cleaning build directory..."
    rm -rf build
    write_success "Build directory cleaned"
else
    write_info "No build directory found, skipping clean step"
fi

# Configure with new build system
write_info "Configuring with new build system..."
if cmake -B build -S .; then
    write_success "Configuration successful"
else
    write_error "Configuration failed"
fi

# Build
write_info "Building project..."
if cmake --build build; then
    write_success "Build successful"
else
    write_error "Build failed"
fi

write_success "Migration complete! The project is now using the modular build system."
write_info "If you encounter any issues, you can revert to the original build system by running:"
write_info "  cp CMakeLists.txt.backup CMakeLists.txt"

write_info "For more information, see docs/build_system_migration_guide.md"
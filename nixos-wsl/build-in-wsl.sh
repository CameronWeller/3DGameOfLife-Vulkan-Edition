#!/bin/bash
# Build script for 3D Game of Life - Vulkan Edition in NixOS WSL
# This script builds the project with all necessary dependencies

set -e  # Exit on any error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Function to print colored output
print_status() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Function to check if command exists
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# Function to check dependencies
check_dependencies() {
    print_status "Checking dependencies..."
    
    local missing_deps=()
    
    # Check required commands
    local commands=("cmake" "ninja" "gcc" "g++" "pkg-config" "git")
    for cmd in "${commands[@]}"; do
        if ! command_exists "$cmd"; then
            missing_deps+=("$cmd")
        fi
    done
    
    # Check Vulkan
    if ! command_exists "vulkaninfo"; then
        missing_deps+=("vulkan-tools")
    fi
    
    # Check shader compiler
    if ! command_exists "glslangValidator"; then
        missing_deps+=("glslang")
    fi
    
    if [ ${#missing_deps[@]} -ne 0 ]; then
        print_error "Missing dependencies: ${missing_deps[*]}"
        print_status "Installing missing dependencies..."
        
        # Install missing packages
        for dep in "${missing_deps[@]}"; do
            case $dep in
                "cmake"|"ninja"|"gcc"|"g++"|"pkg-config"|"git"|"vulkan-tools"|"glslang")
                    print_status "Installing $dep..."
                    nix-env -iA "nixpkgs.$dep" || {
                        print_error "Failed to install $dep"
                        exit 1
                    }
                    ;;
                *)
                    print_warning "Unknown dependency: $dep"
                    ;;
            esac
        done
    else
        print_success "All dependencies are available"
    fi
}

# Function to set up environment
setup_environment() {
    print_status "Setting up environment..."
    
    # Set environment variables
    export VULKAN_SDK=$(find /nix/store -name "vulkan-headers" -type d | head -n1)
    export VK_LAYER_PATH=$(find /nix/store -name "vulkan-validation-layers" -type d | head -n1)/share/vulkan/explicit_layer.d
    export VK_ICD_FILENAMES=$(find /nix/store -name "mesa" -type d | head -n1)/drivers/share/vulkan/icd.d/intel_icd.x86_64.json:$(find /nix/store -name "mesa" -type d | head -n1)/drivers/share/vulkan/icd.d/radeon_icd.x86_64.json
    
    # Development environment
    export CC="gcc"
    export CXX="g++"
    export CMAKE_BUILD_TYPE="Debug"
    export CMAKE_GENERATOR="Ninja"
    export CMAKE_EXPORT_COMPILE_COMMANDS="ON"
    
    # Python environment
    export PYTHONPATH=$(find /nix/store -name "python3" -type d | head -n1)/lib/python3.11/site-packages
    export PYTHONUNBUFFERED="1"
    
    print_success "Environment variables set"
}

# Function to clean build directory
clean_build() {
    if [ "$1" = "--clean" ] || [ "$1" = "-c" ]; then
        print_status "Cleaning build directory..."
        rm -rf build
        print_success "Build directory cleaned"
    fi
}

# Function to configure build
configure_build() {
    print_status "Configuring build..."
    
    # Create build directory
    mkdir -p build
    
    # Configure with CMake
    cmake -B build -S . \
        -DCMAKE_BUILD_TYPE=Debug \
        -DBUILD_TESTING=ON \
        -DENABLE_STATIC_ANALYSIS=ON \
        -DENABLE_SANITIZERS=ON \
        -DENABLE_COVERAGE=ON \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
    
    print_success "Build configured"
}

# Function to build project
build_project() {
    print_status "Building project..."
    
    # Build with Ninja
    cmake --build build --parallel $(nproc)
    
    print_success "Build completed"
}

# Function to run tests
run_tests() {
    if [ "$1" = "--test" ] || [ "$1" = "-t" ]; then
        print_status "Running tests..."
        
        # Run all tests
        ctest --test-dir build --output-on-failure
        
        print_success "Tests completed"
    fi
}

# Function to run static analysis
run_static_analysis() {
    if [ "$1" = "--analyze" ] || [ "$1" = "-a" ]; then
        print_status "Running static analysis..."
        
        # Run clang-tidy
        if command_exists "clang-tidy"; then
            print_status "Running clang-tidy..."
            find src include -name "*.cpp" -o -name "*.hpp" | xargs clang-tidy -p build
        fi
        
        # Run cppcheck
        if command_exists "cppcheck"; then
            print_status "Running cppcheck..."
            cppcheck --enable=all --std=c++17 src/ include/
        fi
        
        print_success "Static analysis completed"
    fi
}

# Function to show help
show_help() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  -c, --clean      Clean build directory before building"
    echo "  -t, --test       Run tests after building"
    echo "  -a, --analyze    Run static analysis"
    echo "  -h, --help       Show this help message"
    echo ""
    echo "Examples:"
    echo "  $0                    # Build project"
    echo "  $0 --clean           # Clean and build"
    echo "  $0 --test            # Build and run tests"
    echo "  $0 --clean --test    # Clean, build, and test"
    echo "  $0 --analyze         # Build and run static analysis"
}

# Main function
main() {
    print_status "Starting build process for 3D Game of Life - Vulkan Edition"
    echo ""
    
    # Parse command line arguments
    local clean_build_flag=false
    local run_tests_flag=false
    local run_analysis_flag=false
    
    while [[ $# -gt 0 ]]; do
        case $1 in
            -c|--clean)
                clean_build_flag=true
                shift
                ;;
            -t|--test)
                run_tests_flag=true
                shift
                ;;
            -a|--analyze)
                run_analysis_flag=true
                shift
                ;;
            -h|--help)
                show_help
                exit 0
                ;;
            *)
                print_error "Unknown option: $1"
                show_help
                exit 1
                ;;
        esac
    done
    
    # Check dependencies
    check_dependencies
    
    # Set up environment
    setup_environment
    
    # Clean build if requested
    if [ "$clean_build_flag" = true ]; then
        clean_build --clean
    fi
    
    # Configure build
    configure_build
    
    # Build project
    build_project
    
    # Run tests if requested
    if [ "$run_tests_flag" = true ]; then
        run_tests --test
    fi
    
    # Run static analysis if requested
    if [ "$run_analysis_flag" = true ]; then
        run_static_analysis --analyze
    fi
    
    echo ""
    print_success "Build process completed successfully!"
    print_status "Executable location: build/3DGameOfLife-Vulkan-Edition"
    print_status "To run the application: ./build/3DGameOfLife-Vulkan-Edition"
}

# Run main function with all arguments
main "$@" 
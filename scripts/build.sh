#!/bin/bash

# Exit on error
set -e

# Default values
BUILD_TYPE="Debug"
BUILD_DIR="build"
ENABLE_TESTS=true
ENABLE_COVERAGE=false
ENABLE_SANITIZERS=false
NUM_JOBS=$(nproc)

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --release)
            BUILD_TYPE="Release"
            shift
            ;;
        --build-dir)
            BUILD_DIR="$2"
            shift 2
            ;;
        --no-tests)
            ENABLE_TESTS=false
            shift
            ;;
        --coverage)
            ENABLE_COVERAGE=true
            shift
            ;;
        --sanitizers)
            ENABLE_SANITIZERS=true
            shift
            ;;
        --jobs)
            NUM_JOBS="$2"
            shift 2
            ;;
        *)
            echo "Unknown option: $1"
            exit 1
            ;;
    esac
done

# Create build directory
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure CMake
cmake .. \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DBUILD_TESTING="$ENABLE_TESTS" \
    -DENABLE_COVERAGE="$ENABLE_COVERAGE" \
    -DENABLE_SANITIZERS="$ENABLE_SANITIZERS"

# Build
cmake --build . --config "$BUILD_TYPE" -j "$NUM_JOBS"

# Run tests if enabled
if [ "$ENABLE_TESTS" = true ]; then
    echo "Running tests..."
    cmake --build . --target run-all-tests
fi

# Generate coverage report if enabled
if [ "$ENABLE_COVERAGE" = true ]; then
    echo "Generating coverage report..."
    lcov --capture --directory . --output-file coverage.info
    lcov --remove coverage.info '/usr/*' --output-file coverage.info
    lcov --list coverage.info
    genhtml coverage.info --output-directory coverage
    echo "Coverage report generated in $BUILD_DIR/coverage"
fi

echo "Build completed successfully!" 
#!/bin/bash

# Exit on error
set -e

# Configuration
BUILD_TYPE=${1:-Release}
BUILD_DIR="build"
NUM_JOBS=$(nproc)

# Create build directory if it doesn't exist
mkdir -p $BUILD_DIR

# Configure CMake
echo "Configuring CMake..."
cmake -B $BUILD_DIR -DCMAKE_BUILD_TYPE=$BUILD_TYPE

# Build the project
echo "Building project..."
cmake --build $BUILD_DIR --config $BUILD_TYPE -j $NUM_JOBS

# Run shader compilation
echo "Compiling shaders..."
./scripts/shader_compile.sh

echo "Build completed successfully!" 
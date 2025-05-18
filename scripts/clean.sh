#!/bin/bash

# Exit on error
set -e

# Configuration
BUILD_DIR="build"
SHADER_OUTPUT_DIR="build/shaders"

echo "Cleaning build artifacts..."

# Remove build directory
if [ -d "$BUILD_DIR" ]; then
    echo "Removing build directory..."
    rm -rf $BUILD_DIR
fi

# Remove compiled shaders
if [ -d "$SHADER_OUTPUT_DIR" ]; then
    echo "Removing compiled shaders..."
    rm -rf $SHADER_OUTPUT_DIR
fi

# Remove CMake cache
if [ -f "CMakeCache.txt" ]; then
    echo "Removing CMake cache..."
    rm CMakeCache.txt
fi

# Remove CMake files
if [ -d "CMakeFiles" ]; then
    echo "Removing CMake files..."
    rm -rf CMakeFiles
fi

echo "Cleanup completed!" 
#!/bin/bash

# Exit on error
set -e

# Configuration
SHADER_DIR="shaders"
OUTPUT_DIR="build/shaders"
GLSLC="glslc"

# Create output directory if it doesn't exist
mkdir -p $OUTPUT_DIR

# Function to compile a shader
compile_shader() {
    local input=$1
    local output="${OUTPUT_DIR}/$(basename $input).spv"
    echo "Compiling $input -> $output"
    $GLSLC $input -o $output
}

# Compile all shaders
echo "Compiling shaders..."

# Vertex shaders
for shader in $SHADER_DIR/*.vert; do
    if [ -f "$shader" ]; then
        compile_shader $shader
    fi
done

# Fragment shaders
for shader in $SHADER_DIR/*.frag; do
    if [ -f "$shader" ]; then
        compile_shader $shader
    fi
done

# Compute shaders
for shader in $SHADER_DIR/*.comp; do
    if [ -f "$shader" ]; then
        compile_shader $shader
    fi
done

echo "Shader compilation completed!" 
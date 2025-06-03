#!/bin/bash
# Validate GLSL shader files

set -e

exit_code=0

echo "🎨 Validating GLSL shaders..."

# Try to find shader validator
validator=""
if command -v glslangValidator >/dev/null 2>&1; then
    validator="glslangValidator"
elif command -v glslc >/dev/null 2>&1; then
    validator="glslc"
elif [[ -n "$VULKAN_SDK" ]] && [[ -f "$VULKAN_SDK/bin/glslangValidator" ]]; then
    validator="$VULKAN_SDK/bin/glslangValidator"
elif [[ -n "$VULKAN_SDK" ]] && [[ -f "$VULKAN_SDK/bin/glslc" ]]; then
    validator="$VULKAN_SDK/bin/glslc"
else
    echo "⚠️  No shader validator found (glslangValidator or glslc)."
    echo "   Install Vulkan SDK or ensure shader validator is in PATH."
    echo "   Skipping shader validation."
    exit 0
fi

for file in "$@"; do
    if [[ -f "$file" ]]; then
        echo "  Validating: $file"
        
        # Determine shader stage from extension
        stage=""
        case "${file##*.}" in
            vert) stage="vertex" ;;
            frag) stage="fragment" ;;
            comp) stage="compute" ;;
            geom) stage="geometry" ;;
            tesc) stage="tesscontrol" ;;
            tese) stage="tesseval" ;;
            glsl) stage="" ;;  # Generic GLSL
            *) stage="" ;;
        esac
        
        # Validate with appropriate tool
        if [[ "$validator" == *"glslangValidator"* ]]; then
            if [[ -n "$stage" ]]; then
                if ! "$validator" -S "$stage" "$file" >/dev/null 2>&1; then
                    echo "❌ Shader validation failed: $file"
                    "$validator" -S "$stage" "$file"
                    exit_code=1
                fi
            else
                if ! "$validator" "$file" >/dev/null 2>&1; then
                    echo "❌ Shader validation failed: $file"
                    "$validator" "$file"
                    exit_code=1
                fi
            fi
        elif [[ "$validator" == *"glslc"* ]]; then
            # Use glslc for validation
            if ! "$validator" -fsyntax-only "$file" >/dev/null 2>&1; then
                echo "❌ Shader validation failed: $file"
                "$validator" -fsyntax-only "$file"
                exit_code=1
            fi
        fi
    fi
done

if [[ $exit_code -eq 0 ]]; then
    echo "✅ All shaders validated successfully."
fi

exit $exit_code 
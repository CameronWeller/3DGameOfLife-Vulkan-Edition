#!/bin/bash
# Validate vcpkg.json manifest file

set -e

exit_code=0

echo "📦 Validating vcpkg.json manifest..."

for file in "$@"; do
    if [[ -f "$file" && "$file" == *"vcpkg.json" ]]; then
        echo "  Validating: $file"
        
        # Check if it's valid JSON
        if ! python3 -m json.tool "$file" >/dev/null 2>&1; then
            echo "❌ Invalid JSON syntax in: $file"
            exit_code=1
            continue
        fi
        
        # Check for required fields
        required_fields=("name" "version")
        for field in "${required_fields[@]}"; do
            if ! grep -q "\"$field\"" "$file"; then
                echo "❌ Missing required field '$field' in: $file"
                exit_code=1
            fi
        done
        
        # Check for valid version format
        if grep -q "\"version\"" "$file"; then
            version=$(grep "\"version\"" "$file" | sed 's/.*"version":[[:space:]]*"\([^"]*\)".*/\1/')
            if ! echo "$version" | grep -qE '^[0-9]+\.[0-9]+\.[0-9]+(-[a-zA-Z0-9.-]+)?$'; then
                echo "⚠️  Version format might not follow semantic versioning: $version"
                echo "   Consider using format: major.minor.patch[-prerelease]"
            fi
        fi
        
        # Check for common Vulkan dependencies
        vulkan_deps=("vulkan" "glfw3" "glm" "imgui")
        found_vulkan_deps=()
        
        for dep in "${vulkan_deps[@]}"; do
            if grep -q "\"$dep\"" "$file"; then
                found_vulkan_deps+=("$dep")
            fi
        done
        
        if [[ ${#found_vulkan_deps[@]} -gt 0 ]]; then
            echo "  ✅ Found Vulkan-related dependencies: ${found_vulkan_deps[*]}"
        fi
        
        # Check for deprecated packages
        deprecated_packages=("freeglut" "opengl" "glew")
        for dep in "${deprecated_packages[@]}"; do
            if grep -q "\"$dep\"" "$file"; then
                echo "⚠️  Consider modern alternatives to '$dep' for Vulkan projects"
            fi
        done
        
        # Validate dependency structure
        if grep -q "\"dependencies\"" "$file"; then
            # Check if dependencies is an array
            if ! grep -A 50 "\"dependencies\"" "$file" | grep -q "\["; then
                echo "❌ Dependencies should be an array in: $file"
                exit_code=1
            fi
        fi
        
        # Check for features
        if grep -q "\"features\"" "$file"; then
            echo "  ℹ️  Features detected - ensure they are properly configured"
        fi
        
        # Check for overrides (usually not recommended)
        if grep -q "\"overrides\"" "$file"; then
            echo "  ⚠️  Overrides detected - use with caution"
        fi
        
        echo "  ✅ vcpkg.json validation completed"
    fi
done

if [[ $exit_code -eq 0 ]]; then
    echo "✅ vcpkg.json validation passed."
fi

exit $exit_code 
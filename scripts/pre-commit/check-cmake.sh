#!/bin/bash
# Check CMake files for best practices

set -e

exit_code=0

echo "🔨 Checking CMake best practices..."

for file in "$@"; do
    if [[ -f "$file" ]]; then
        echo "  Checking: $file"
        
        # Check for minimum CMake version
        if [[ "$file" == "CMakeLists.txt" ]] && ! grep -q "cmake_minimum_required" "$file"; then
            echo "❌ Missing cmake_minimum_required in: $file"
            exit_code=1
        fi
        
        # Check for project() call in main CMakeLists.txt
        if [[ "$file" == "CMakeLists.txt" ]] && [[ "$file" == "./CMakeLists.txt" ]] && ! grep -q "project(" "$file"; then
            echo "❌ Missing project() call in main CMakeLists.txt"
            exit_code=1
        fi
        
        # Check for deprecated commands
        deprecated_commands=(
            "INCLUDE_REGULAR_EXPRESSION"
            "EXEC_PROGRAM"
            "MAKE_DIRECTORY"
            "SUBDIR_DEPENDS"
            "SUBDIRS"
            "USE_MANGLED_MESA"
            "VARIABLE_REQUIRES"
            "WRITE_FILE"
        )
        
        for cmd in "${deprecated_commands[@]}"; do
            if grep -qi "$cmd" "$file"; then
                echo "⚠️  Deprecated CMake command '$cmd' found in: $file"
                echo "   Consider using modern alternatives."
                exit_code=1
            fi
        done
        
        # Check for proper target-based approach
        if grep -q "link_directories" "$file"; then
            echo "⚠️  Found link_directories() in: $file"
            echo "   Consider using target_link_libraries() with full paths instead."
        fi
        
        if grep -q "include_directories" "$file"; then
            echo "⚠️  Found include_directories() in: $file"
            echo "   Consider using target_include_directories() for better scoping."
        fi
        
        # Check for proper variable usage
        if grep -qE '\${CMAKE_SOURCE_DIR}.*\${CMAKE_SOURCE_DIR}' "$file"; then
            echo "⚠️  Nested CMAKE_SOURCE_DIR usage in: $file"
            echo "   This might indicate incorrect path construction."
        fi
        
        # Check for hardcoded paths
        if grep -qE '(C:|D:|/usr/local)' "$file"; then
            echo "⚠️  Hardcoded absolute paths found in: $file"
            echo "   Use relative paths or CMake variables instead."
            exit_code=1
        fi
        
        # Check for proper generator expressions
        if grep -qE '\$<CONFIG:.*>' "$file"; then
            echo "ℹ️  Generator expressions found in: $file"
            echo "   Ensure they are used correctly for multi-config generators."
        fi
        
        echo "  ✅ CMake check completed for: $file"
    fi
done

if [[ $exit_code -eq 0 ]]; then
    echo "✅ CMake best practices check passed."
fi

exit $exit_code 
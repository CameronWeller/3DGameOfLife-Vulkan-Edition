#!/bin/bash
# Check for proper include guards in C++ header files

set -e

exit_code=0

echo "🛡️  Checking C++ include guards..."

for file in "$@"; do
    if [[ -f "$file" && "$file" == *.hpp ]]; then
        echo "  Checking: $file"
        
        # Generate expected include guard based on file path
        # Convert path to uppercase, replace / and . with _
        guard_name=$(echo "$file" | tr '[:lower:]' '[:upper:]' | sed 's/[\/\.]/_/g' | sed 's/^.*\///g')_H
        
        # Check if file has include guards
        if ! grep -q "#ifndef.*_H" "$file"; then
            echo "❌ Missing include guard in: $file"
            echo "   Expected: #ifndef ${guard_name}"
            exit_code=1
            continue
        fi
        
        # Check if #ifndef and #define match
        ifndef_line=$(grep "#ifndef.*_H" "$file" | head -n1)
        define_line=$(grep "#define.*_H" "$file" | head -n1)
        
        if [[ -z "$ifndef_line" || -z "$define_line" ]]; then
            echo "❌ Incomplete include guard in: $file"
            exit_code=1
            continue
        fi
        
        # Extract guard names
        ifndef_guard=$(echo "$ifndef_line" | sed 's/#ifndef[[:space:]]*//')
        define_guard=$(echo "$define_line" | sed 's/#define[[:space:]]*//')
        
        if [[ "$ifndef_guard" != "$define_guard" ]]; then
            echo "❌ Mismatched include guard in: $file"
            echo "   #ifndef: $ifndef_guard"
            echo "   #define: $define_guard"
            exit_code=1
            continue
        fi
        
        # Check if there's a closing #endif
        if ! grep -q "#endif" "$file"; then
            echo "❌ Missing #endif in: $file"
            exit_code=1
            continue
        fi
        
        echo "  ✅ Include guard OK: $ifndef_guard"
    fi
done

if [[ $exit_code -eq 0 ]]; then
    echo "✅ All include guards are properly formatted."
fi

exit $exit_code 
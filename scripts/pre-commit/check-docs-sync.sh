#!/bin/bash
# Check that documentation is in sync with code

set -e

exit_code=0

echo "📚 Checking documentation synchronization..."

for file in "$@"; do
    if [[ -f "$file" ]]; then
        echo "  Checking: $file"
        
        # Check README.md for common issues
        if [[ "$file" == "README.md" ]]; then
            # Check if build instructions mention correct CMake version
            if grep -q "cmake" "$file"; then
                if ! grep -qE "CMake [0-9]+\.[0-9]+" "$file"; then
                    echo "  ⚠️  README mentions CMake but no version specified"
                fi
            fi
            
            # Check if Vulkan version is mentioned
            if ! grep -qE "Vulkan [0-9]+\.[0-9]+" "$file"; then
                echo "  ⚠️  Consider specifying Vulkan version requirement in README"
            fi
            
            # Check for broken internal links
            broken_links=()
            while IFS= read -r line; do
                if echo "$line" | grep -qE '\[.*\]\([^http].*\)'; then
                    link=$(echo "$line" | grep -oE '\([^http][^)]*\)' | tr -d '()')
                    if [[ ! -f "$link" && ! -d "$link" ]]; then
                        broken_links+=("$link")
                    fi
                fi
            done < "$file"
            
            if [[ ${#broken_links[@]} -gt 0 ]]; then
                echo "  ❌ Broken internal links found in README:"
                printf '    %s\n' "${broken_links[@]}"
                exit_code=1
            fi
            
            # Check if API documentation is mentioned
            if grep -qi "api\|documentation" "$file"; then
                if [[ -d "docs" ]] && ! grep -q "docs/" "$file"; then
                    echo "  ℹ️  Consider linking to docs/ directory in README"
                fi
            fi
        fi
        
        # Check docs directory files
        if [[ "$file" == docs/* ]]; then
            # Check for outdated code examples
            if grep -q "```cpp\|```c\|```cmake" "$file"; then
                echo "  ℹ️  Code examples found - ensure they are up to date"
                
                # Check for common outdated patterns
                if grep -q "Vulkan-Hpp\|vulkan.hpp" "$file"; then
                    echo "  ℹ️  Vulkan-Hpp usage detected - ensure version compatibility"
                fi
            fi
            
            # Check for TODO/FIXME in documentation
            if grep -qE "(TODO|FIXME|XXX)" "$file"; then
                echo "  ⚠️  TODO/FIXME found in documentation: $file"
                echo "     Consider completing before release"
            fi
        fi
        
        # Check for version consistency
        if [[ "$file" == "README.md" || "$file" == "docs/"* ]]; then
            # Look for version numbers and check against VERSION file
            if [[ -f "VERSION" ]]; then
                project_version=$(cat VERSION | tr -d ' \n')
                
                # Check if documentation mentions a different version
                if grep -qE "[0-9]+\.[0-9]+\.[0-9]+" "$file"; then
                    doc_versions=$(grep -oE "[0-9]+\.[0-9]+\.[0-9]+" "$file")
                    for doc_ver in $doc_versions; do
                        if [[ "$doc_ver" != "$project_version" ]]; then
                            echo "  ⚠️  Version mismatch in $file: $doc_ver (project: $project_version)"
                        fi
                    done
                fi
            fi
        fi
        
        # Check for build instructions accuracy
        if [[ "$file" == "README.md" ]] && grep -q "cmake.*build" "$file"; then
            # Verify CMakeLists.txt exists
            if [[ ! -f "CMakeLists.txt" ]]; then
                echo "  ❌ README mentions CMake build but CMakeLists.txt not found"
                exit_code=1
            fi
            
            # Check if mentioned build targets exist in CMakeLists.txt
            if grep -qE "(make|cmake --build)" "$file"; then
                # Extract mentioned targets
                build_commands=$(grep -oE "cmake --build.*--target [a-zA-Z_-]+" "$file" || true)
                if [[ -n "$build_commands" ]]; then
                    while IFS= read -r cmd; do
                        target=$(echo "$cmd" | grep -oE "--target [a-zA-Z_-]+" | sed 's/--target //')
                        if [[ -n "$target" ]] && ! grep -q "add_custom_target($target\|add_executable($target\|add_library($target" CMakeLists.txt; then
                            echo "  ⚠️  README mentions target '$target' not found in CMakeLists.txt"
                        fi
                    done <<< "$build_commands"
                fi
            fi
        fi
        
        echo "  ✅ Documentation sync check completed for: $file"
    fi
done

if [[ $exit_code -eq 0 ]]; then
    echo "✅ Documentation synchronization check passed."
fi

exit $exit_code 
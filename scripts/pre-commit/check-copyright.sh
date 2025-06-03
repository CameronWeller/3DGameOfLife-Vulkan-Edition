#!/bin/bash
# Check for proper copyright headers in source files

set -e

exit_code=0
current_year=$(date +%Y)

echo "©️  Checking copyright headers..."

for file in "$@"; do
    if [[ -f "$file" ]]; then
        echo "  Checking: $file"
        
        # Check if file has any copyright notice
        if ! grep -qi "copyright\|©" "$file"; then
            echo "⚠️  No copyright notice found in: $file"
            echo "   Consider adding a copyright header."
            echo "   Example:"
            echo "   /*"
            echo "    * Copyright (c) $current_year YourName/YourOrganization"
            echo "    * Licensed under the MIT License"
            echo "    */"
            # Don't fail for missing copyright, just warn
            continue
        fi
        
        # Check if copyright year is current or reasonable
        if grep -qi "copyright" "$file"; then
            copyright_line=$(grep -i "copyright" "$file" | head -n1)
            
            # Extract year from copyright line
            if echo "$copyright_line" | grep -qE "[0-9]{4}"; then
                year=$(echo "$copyright_line" | grep -oE "[0-9]{4}" | tail -n1)
                
                # Check if year is reasonable (not in the future, not too old)
                if [[ "$year" -gt "$current_year" ]]; then
                    echo "⚠️  Copyright year is in the future in: $file"
                    echo "   Line: $copyright_line"
                elif [[ "$year" -lt "$((current_year - 10))" ]]; then
                    echo "ℹ️  Copyright year might be outdated in: $file"
                    echo "   Consider updating: $copyright_line"
                fi
            fi
        fi
        
        # Check for license information
        license_patterns=(
            "MIT License"
            "Apache License"
            "GPL"
            "BSD"
            "Licensed under"
            "SPDX-License-Identifier"
        )
        
        has_license=false
        for pattern in "${license_patterns[@]}"; do
            if grep -qi "$pattern" "$file"; then
                has_license=true
                break
            fi
        done
        
        if [[ "$has_license" == false ]]; then
            echo "ℹ️  No license information found in: $file"
            echo "   Consider adding license information to the header."
        fi
        
        echo "  ✅ Copyright check completed for: $file"
    fi
done

if [[ $exit_code -eq 0 ]]; then
    echo "✅ Copyright header check completed."
fi

exit $exit_code 
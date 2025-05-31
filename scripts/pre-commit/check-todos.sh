#!/bin/bash
# Check for TODO/FIXME comments in production code

set -e

exit_code=0
todo_pattern='(TODO|FIXME|XXX|HACK)'

echo "🔍 Checking for TODO/FIXME comments in production code..."

for file in "$@"; do
    if [[ -f "$file" ]]; then
        if grep -n -E "$todo_pattern" "$file"; then
            echo "❌ Found TODO/FIXME comments in: $file"
            echo "   Please resolve these before committing to production code."
            exit_code=1
        fi
    fi
done

if [[ $exit_code -eq 0 ]]; then
    echo "✅ No TODO/FIXME comments found in production code."
fi

exit $exit_code 
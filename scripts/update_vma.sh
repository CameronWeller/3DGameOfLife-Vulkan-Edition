#!/bin/bash

# Update VMA submodule to latest version
# This script ensures we always have the latest VMA version with full documentation

echo "Updating VMA submodule to latest version..."

cd "$(dirname "$0")/.."

# Update the submodule to the latest commit
git submodule update --remote third_party/VulkanMemoryAllocator

# Check if there are any changes
if git diff --quiet HEAD -- third_party/VulkanMemoryAllocator; then
    echo "VMA is already up to date."
else
    echo "VMA submodule updated. Changes:"
    git diff HEAD -- third_party/VulkanMemoryAllocator
    echo ""
    echo "To commit these changes, run:"
    echo "git add third_party/VulkanMemoryAllocator"
    echo "git commit -m 'Update VMA submodule to latest version'"
fi 
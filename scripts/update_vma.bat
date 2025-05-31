@echo off
REM Update VMA submodule to latest version
REM This script ensures we always have the latest VMA version with full documentation

echo Updating VMA submodule to latest version...

cd /d "%~dp0\.."

REM Update the submodule to the latest commit
git submodule update --remote third_party/VulkanMemoryAllocator

REM Check if there are any changes
git diff --quiet HEAD -- third_party/VulkanMemoryAllocator
if %errorlevel% equ 0 (
    echo VMA is already up to date.
) else (
    echo VMA submodule updated. Changes:
    git diff HEAD -- third_party/VulkanMemoryAllocator
    echo.
    echo To commit these changes, run:
    echo git add third_party/VulkanMemoryAllocator
    echo git commit -m "Update VMA submodule to latest version"
)

pause 
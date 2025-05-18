@echo off
setlocal enabledelayedexpansion

echo Copying build from Docker container...

:: Create build directory if it doesn't exist
if not exist "build" mkdir build

:: Copy the build files from the Docker container
docker cp vulkan-engine:/workspace/build/. ./build/

:: Verify the copy
if exist "build\vulkan-engine.exe" (
    echo Successfully copied vulkan-engine.exe
) else (
    echo Failed to copy vulkan-engine.exe
    exit /b 1
)

:: Copy shaders
if exist "build\shaders" (
    echo Successfully copied shaders directory
) else (
    echo Failed to copy shaders directory
    exit /b 1
)

echo Build files copied successfully!
exit /b 0 
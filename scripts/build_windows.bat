@echo off
setlocal enabledelayedexpansion

:: Check for Visual Studio installation
if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" (
    call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
) else if exist "C:\Program Files (x86)\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" (
    call "C:\Program Files (x86)\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
) else (
    echo Visual Studio 2022 not found. Please install Visual Studio 2022 with C++ development tools.
    exit /b 1
)

:: Check for Vulkan SDK
if not defined VULKAN_SDK (
    echo Vulkan SDK not found in environment. Please install Vulkan SDK and ensure it's in your PATH.
    exit /b 1
)

:: Create and enter build directory
if not exist "build" mkdir build
cd build

:: Configure with CMake
cmake -G "Visual Studio 17 2022" -A x64 ..

:: Build
cmake --build . --config Release

:: Create shaders directory if it doesn't exist
if not exist "Release\shaders" mkdir "Release\shaders"

:: Copy shaders to output directory
if exist "..\shaders\*.spv" (
    copy /Y "..\shaders\*.spv" "Release\shaders\"
)

:: Return to original directory
cd ..

echo Build completed. Executable is in build\Release\vulkan-engine.exe 
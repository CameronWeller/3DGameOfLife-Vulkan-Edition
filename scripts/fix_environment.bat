@echo off
setlocal enabledelayedexpansion

echo Checking and fixing environment variables...

:: Check for Visual Studio
where cl.exe >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo Visual Studio not found in PATH. Attempting to find it...
    set "VS_PATH=%ProgramFiles(x86)%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
    if exist "!VS_PATH!" (
        echo Found Visual Studio 2022. Adding to PATH...
        call "!VS_PATH!"
    ) else (
        echo Error: Visual Studio not found. Please install Visual Studio 2022 with C++ development tools.
        exit /b 1
    )
)

:: Check for Vulkan SDK
if "%VULKAN_SDK%"=="" (
    echo Vulkan SDK not found in environment. Attempting to find it...
    set "FOUND=0"
    for %%p in (
        "C:\VulkanSDK"
        "%ProgramFiles%\VulkanSDK"
        "%ProgramFiles(x86)%\VulkanSDK"
    ) do (
        if exist "%%~p" (
            for /f "delims=" %%v in ('dir /b /ad "%%~p" ^| sort /r') do (
                set "VULKAN_SDK=%%~p\%%v"
                set "FOUND=1"
                goto :found_vulkan
            )
        )
    )
    :found_vulkan
    if !FOUND!==0 (
        echo Error: Vulkan SDK not found. Please install the Vulkan SDK.
        exit /b 1
    )
    setx VULKAN_SDK "!VULKAN_SDK!"
    set "PATH=!VULKAN_SDK!\Bin;!PATH!"
)

:: Check for vcpkg
if "%VCPKG_ROOT%"=="" (
    echo vcpkg not found in environment. Attempting to find it...
    set "FOUND=0"
    for %%p in (
        "C:\vcpkg"
        "%ProgramFiles%\vcpkg"
        "%USERPROFILE%\vcpkg"
    ) do (
        if exist "%%~p" (
            set "VCPKG_ROOT=%%~p"
            set "FOUND=1"
            goto :found_vcpkg
        )
    )
    :found_vcpkg
    if !FOUND!==0 (
        echo Error: vcpkg not found. Please install vcpkg.
        exit /b 1
    )
    setx VCPKG_ROOT "!VCPKG_ROOT!"
    set "PATH=!VCPKG_ROOT!;!PATH!"
)

:: Create build directory if it doesn't exist
if not exist "build" mkdir build

:: Run CMake configuration
echo Configuring CMake...
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE="%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake" -DCMAKE_BUILD_TYPE=Debug

:: Build the project
echo Building the project...
cmake --build . --config Debug

echo Environment setup complete!
cd .. 
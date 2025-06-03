@echo off
echo Running Minimal Vulkan App...
echo.

REM Check if the executable exists
if not exist "build_minimal\Release\minimal_vulkan_app.exe" (
    echo ERROR: Executable not found at build_minimal\Release\minimal_vulkan_app.exe
    echo Please build the project first using the build script.
    pause
    exit /b 1
)

REM Run the application
echo Starting application...
echo Controls:
echo   ESC - Toggle mouse look
echo   WASD - Move camera
echo   Space/Ctrl - Move up/down
echo   R - Reset camera
echo   1 - Fly mode, 2 - Orbit mode
echo.

build_minimal\Release\minimal_vulkan_app.exe

echo.
echo Application finished.
pause 
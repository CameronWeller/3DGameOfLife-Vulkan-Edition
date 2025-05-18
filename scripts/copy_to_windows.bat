@echo off
setlocal enabledelayedexpansion

:: Create output directory if it doesn't exist
if not exist "build\Release" mkdir "build\Release"
if not exist "build\Release\shaders" mkdir "build\Release\shaders"

:: Copy executable from Docker build
copy /Y "build\vulkan-engine" "build\Release\vulkan-engine.exe"

:: Copy shaders
xcopy /Y /I "shaders\*.spv" "build\Release\shaders\"

echo Files copied successfully to build\Release\ 
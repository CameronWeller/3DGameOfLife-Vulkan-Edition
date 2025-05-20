@echo off
setlocal enabledelayedexpansion

:: Create temp directory
if not exist "temp" mkdir temp
cd temp

:: Get latest CMake version
echo Downloading CMake version information...
powershell -Command "& {Invoke-WebRequest -Uri 'https://api.github.com/repos/Kitware/CMake/releases/latest' -OutFile 'latest.json'}"
for /f "tokens=* usebackq" %%a in (`powershell -Command "& {(Get-Content 'latest.json' | ConvertFrom-Json).tag_name}"`) do set VERSION=%%a
set VERSION=%VERSION:v=%

:: Download CMake installer
echo Downloading CMake %VERSION%...
set INSTALLER=cmake-%VERSION%-windows-x86_64.msi
powershell -Command "& {Invoke-WebRequest -Uri 'https://github.com/Kitware/CMake/releases/download/v%VERSION%/%INSTALLER%' -OutFile '%INSTALLER%'}"

:: Install CMake
echo Installing CMake...
msiexec /i %INSTALLER% /quiet ADD_CMAKE_TO_PATH=System

:: Clean up
cd ..
rmdir /s /q temp

echo CMake %VERSION% has been installed. Please restart your terminal to use the new version. 
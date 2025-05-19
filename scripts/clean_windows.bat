@echo off
echo Cleaning build directory...

if exist "build" (
    rmdir /s /q build
    echo Build directory removed successfully.
) else (
    echo Build directory not found. Nothing to clean.
)

echo Clean operation complete. 
@echo off
echo Starting setup process...

:: Check if running as administrator
net session >nul 2>&1
if %errorLevel% neq 0 (
    echo Please run this script as Administrator
    echo Right-click on setup.bat and select "Run as administrator"
    pause
    exit /b 1
)

:: Run the PowerShell script
powershell.exe -ExecutionPolicy Bypass -File "%~dp0setup.ps1"

:: Check if the script completed successfully
if %errorLevel% neq 0 (
    echo Setup failed. Please check setup.log for details.
    pause
    exit /b 1
)

echo Setup completed successfully!
pause 
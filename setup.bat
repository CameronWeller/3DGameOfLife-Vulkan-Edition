@echo off
setlocal enabledelayedexpansion

:: Check if running as administrator
net session >nul 2>&1
if %errorLevel% neq 0 (
    echo Please run this script as Administrator
    pause
    exit /b 1
)

:: Check if PowerShell is installed
where powershell >nul 2>&1
if %errorLevel% neq 0 (
    echo PowerShell not found. Installing PowerShell...
    
    :: Download PowerShell installer using curl
    echo Downloading PowerShell installer...
    curl -L -o PowerShell.msi https://github.com/PowerShell/PowerShell/releases/download/v7.3.8/PowerShell-7.3.8-win-x64.msi
    
    :: Install PowerShell
    echo Installing PowerShell...
    msiexec /i PowerShell.msi /qn
    
    :: Clean up
    del PowerShell.msi
    
    :: Add PowerShell to PATH
    setx PATH "%PATH%;C:\Program Files\PowerShell\7" /M
)

:: Wait for PowerShell to be available
timeout /t 10 /nobreak

:: Run the PowerShell setup script
"C:\Program Files\PowerShell\7\pwsh.exe" -ExecutionPolicy Bypass -File setup.ps1

:: Check if the script completed successfully
if %errorLevel% neq 0 (
    echo Setup failed. Please check setup.log for details.
    pause
    exit /b 1
)

echo Setup completed successfully!
pause 
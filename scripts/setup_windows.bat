@echo off
echo Installing Chocolatey package manager...
powershell -Command "Set-ExecutionPolicy Bypass -Scope Process -Force; [System.Net.ServicePointManager]::SecurityProtocol = [System.Net.ServicePointManager]::SecurityProtocol -bor 3072; iex ((New-Object System.Net.WebClient).DownloadString('https://community.chocolatey.org/install.ps1'))"

echo Installing VcXsrv...
choco install vcxsrv -y

echo Creating X server startup script...
echo @echo off > "%USERPROFILE%\start_xserver.bat"
echo "C:\Program Files\VcXsrv\vcxsrv.exe" :0 -multiwindow -clipboard -wgl -ac >> "%USERPROFILE%\start_xserver.bat"

echo Setting up DISPLAY environment variable...
setx DISPLAY "localhost:0.0"

echo Creating scheduled task to start X server on login...
powershell -Command "Register-ScheduledTask -TaskName 'StartXServer' -Trigger (New-ScheduledTaskTrigger -AtLogon) -Action (New-ScheduledTaskAction -Execute '%USERPROFILE%\start_xserver.bat') -RunLevel Highest -Force"

echo Starting X server...
start "" "%USERPROFILE%\start_xserver.bat"

echo Setup complete! The X server should now be running.
echo You can now run your Docker container with graphics support. 
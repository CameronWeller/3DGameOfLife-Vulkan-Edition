@echo off
echo Running build with 30-second timeout...

:: Start the build process with a 30-second timeout
powershell -Command "& { $process = Start-Process -FilePath 'cmd.exe' -ArgumentList '/c', '%~dp0build_windows.bat' -NoNewWindow -PassThru -RedirectStandardOutput 'build_output.log' -RedirectStandardError 'build_error.log'; if (-not (Wait-Process -InputObject $process -Timeout 30 -ErrorAction SilentlyContinue)) { Write-Host 'Build timed out after 30 seconds, terminating...'; Stop-Process -InputObject $process -Force } }"

echo Build completed or timed out after 30 seconds.
echo Full build output is in build_output.log

:: Display the executable location info
echo Executable is in build\Release\vulkan-engine.exe 
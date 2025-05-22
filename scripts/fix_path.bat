@echo off
echo Running bootstrap script with full PowerShell path...
"C:\Program Files\PowerShell\7\pwsh.exe" -Command "& {Set-Location 'C:\cpp-vulkan-hip-engine\vcpkg'; .\bootstrap-vcpkg.bat}"
echo Done.
pause 
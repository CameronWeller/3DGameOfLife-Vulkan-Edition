@echo off
set "PATH=%PATH%;C:\Program Files\PowerShell\7"
"C:\Program Files\PowerShell\7\pwsh.exe" -Command "& {Set-ExecutionPolicy Bypass -Scope Process -Force; C:\vcpkg\bootstrap-vcpkg.bat}"

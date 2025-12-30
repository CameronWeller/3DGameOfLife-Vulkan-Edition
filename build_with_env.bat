@echo off
echo Setting up Visual Studio environment...
call "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\VC\\Auxiliary\\Build\\vcvarsall.bat" x64
echo.
echo Adding Windows SDK to PATH and LIB...
set "PATH=%PATH%;C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64"
set "LIB=%LIB%;C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\um\x64;C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\ucrt\x64"
echo.
echo Current LIB path:
echo %LIB%
echo.
echo Running CMake...
cmake -B build_opengl -G Ninja
echo.
echo Running build script...
.\scripts\build_fast.ps1 -BuildDirectory build_opengl

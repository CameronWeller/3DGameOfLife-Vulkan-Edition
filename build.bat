@echo off
set VCPKG_ROOT=C:\vcpkg_new_20250519_153643
cd C:\cpp-vulkan-hip-engine
if not exist build mkdir build
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake -DCMAKE_PREFIX_PATH=%VCPKG_ROOT%\installed\x64-windows
cmake --build . --config Release 
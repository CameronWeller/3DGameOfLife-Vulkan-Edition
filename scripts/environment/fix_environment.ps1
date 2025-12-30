# Fix environment and build issues script
$ErrorActionPreference = "Stop"

# Function to check if a command exists
function Test-Command($cmdname) {
    return [bool](Get-Command -Name $cmdname -ErrorAction SilentlyContinue)
}

# Find Visual Studio
$vsPath = $null
$vsPaths = @(
    "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\VC\\Auxiliary\\Build\\vcvarsall.bat",
    "C:\\Program Files (x86)\\Microsoft Visual Studio\\2022\\Community\\VC\\Auxiliary\\Build\\vcvarsall.bat",
    "C:\\Program Files (x86)\\Microsoft Visual Studio\\2022\\BuildTools\\VC\\Auxiliary\\Build\\vcvarsall.bat",
    "C:\\Program Files\\Microsoft Visual Studio\\2019\\Community\\VC\\Auxiliary\\Build\\vcvarsall.bat",
    "C:\\Program Files (x86)\\Microsoft Visual Studio\\2019\\Community\\VC\\Auxiliary\\Build\\vcvarsall.bat",
    "C:\\Program Files (x86)\\Microsoft Visual Studio\\2019\\BuildTools\\VC\\Auxiliary\\Build\\vcvarsall.bat"
)
foreach ($path in $vsPaths) {
    if (Test-Path $path) {
        $vsPath = $path
        break
    }
}

if (-not $vsPath) {
    Write-Error "Visual Studio not found. Please install Visual Studio with C++ development tools."
    exit 1
}

# Find Windows 10 SDK
$sdk_key = "HKLM:\SOFTWARE\Wow6432Node\Microsoft\Microsoft SDKs\Windows\v10.0"
$sdk = Get-ItemProperty -Path $sdk_key -ErrorAction SilentlyContinue
if (-not $sdk) {
    $sdk_key = "HKLM:\SOFTWARE\Microsoft\Microsoft SDKs\Windows\v10.0"
    $sdk = Get-ItemProperty -Path $sdk_key -ErrorAction SilentlyContinue
}

if (-not $sdk) {
    Write-Error "Windows 10 SDK not found in registry. Please ensure the Windows 10/11 SDK is installed."
    exit 1
}

$sdk_root = $sdk.InstallationFolder
# Find latest SDK version by looking at include directories
$sdk_version = (Get-ChildItem (Join-Path $sdk_root "Include")).Name | Sort-Object -Descending | Select-Object -First 1
$sdk_bin_path = Join-Path $sdk_root "bin\$sdk_version\x64"
$sdk_lib_path = Join-Path $sdk_root "Lib\$sdk_version\um\x64"
$sdk_ucrt_lib_path = Join-Path $sdk_root "Lib\$sdk_version\ucrt\x64"

# Create a batch file to run the build
$batchContent = @"
@echo off
echo Setting up Visual Studio environment...
call "$vsPath" x64
echo.
echo Adding Windows SDK to PATH and LIB...
set "PATH=%PATH%;$sdk_bin_path"
set "LIB=%LIB%;$sdk_lib_path;$sdk_ucrt_lib_path"
echo.
echo Current LIB path:
echo %LIB%
echo.
echo Running CMake...
cmake -B build_opengl -G Ninja
echo.
echo Running build script...
.\scripts\build_fast.ps1 -BuildDirectory build_opengl
"@

$batchContent | Out-File -FilePath "build_with_env.bat" -Encoding ascii

Write-Host "Created build_with_env.bat. Run it to build the project." 
# Diagnostic script to check environment and vcpkg state
$ErrorActionPreference = "Stop"

Write-Host "=== Environment Diagnostics ==="
Write-Host ""

# Check PATH
Write-Host "Current PATH:"
$env:Path -split ';' | ForEach-Object { Write-Host "  $_" }
Write-Host ""

# Check Visual Studio
Write-Host "Visual Studio Environment:"
Write-Host "VSINSTALLDIR: $env:VSINSTALLDIR"
Write-Host "VCINSTALLDIR: $env:VCINSTALLDIR"
Write-Host "VCPKG_VISUAL_STUDIO_PATH: $env:VCPKG_VISUAL_STUDIO_PATH"
Write-Host ""

# Check vcpkg state
Write-Host "vcpkg State:"
$vcpkgRoot = "vcpkg"
if (Test-Path $vcpkgRoot) {
    Write-Host "vcpkg directory exists"
    
    # Check for lock files
    $lockFiles = Get-ChildItem -Path $vcpkgRoot -Filter "*.lock" -Recurse
    if ($lockFiles) {
        Write-Host "Found lock files:"
        $lockFiles | ForEach-Object { Write-Host "  $($_.FullName)" }
    } else {
        Write-Host "No lock files found"
    }
    
    # Check vcpkg.json
    $manifestPath = Join-Path $vcpkgRoot "vcpkg.json"
    if (Test-Path $manifestPath) {
        Write-Host "vcpkg.json exists"
        Get-Content $manifestPath | Write-Host
    } else {
        Write-Host "vcpkg.json not found"
    }
} else {
    Write-Host "vcpkg directory not found"
}

# Check for running vcpkg processes
Write-Host "`nRunning vcpkg processes:"
Get-Process | Where-Object { $_.ProcessName -like "*vcpkg*" } | Format-Table Id, ProcessName, Path

Write-Host "`n=== End of Diagnostics ===" 
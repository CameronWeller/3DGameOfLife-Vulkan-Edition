Write-Host "Running build with 30-second timeout..."

# Start the build process
$buildProcess = Start-Process -FilePath "cmd.exe" -ArgumentList "/c", "$PSScriptRoot\build_windows.bat" -NoNewWindow -PassThru

# Wait for up to 30 seconds
if (-not (Wait-Process -InputObject $buildProcess -Timeout 30 -ErrorAction SilentlyContinue)) {
    Write-Host "Build timed out after 30 seconds, terminating process..."
    Stop-Process -InputObject $buildProcess -Force
} else {
    Write-Host "Build completed within 30 seconds."
}

Write-Host "Executable is in build\Release\vulkan-engine.exe" 
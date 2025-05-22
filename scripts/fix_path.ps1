# Add PowerShell to the system PATH
$powershellPath = "C:\Program Files\PowerShell\7"
$currentPath = [Environment]::GetEnvironmentVariable("Path", "Machine")
if (-not $currentPath.Contains($powershellPath)) {
    $newPath = $currentPath + ";" + $powershellPath
    [Environment]::SetEnvironmentVariable("Path", $newPath, "Machine")
    Write-Host "PowerShell added to PATH."
} else {
    Write-Host "PowerShell is already in PATH."
}

# Run the bootstrap script
Set-Location -Path "C:\cpp-vulkan-hip-engine\vcpkg"
.\bootstrap-vcpkg.bat
Set-Location -Path "C:\cpp-vulkan-hip-engine" 
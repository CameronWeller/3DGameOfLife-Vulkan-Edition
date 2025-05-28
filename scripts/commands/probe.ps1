# Probe command for C++ Vulkan/HIP Engine
# Discovers and saves system information, paths, and environment details

param(
    [switch]$Verbose,
    [switch]$Force,
    [switch]$ShowOnly
)

$ErrorActionPreference = "Stop"

# Get script directory and project root
$scriptDir = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$projectRoot = Split-Path -Parent $scriptDir

# Import common functions
. "$scriptDir\common\functions.ps1"

Write-InfoOutput "Probing system environment..."

# Define the environment cache file
$envCacheDir = Join-Path $scriptDir "cache"
$envCacheFile = Join-Path $envCacheDir "environment.json"

# Create cache directory if it doesn't exist
if (-not (Test-Path $envCacheDir)) {
    New-Item -ItemType Directory -Path $envCacheDir -Force | Out-Null
}

# Check if we should use cached data
if ((Test-Path $envCacheFile) -and -not $Force) {
    $cacheAge = (Get-Date) - (Get-Item $envCacheFile).LastWriteTime
    if ($cacheAge.TotalHours -lt 24) {
        Write-InfoOutput "Using cached environment data (age: $($cacheAge.TotalHours.ToString('F1')) hours)"
        Write-InfoOutput "Use -Force to refresh the cache"
        
        if ($ShowOnly) {
            $cachedData = Get-Content $envCacheFile -Raw | ConvertFrom-Json
            Write-Host ""
            Write-Host "Cached Environment Information:" -ForegroundColor Cyan
            Write-Host "===============================" -ForegroundColor Cyan
            $cachedData | ConvertTo-Json -Depth 10 | Write-Host
        }
        return
    }
}

# Initialize environment data structure
$envData = @{
    Timestamp = (Get-Date).ToString("yyyy-MM-dd HH:mm:ss")
    System = @{}
    PowerShell = @{}
    VisualStudio = @{}
    SDKs = @{}
    Tools = @{}
    Paths = @{}
    ProjectPaths = @{}
}

# Probe system information
Write-VerboseOutput "Gathering system information..."
$envData.System = @{
    OS = [System.Environment]::OSVersion.VersionString
    Platform = [System.Environment]::OSVersion.Platform
    Version = [System.Environment]::OSVersion.Version.ToString()
    Architecture = [System.Environment]::Is64BitOperatingSystem ? "x64" : "x86"
    ProcessorCount = [System.Environment]::ProcessorCount
    MachineName = [System.Environment]::MachineName
    UserName = [System.Environment]::UserName
    UserDomainName = [System.Environment]::UserDomainName
    SystemDirectory = [System.Environment]::SystemDirectory
    TempPath = [System.IO.Path]::GetTempPath()
}

# Probe PowerShell information
Write-VerboseOutput "Gathering PowerShell information..."
$envData.PowerShell = @{
    Version = $PSVersionTable.PSVersion.ToString()
    Edition = $PSVersionTable.PSEdition
    ExecutionPolicy = (Get-ExecutionPolicy).ToString()
    Host = $Host.Name
    HostVersion = $Host.Version.ToString()
    CurrentDirectory = (Get-Location).Path
}

# Probe Visual Studio installation
Write-VerboseOutput "Searching for Visual Studio installations..."
$vswhereExe = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (Test-Path $vswhereExe) {
    $vsInstalls = & $vswhereExe -all -format json | ConvertFrom-Json
    $envData.VisualStudio = @{
        Installations = @()
        PreferredVersion = $null
        VsWherePath = $vswhereExe
    }
    
    foreach ($vs in $vsInstalls) {
        $vsInfo = @{
            DisplayName = $vs.displayName
            Version = $vs.installationVersion
            Path = $vs.installationPath
            ProductId = $vs.productId
            IsPrerelease = $vs.isPrerelease
            HasCppTools = $false
        }
        
        # Check for C++ tools
        $vcToolsPath = Join-Path $vs.installationPath "VC\Tools\MSVC"
        if (Test-Path $vcToolsPath) {
            $vsInfo.HasCppTools = $true
            $vsInfo.VCToolsPath = $vcToolsPath
            $vsInfo.VCToolsVersions = (Get-ChildItem $vcToolsPath -Directory).Name
        }
        
        $envData.VisualStudio.Installations += $vsInfo
        
        # Set preferred version (latest non-prerelease with C++ tools)
        if ($vsInfo.HasCppTools -and -not $vsInfo.IsPrerelease -and -not $envData.VisualStudio.PreferredVersion) {
            $envData.VisualStudio.PreferredVersion = $vsInfo
        }
    }
}

# Probe SDKs
Write-VerboseOutput "Searching for SDKs..."

# Vulkan SDK
$vulkanSdk = $env:VULKAN_SDK
if ($vulkanSdk -and (Test-Path $vulkanSdk)) {
    $envData.SDKs.Vulkan = @{
        Path = $vulkanSdk
        Version = $null
        IncludePath = Join-Path $vulkanSdk "Include"
        LibPath = Join-Path $vulkanSdk "Lib"
        BinPath = Join-Path $vulkanSdk "Bin"
    }
    
    # Try to get version from include files
    $vulkanHeaderPath = Join-Path $vulkanSdk "Include\vulkan\vulkan_core.h"
    if (Test-Path $vulkanHeaderPath) {
        $content = Get-Content $vulkanHeaderPath -TotalCount 100
        $versionMatch = $content | Select-String -Pattern "VK_HEADER_VERSION\s+(\d+)"
        if ($versionMatch) {
            $envData.SDKs.Vulkan.Version = $versionMatch.Matches[0].Groups[1].Value
        }
    }
}

# HIP SDK
$hipPath = $env:HIP_PATH
if ($hipPath -and (Test-Path $hipPath)) {
    $envData.SDKs.HIP = @{
        Path = $hipPath
        Version = $null
        IncludePath = Join-Path $hipPath "include"
        LibPath = Join-Path $hipPath "lib"
        BinPath = Join-Path $hipPath "bin"
    }
    
    # Try to get version
    $hipVersionFile = Join-Path $hipPath "bin\.hipVersion"
    if (Test-Path $hipVersionFile) {
        $envData.SDKs.HIP.Version = (Get-Content $hipVersionFile -First 1).Trim()
    }
}

# Windows SDK
$windowsSdkPath = "${env:ProgramFiles(x86)}\Windows Kits\10"
if (Test-Path $windowsSdkPath) {
    $envData.SDKs.WindowsSDK = @{
        Path = $windowsSdkPath
        Versions = @()
        IncludePath = Join-Path $windowsSdkPath "Include"
        LibPath = Join-Path $windowsSdkPath "Lib"
    }
    
    $includeVersions = Get-ChildItem (Join-Path $windowsSdkPath "Include") -Directory -ErrorAction SilentlyContinue
    foreach ($ver in $includeVersions) {
        if ($ver.Name -match "^\d+\.\d+\.\d+\.\d+$") {
            $envData.SDKs.WindowsSDK.Versions += $ver.Name
        }
    }
}

# Probe tools
Write-VerboseOutput "Searching for development tools..."
$toolsToProbe = @(
    @{Name = "git"; Command = "git"; VersionArg = "--version"; VersionPattern = "(\d+\.\d+\.\d+)"},
    @{Name = "cmake"; Command = "cmake"; VersionArg = "--version"; VersionPattern = "(\d+\.\d+\.\d+)"},
    @{Name = "ninja"; Command = "ninja"; VersionArg = "--version"; VersionPattern = "(\d+\.\d+\.\d+)"},
    @{Name = "vcpkg"; Command = "vcpkg"; VersionArg = "version"; VersionPattern = "(\d+\.\d+\.\d+)"},
    @{Name = "clang-format"; Command = "clang-format"; VersionArg = "--version"; VersionPattern = "(\d+\.\d+\.\d+)"},
    @{Name = "clang-tidy"; Command = "clang-tidy"; VersionArg = "--version"; VersionPattern = "(\d+\.\d+\.\d+)"},
    @{Name = "cppcheck"; Command = "cppcheck"; VersionArg = "--version"; VersionPattern = "(\d+\.\d+)"},
    @{Name = "docker"; Command = "docker"; VersionArg = "--version"; VersionPattern = "(\d+\.\d+\.\d+)"},
    @{Name = "python"; Command = "python"; VersionArg = "--version"; VersionPattern = "(\d+\.\d+\.\d+)"}
)

foreach ($tool in $toolsToProbe) {
    $cmd = Get-Command $tool.Command -ErrorAction SilentlyContinue
    if ($cmd) {
        $toolInfo = @{
            Found = $true
            Path = $cmd.Source
            Version = $null
        }
        
        # Try to get version
        try {
            $versionOutput = & $tool.Command $tool.VersionArg 2>&1
            if ($versionOutput -match $tool.VersionPattern) {
                $toolInfo.Version = $matches[1]
            }
        } catch {
            Write-VerboseOutput "Failed to get version for $($tool.Name)"
        }
        
        $envData.Tools[$tool.Name] = $toolInfo
    } else {
        $envData.Tools[$tool.Name] = @{
            Found = $false
            Path = $null
            Version = $null
        }
    }
}

# Probe important paths
Write-VerboseOutput "Gathering important paths..."
$envData.Paths = @{
    ProgramFiles = ${env:ProgramFiles}
    ProgramFilesX86 = ${env:ProgramFiles(x86)}
    LocalAppData = $env:LOCALAPPDATA
    AppData = $env:APPDATA
    UserProfile = $env:USERPROFILE
    Path = $env:Path -split ';'
}

# Probe project-specific paths
Write-VerboseOutput "Gathering project paths..."
$envData.ProjectPaths = @{
    Root = $projectRoot
    Scripts = $scriptDir
    Build = Join-Path $projectRoot "build"
    Source = Join-Path $projectRoot "src"
    Shaders = Join-Path $projectRoot "shaders"
    Tests = Join-Path $projectRoot "tests"
    Docs = Join-Path $projectRoot "docs"
    Tools = Join-Path $projectRoot "tools"
    VcpkgRoot = Join-Path $projectRoot "vcpkg"
    VcpkgInstalled = Join-Path $projectRoot "vcpkg_installed"
}

# Special handling for vcpkg
if (Test-Path $envData.ProjectPaths.VcpkgRoot) {
    $vcpkgExe = Join-Path $envData.ProjectPaths.VcpkgRoot "vcpkg.exe"
    if (Test-Path $vcpkgExe) {
        $envData.ProjectPaths.VcpkgExecutable = $vcpkgExe
        
        # Get installed packages
        try {
            $installedPackages = & $vcpkgExe list 2>&1
            if ($LASTEXITCODE -eq 0) {
                $envData.ProjectPaths.VcpkgPackages = @($installedPackages | ForEach-Object { $_.Split(':')[0] })
            }
        } catch {
            Write-VerboseOutput "Failed to get vcpkg packages"
        }
    }
}

# Save to cache file
Write-VerboseOutput "Saving environment data to cache..."
$envData | ConvertTo-Json -Depth 10 | Set-Content -Path $envCacheFile -Encoding UTF8

# Display summary
Write-Host ""
Write-Host "System Environment Summary" -ForegroundColor Cyan
Write-Host "=========================" -ForegroundColor Cyan
Write-Host ""

# System info
Write-Host "System:" -ForegroundColor Yellow
Write-Host "  OS: $($envData.System.OS)"
Write-Host "  Architecture: $($envData.System.Architecture)"
Write-Host "  Processors: $($envData.System.ProcessorCount)"
Write-Host ""

# PowerShell info
Write-Host "PowerShell:" -ForegroundColor Yellow
Write-Host "  Version: $($envData.PowerShell.Version)"
Write-Host "  Edition: $($envData.PowerShell.Edition)"
Write-Host ""

# Visual Studio info
if ($envData.VisualStudio.PreferredVersion) {
    Write-Host "Visual Studio:" -ForegroundColor Yellow
    Write-Host "  Version: $($envData.VisualStudio.PreferredVersion.DisplayName)"
    Write-Host "  Path: $($envData.VisualStudio.PreferredVersion.Path)"
    Write-Host "  C++ Tools: $($envData.VisualStudio.PreferredVersion.HasCppTools)"
} else {
    Write-WarningOutput "Visual Studio: Not found"
}
Write-Host ""

# SDKs
Write-Host "SDKs:" -ForegroundColor Yellow
foreach ($sdk in $envData.SDKs.Keys) {
    if ($envData.SDKs[$sdk]) {
        Write-Host "  $sdk : Found at $($envData.SDKs[$sdk].Path)"
    } else {
        Write-Host "  $sdk : Not found" -ForegroundColor Red
    }
}
Write-Host ""

# Tools
Write-Host "Development Tools:" -ForegroundColor Yellow
foreach ($toolName in $envData.Tools.Keys) {
    $tool = $envData.Tools[$toolName]
    if ($tool.Found) {
        $version = if ($tool.Version) { "v$($tool.Version)" } else { "version unknown" }
        Write-Host "  $toolName : Found ($version)" -ForegroundColor Green
    } else {
        Write-Host "  $toolName : Not found" -ForegroundColor Red
    }
}

Write-Host ""
Write-SuccessOutput "Environment probe completed!"
Write-InfoOutput "Cache saved to: $envCacheFile"

if ($ShowOnly) {
    Write-Host ""
    Write-Host "Full Environment Data:" -ForegroundColor Cyan
    Write-Host "=====================" -ForegroundColor Cyan
    $envData | ConvertTo-Json -Depth 10 | Write-Host
} 
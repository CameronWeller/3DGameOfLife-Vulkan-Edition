# Common functions for C++ Vulkan/HIP Engine scripts

# Function to write verbose output
function Write-VerboseOutput {
    param([string]$Message)
    if ($Verbose) {
        Write-Host "[VERBOSE] $Message" -ForegroundColor DarkGray
    }
}

# Function to write info output
function Write-InfoOutput {
    param([string]$Message)
    Write-Host "[INFO] $Message" -ForegroundColor Cyan
}

# Function to write success output
function Write-SuccessOutput {
    param([string]$Message)
    Write-Host "[SUCCESS] $Message" -ForegroundColor Green
}

# Function to write warning output
function Write-WarningOutput {
    param([string]$Message)
    Write-Host "[WARNING] $Message" -ForegroundColor Yellow
}

# Function to write error output
function Write-ErrorOutput {
    param([string]$Message)
    Write-Host "[ERROR] $Message" -ForegroundColor Red
}

# Function to check if a command exists
function Test-Command {
    param([string]$CommandName)
    return [bool](Get-Command -Name $CommandName -ErrorAction SilentlyContinue)
}

# Function to get command version
function Get-CommandVersion {
    param([string]$CommandName)
    $cmd = Get-Command $CommandName -ErrorAction SilentlyContinue
    if ($cmd) {
        if ($cmd.Version) {
            return $cmd.Version
        }
        # Try to get version from executable
        try {
            $output = & $CommandName --version 2>&1
            if ($output -match '(\d+\.\d+\.\d+)') {
                return [Version]$matches[1]
            }
        }
        catch {
            return $null
        }
    }
    return $null
}

# Function to add to PATH if not exists
function Add-ToPath {
    param([string]$Path)
    if (-not $env:Path.Contains($Path)) {
        $env:Path = "$Path;$env:Path"
        Write-VerboseOutput "Added to PATH: $Path"
    }
}

# Function to clean up temporary files
function Remove-TempFiles {
    param([string[]]$TempFiles)
    foreach ($file in $TempFiles) {
        if (Test-Path $file) {
            Remove-Item $file -Force -ErrorAction SilentlyContinue
            Write-VerboseOutput "Removed temp file: $file"
        }
    }
}

# Function to find Visual Studio installation
function Find-VisualStudio {
    # Try to get from cache first
    $cachedPath = Get-CachedVisualStudioPath
    if ($cachedPath -and (Test-Path $cachedPath)) {
        Write-VerboseOutput "Using cached Visual Studio path: $cachedPath"
        return $cachedPath
    }
    
    # If not in cache or path doesn't exist, search for it
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere)) {
        Write-ErrorOutput "vswhere not found. Please install Visual Studio 2022 with C++ development tools."
        return $null
    }

    # Try to find Visual Studio installation
    $vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $vsPath) {
        # Try to find BuildTools
        $vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath -prerelease
    }

    if (-not $vsPath) {
        Write-ErrorOutput "Visual Studio with C++ tools not found. Please install Visual Studio 2022 with C++ development tools or Build Tools."
        return $null
    }

    # Check Visual Studio version
    $vsVersion = & $vswhere -latest -products * -property catalog_productLineVersion
    if ($vsVersion -lt "2022") {
        Write-ErrorOutput "Visual Studio 2022 or later is required. Found version: $vsVersion"
        return $null
    }

    Write-VerboseOutput "Found Visual Studio at: $vsPath"
    
    # Update cache with the found path
    Write-VerboseOutput "Updating environment cache..."
    & "$scriptDir\commands\probe.ps1" -Force
    
    return $vsPath
}

# Function to setup Visual Studio environment
function Initialize-VisualStudioEnvironment {
    param([string]$VsPath)

    $vcvarsPath = Join-Path $VsPath "VC\Auxiliary\Build\vcvarsall.bat"
    if (-not (Test-Path $vcvarsPath)) {
        # Try alternative path for BuildTools
        $vcvarsPath = Join-Path "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\BuildTools" "VC\Auxiliary\Build\vcvarsall.bat"
        if (-not (Test-Path $vcvarsPath)) {
            Write-ErrorOutput "Could not find vcvarsall.bat. Please ensure Visual Studio or Build Tools is properly installed."
            return $false
        }
    }

    Write-VerboseOutput "Setting up Visual Studio environment..."
    Write-VerboseOutput "Using vcvarsall.bat at: $vcvarsPath"

    # Set up environment variables
    $env:VSINSTALLDIR = $VsPath
    $env:VCINSTALLDIR = Join-Path $VsPath "VC"
    $env:VCPKG_VISUAL_STUDIO_PATH = $VsPath

    # Set up Visual Studio environment using PowerShell
    $vsDevCmd = Join-Path $VsPath "Common7\Tools\Launch-VsDevShell.ps1"
    $tempFile = [System.IO.Path]::GetTempFileName()
    try {
        if (Test-Path $vsDevCmd) {
            Write-VerboseOutput "Using Visual Studio Developer Shell..."
            & $vsDevCmd -Arch amd64 -HostArch amd64 -SkipAutomaticLocation
            if ($LASTEXITCODE -ne 0) {
                throw "Failed to initialize Visual Studio Developer Shell"
            }
        } else {
            Write-VerboseOutput "Using vcvarsall.bat..."
            # Fallback to using vcvarsall.bat
            $cmdLine = "`"$vcvarsPath`" x64 > `"$tempFile`" 2>&1"
            $process = Start-Process -FilePath "cmd.exe" -ArgumentList "/c $cmdLine" -NoNewWindow -Wait -PassThru
            if ($process.ExitCode -ne 0) {
                $errorOutput = Get-Content $tempFile
                throw "Failed to set up Visual Studio environment: $errorOutput"
            }
        }
    }
    catch {
        Write-ErrorOutput $_.Exception.Message
        return $false
    }
    finally {
        Remove-TempFiles @($tempFile)
    }

    return $true
}

# Function to check Vulkan SDK
function Test-VulkanSdk {
    # Try to get from cache first
    $cachedSdk = Get-CachedSdkPath -SdkName "Vulkan"
    if ($cachedSdk -and (Test-Path $cachedSdk)) {
        Write-VerboseOutput "Using cached Vulkan SDK path: $cachedSdk"
        return $true
    }
    
    # Check environment variable
    $vulkanSdk = $env:VULKAN_SDK
    if (-not $vulkanSdk) {
        Write-ErrorOutput "VULKAN_SDK environment variable not set. Please install Vulkan SDK."
        return $false
    }

    $vulkanInclude = Join-Path $vulkanSdk "Include\vulkan"
    if (-not (Test-Path $vulkanInclude)) {
        Write-ErrorOutput "Vulkan SDK installation appears to be incomplete. Missing: $vulkanInclude"
        return $false
    }

    Write-VerboseOutput "Vulkan SDK found at: $vulkanSdk"
    return $true
}

# Function to check HIP SDK
function Test-HipSdk {
    # Try to get from cache first
    $cachedSdk = Get-CachedSdkPath -SdkName "HIP"
    if ($cachedSdk -and (Test-Path $cachedSdk)) {
        Write-VerboseOutput "Using cached HIP SDK path: $cachedSdk"
        return $true
    }
    
    # Check environment variable
    $hipSdk = $env:HIP_PATH
    if (-not $hipSdk) {
        Write-ErrorOutput "HIP_PATH environment variable not set. Please install HIP SDK."
        return $false
    }

    $hipInclude = Join-Path $hipSdk "include"
    if (-not (Test-Path $hipInclude)) {
        Write-ErrorOutput "HIP SDK installation appears to be incomplete. Missing: $hipInclude"
        return $false
    }

    Write-VerboseOutput "HIP SDK found at: $hipSdk"
    return $true
}

# Function to setup vcpkg
function Initialize-Vcpkg {
    param([string]$ProjectRoot)
    
    $vcpkgRoot = Join-Path $ProjectRoot "vcpkg"
    if (-not (Test-Path $vcpkgRoot)) {
        Write-ErrorOutput "vcpkg directory not found at: $vcpkgRoot"
        Write-InfoOutput "Please ensure vcpkg is cloned in the project root directory."
        return $null
    }

    # Set up vcpkg environment
    $env:VCPKG_ROOT = $vcpkgRoot

    # Ensure vcpkg is bootstrapped
    $vcpkgExe = Join-Path $vcpkgRoot "vcpkg.exe"
    if (-not (Test-Path $vcpkgExe)) {
        Write-InfoOutput "Bootstrapping vcpkg..."
        Push-Location $vcpkgRoot
        try {
            & .\bootstrap-vcpkg.bat
            if ($LASTEXITCODE -ne 0) {
                throw "Failed to bootstrap vcpkg"
            }
            
            # Verify vcpkg executable was created
            if (-not (Test-Path $vcpkgExe)) {
                throw "vcpkg.exe was not created after bootstrapping"
            }
        }
        catch {
            Write-ErrorOutput $_.Exception.Message
            Pop-Location
            return $null
        }
        finally {
            Pop-Location
        }
    }

    Write-VerboseOutput "vcpkg initialized at: $vcpkgRoot"
    return $vcpkgRoot
}

# Function to validate vcpkg manifest
function Test-VcpkgManifest {
    param([string]$ManifestPath)
    
    if (-not (Test-Path $ManifestPath)) {
        Write-ErrorOutput "vcpkg.json manifest file not found at: $ManifestPath"
        return $false
    }
    
    try {
        $manifest = Get-Content $ManifestPath -Raw | ConvertFrom-Json
        if (-not $manifest.name) {
            Write-ErrorOutput "vcpkg.json is missing required 'name' field"
            return $false
        }
        if (-not $manifest.version) {
            Write-ErrorOutput "vcpkg.json is missing required 'version' field"
            return $false
        }
        if (-not $manifest.dependencies) {
            Write-ErrorOutput "vcpkg.json is missing required 'dependencies' field"
            return $false
        }
        Write-VerboseOutput "vcpkg manifest validated successfully"
        return $true
    }
    catch {
        Write-ErrorOutput "Failed to parse vcpkg.json: $($_.Exception.Message)"
        return $false
    }
}

# Function to download and setup Ninja
function Initialize-Ninja {
    param([string]$ProjectRoot)
    
    $ninjaVersion = "1.12.1"
    $ninjaUrl = "https://github.com/ninja-build/ninja/releases/download/v$ninjaVersion/ninja-win.zip"
    $ninjaDir = Join-Path $ProjectRoot "tools\ninja"
    $ninjaExe = Join-Path $ninjaDir "ninja.exe"
    
    if (-not (Test-Path $ninjaExe)) {
        Write-InfoOutput "Downloading Ninja..."
        if (-not (Test-Path $ninjaDir)) {
            New-Item -ItemType Directory -Path $ninjaDir -Force | Out-Null
        }
        
        $tempZip = Join-Path $env:TEMP "ninja.zip"
        try {
            $webClient = New-Object System.Net.WebClient
            $webClient.DownloadFile($ninjaUrl, $tempZip)
            
            Write-VerboseOutput "Extracting Ninja..."
            Expand-Archive -Path $tempZip -DestinationPath $ninjaDir -Force
            
            # Add ninja to PATH for this session
            Add-ToPath $ninjaDir
        }
        catch {
            Write-ErrorOutput "Failed to download or extract Ninja: $($_.Exception.Message)"
            return $null
        }
        finally {
            Remove-TempFiles @($tempZip)
        }
    }
    
    Write-VerboseOutput "Ninja initialized at: $ninjaExe"
    return $ninjaExe
}

# Function to check required tools
function Test-RequiredTools {
    param([hashtable]$RequiredTools)
    
    $missingTools = @()
    $outdatedTools = @()

    foreach ($tool in $RequiredTools.Keys) {
        if (-not (Test-Command $tool)) {
            $missingTools += $tool
        } else {
            $version = Get-CommandVersion $tool
            if ($version -and $version -lt [Version]$RequiredTools[$tool]) {
                $outdatedTools += "$tool (found: $version, required: $($RequiredTools[$tool]))"
            }
        }
    }

    if ($missingTools.Count -gt 0) {
        Write-ErrorOutput "Missing required tools: $($missingTools -join ', ')"
        return $false
    }

    if ($outdatedTools.Count -gt 0) {
        Write-ErrorOutput "Outdated tools: $($outdatedTools -join ', ')"
        return $false
    }

    Write-VerboseOutput "All required tools are present and up to date"
    return $true
}

# Function to get cached environment data
function Get-CachedEnvironment {
    param(
        [switch]$Force,
        [int]$MaxAgeHours = 24
    )
    
    $scriptDir = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
    $envCacheFile = Join-Path $scriptDir "cache\environment.json"
    
    # Check if cache exists
    if (-not (Test-Path $envCacheFile)) {
        Write-VerboseOutput "Environment cache not found. Running probe..."
        & "$scriptDir\commands\probe.ps1"
        if (-not (Test-Path $envCacheFile)) {
            Write-ErrorOutput "Failed to create environment cache"
            return $null
        }
    }
    
    # Check cache age
    if (-not $Force) {
        $cacheAge = (Get-Date) - (Get-Item $envCacheFile).LastWriteTime
        if ($cacheAge.TotalHours -gt $MaxAgeHours) {
            Write-VerboseOutput "Environment cache is stale (age: $($cacheAge.TotalHours.ToString('F1')) hours). Refreshing..."
            & "$scriptDir\commands\probe.ps1" -Force
        }
    }
    
    # Load and return cache
    try {
        $envData = Get-Content $envCacheFile -Raw | ConvertFrom-Json
        return $envData
    }
    catch {
        Write-ErrorOutput "Failed to load environment cache: $($_.Exception.Message)"
        return $null
    }
}

# Function to get Visual Studio path from cache
function Get-CachedVisualStudioPath {
    $envData = Get-CachedEnvironment
    if ($envData -and $envData.VisualStudio -and $envData.VisualStudio.PreferredVersion) {
        return $envData.VisualStudio.PreferredVersion.Path
    }
    return $null
}

# Function to get tool path from cache
function Get-CachedToolPath {
    param([string]$ToolName)
    
    $envData = Get-CachedEnvironment
    if ($envData -and $envData.Tools -and $envData.Tools.$ToolName -and $envData.Tools.$ToolName.Found) {
        return $envData.Tools.$ToolName.Path
    }
    return $null
}

# Function to get SDK path from cache
function Get-CachedSdkPath {
    param([string]$SdkName)
    
    $envData = Get-CachedEnvironment
    if ($envData -and $envData.SDKs -and $envData.SDKs.$SdkName) {
        return $envData.SDKs.$SdkName.Path
    }
    return $null
}

# Function to get project path from cache
function Get-CachedProjectPath {
    param([string]$PathName)
    
    $envData = Get-CachedEnvironment
    if ($envData -and $envData.ProjectPaths -and $envData.ProjectPaths.$PathName) {
        return $envData.ProjectPaths.$PathName
    }
    return $null
}

# Function to check if environment is properly configured
function Test-Environment {
    $envData = Get-CachedEnvironment
    if (-not $envData) {
        return $false
    }
    
    $issues = @()
    
    # Check Visual Studio
    if (-not $envData.VisualStudio.PreferredVersion) {
        $issues += "Visual Studio with C++ tools not found"
    }
    
    # Check required SDKs
    if (-not $envData.SDKs.Vulkan) {
        $issues += "Vulkan SDK not found"
    }
    
    # Check required tools
    $requiredTools = @("git", "cmake")
    foreach ($tool in $requiredTools) {
        if (-not $envData.Tools.$tool.Found) {
            $issues += "$tool not found"
        }
    }
    
    if ($issues.Count -gt 0) {
        Write-ErrorOutput "Environment issues detected:"
        foreach ($issue in $issues) {
            Write-ErrorOutput "  - $issue"
        }
        return $false
    }
    
    return $true
}

# Export functions
Export-ModuleMember -Function * 
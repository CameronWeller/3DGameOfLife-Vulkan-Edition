# Pester test file for install_dependencies.ps1
$scriptPath = Join-Path (Split-Path -Parent $PSScriptRoot) "install_dependencies.ps1"

# Function to check if a command exists
function Test-CommandExists {
    param ($command)
    $oldPreference = $ErrorActionPreference
    $ErrorActionPreference = 'stop'
    try { if (Get-Command $command) { return $true } }
    catch { return $false }
    finally { $ErrorActionPreference = $oldPreference }
}

Describe "Install Dependencies Script Tests" {
    Context "Environment Validation" {
        It "Should require PowerShell 5.0 or higher" {
            $psVersion = $PSVersionTable.PSVersion.Major
            $psVersion | Should BeGreaterThan 4
        }

        It "Should require administrator privileges" {
            $isAdmin = ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
            $isAdmin | Should Be $true
        }

        It "Should have required commands available" {
            $requiredCommands = @('git', 'cmake')
            foreach ($cmd in $requiredCommands) {
                Test-CommandExists $cmd | Should Be $true
            }
        }
    }

    Context "Vcpkg Integration" {
        BeforeEach {
            $projectRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
            $vcpkgRoot = Join-Path $projectRoot "vcpkg"
            $buildDir = Join-Path $projectRoot "build"
            
            # Create mock vcpkg directory
            if (-not (Test-Path $vcpkgRoot)) {
                New-Item -ItemType Directory -Force -Path $vcpkgRoot | Out-Null
                New-Item -ItemType File -Force -Path (Join-Path $vcpkgRoot "vcpkg.exe") | Out-Null
            }
            
            # Clean build directory
            if (Test-Path $buildDir) {
                Remove-Item -Recurse -Force $buildDir
            }
        }

        AfterEach {
            $projectRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
            $vcpkgRoot = Join-Path $projectRoot "vcpkg"
            $buildDir = Join-Path $projectRoot "build"
            
            # Cleanup
            if (Test-Path $vcpkgRoot) {
                Remove-Item -Recurse -Force $vcpkgRoot
            }
            if (Test-Path $buildDir) {
                Remove-Item -Recurse -Force $buildDir
            }
        }

        It "Should set VCPKG_ROOT environment variable" {
            $projectRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
            $vcpkgRoot = Join-Path $projectRoot "vcpkg"
            
            & $scriptPath
            
            [Environment]::GetEnvironmentVariable("VCPKG_ROOT", [EnvironmentVariableTarget]::Machine) | Should Be $vcpkgRoot
        }

        It "Should create build directory" {
            $projectRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
            $buildDir = Join-Path $projectRoot "build"
            
            & $scriptPath
            
            Test-Path $buildDir | Should Be $true
        }
    }

    Context "Error Handling" {
        It "Should fail if vcpkg directory is not found" {
            $projectRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
            $vcpkgRoot = Join-Path $projectRoot "vcpkg"
            
            if (Test-Path $vcpkgRoot) {
                Remove-Item -Recurse -Force $vcpkgRoot
            }
            
            { & $scriptPath } | Should Throw
        }
    }
} 
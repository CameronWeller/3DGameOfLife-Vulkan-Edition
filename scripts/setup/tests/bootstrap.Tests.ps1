# Pester test file for bootstrap.ps1
$scriptPath = Join-Path (Split-Path -Parent $PSScriptRoot) "bootstrap.ps1"

# Function to check if a command exists
function Test-CommandExists {
    param ($command)
    $oldPreference = $ErrorActionPreference
    $ErrorActionPreference = 'stop'
    try { if (Get-Command $command) { return $true } }
    catch { return $false }
    finally { $ErrorActionPreference = $oldPreference }
}

Describe "Bootstrap Script Tests" {
    Context "Parameter Validation" {
        It "Should accept -disableMetrics switch" {
            $params = @{
                disableMetrics = $true
            }
            { & $scriptPath @params } | Should Not Throw
        }
    }

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

    Context "Vcpkg Root Directory" {
        BeforeEach {
            $projectRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
            $vcpkgRootDir = Join-Path $projectRoot "vcpkg"
            if (Test-Path $vcpkgRootDir) {
                Remove-Item -Recurse -Force $vcpkgRootDir
            }
        }

        AfterEach {
            $projectRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
            $vcpkgRootDir = Join-Path $projectRoot "vcpkg"
            if (Test-Path $vcpkgRootDir) {
                Remove-Item -Recurse -Force $vcpkgRootDir
            }
        }

        It "Should create vcpkg directory if it doesn't exist" {
            $projectRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
            $vcpkgRootDir = Join-Path $projectRoot "vcpkg"
            
            & $scriptPath
            
            Test-Path $vcpkgRootDir | Should Be $true
        }

        It "Should clone vcpkg repository if not present" {
            $projectRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
            $vcpkgRootDir = Join-Path $projectRoot "vcpkg"
            
            & $scriptPath
            
            Test-Path (Join-Path $vcpkgRootDir ".git") | Should Be $true
        }
    }

    Context "Metrics Configuration" {
        It "Should create disable-metrics file when -disableMetrics is specified" {
            $projectRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
            $vcpkgRootDir = Join-Path $projectRoot "vcpkg"
            
            $params = @{
                disableMetrics = $true
            }
            & $scriptPath @params
            
            Test-Path (Join-Path $vcpkgRootDir "vcpkg.disable-metrics") | Should Be $true
        }
    }
} 
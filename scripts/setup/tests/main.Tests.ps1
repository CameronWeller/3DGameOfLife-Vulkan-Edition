# Pester test file for main.ps1
$scriptPath = Join-Path (Split-Path -Parent $PSScriptRoot) "main.ps1"

# Function to check if a command exists
function Test-CommandExists {
    param ($command)
    $oldPreference = $ErrorActionPreference
    $ErrorActionPreference = 'stop'
    try { if (Get-Command $command) { return $true } }
    catch { return $false }
    finally { $ErrorActionPreference = $oldPreference }
}

Describe "Main Setup Script Tests" {
    Context "Parameter Validation" {
        It "Should accept -Admin switch" {
            $params = @{
                Admin = $true
            }
            { & $scriptPath @params } | Should Not Throw
        }

        It "Should accept -Clean switch" {
            $params = @{
                Clean = $true
            }
            { & $scriptPath @params } | Should Not Throw
        }

        It "Should accept -Help switch" {
            $params = @{
                Help = $true
            }
            { & $scriptPath @params } | Should Not Throw
        }
    }

    Context "Admin Privilege Check" {
        It "Should detect admin privileges correctly" {
            $isAdmin = ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
            $isAdmin | Should BeOfType [bool]
        }
    }

    Context "Clean Functionality" {
        BeforeEach {
            # Create test directories
            New-Item -ItemType Directory -Force -Path "build" | Out-Null
            New-Item -ItemType Directory -Force -Path "vcpkg_installed" | Out-Null
        }

        AfterEach {
            # Cleanup test directories
            Remove-Item -Recurse -Force "build" -ErrorAction SilentlyContinue
            Remove-Item -Recurse -Force "vcpkg_installed" -ErrorAction SilentlyContinue
        }

        It "Should remove build directories when -Clean is specified" {
            $params = @{
                Clean = $true
            }
            & $scriptPath @params
            
            Test-Path "build" | Should Be $false
            Test-Path "vcpkg_installed" | Should Be $false
        }
    }
} 
# Test runner script for setup tests
param(
    [switch]$Verbose,
    [string]$TestPath = $PSScriptRoot
)

# Ensure Pester is installed
if (-not (Get-Module -ListAvailable -Name Pester)) {
    Write-Host "Installing Pester module..."
    Install-Module -Name Pester -Force -SkipPublisherCheck
}

# Import Pester
Import-Module Pester

# Run the tests with appropriate parameters
$pesterParams = @{
    Path = $TestPath
    OutputFile = Join-Path $TestPath "TestResults.xml"
    OutputFormat = "NUnitXml"
}

if ($Verbose) {
    $pesterParams.Add("Verbose", $true)
}

Invoke-Pester @pesterParams 
# Test command for C++ Vulkan/HIP Engine

param(
    [Parameter()]
    [ValidateSet("Debug", "Release")]
    [string]$BuildConfig = "Debug",
    
    [switch]$Verbose,
    [string]$Filter = ""
)

$ErrorActionPreference = "Stop"

# Get script directory and project root
$scriptDir = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$projectRoot = Split-Path -Parent $scriptDir

# Import common functions
. "$scriptDir\common\functions.ps1"

Write-InfoOutput "Running tests..."

# Check if build directory exists
$buildDir = Join-Path $projectRoot "build"
if (-not (Test-Path $buildDir)) {
    Write-ErrorOutput "Build directory not found. Please build the project first."
    Write-InfoOutput "Run: .\main.ps1 build"
    exit 1
}

# Find test executables
$testPattern = "*test*.exe"
$testExecutables = Get-ChildItem -Path $buildDir -Filter $testPattern -Recurse -ErrorAction SilentlyContinue

if ($testExecutables.Count -eq 0) {
    Write-WarningOutput "No test executables found in build directory."
    Write-InfoOutput "Looking for executables matching pattern: $testPattern"
    exit 0
}

Write-InfoOutput "Found $($testExecutables.Count) test executable(s)"

# Run each test executable
$totalTests = 0
$passedTests = 0
$failedTests = 0
$testResults = @()

foreach ($testExe in $testExecutables) {
    Write-Host ""
    Write-InfoOutput "Running: $($testExe.Name)"
    Write-Host "=" * 60 -ForegroundColor DarkGray
    
    $testArgs = @()
    
    # Add filter if specified
    if ($Filter) {
        $testArgs += "--gtest_filter=$Filter"
    }
    
    # Add verbose output if requested
    if ($Verbose) {
        $testArgs += "--gtest_print_time=1"
        $testArgs += "--gtest_print_utf8=1"
    }
    
    # Run the test
    $startTime = Get-Date
    try {
        $output = & $testExe.FullName $testArgs 2>&1
        $exitCode = $LASTEXITCODE
        $endTime = Get-Date
        $duration = $endTime - $startTime
        
        # Parse test output for results
        $testOutput = $output -join "`n"
        Write-Host $testOutput
        
        # Try to extract test counts from Google Test output
        if ($testOutput -match "\[=+\] (\d+) tests? from (\d+) test (?:cases?|suites?) ran") {
            $testsRun = [int]$matches[1]
            $totalTests += $testsRun
        }
        
        if ($testOutput -match "\[  PASSED  \] (\d+) tests?") {
            $passed = [int]$matches[1]
            $passedTests += $passed
        }
        
        if ($testOutput -match "\[  FAILED  \] (\d+) tests?") {
            $failed = [int]$matches[1]
            $failedTests += $failed
        }
        
        $testResults += @{
            Name = $testExe.Name
            ExitCode = $exitCode
            Duration = $duration
            Output = $testOutput
            Success = ($exitCode -eq 0)
        }
        
        if ($exitCode -eq 0) {
            Write-SuccessOutput "Test passed in $($duration.TotalSeconds.ToString('F2'))s"
        } else {
            Write-ErrorOutput "Test failed with exit code: $exitCode"
        }
    }
    catch {
        Write-ErrorOutput "Failed to run test: $($_.Exception.Message)"
        $testResults += @{
            Name = $testExe.Name
            ExitCode = -1
            Duration = (Get-Date) - $startTime
            Output = $_.Exception.Message
            Success = $false
        }
        $failedTests++
    }
}

# Display summary
Write-Host ""
Write-Host "Test Summary" -ForegroundColor Cyan
Write-Host "============" -ForegroundColor Cyan
Write-Host "Total test executables: $($testExecutables.Count)"
Write-Host "Total tests run: $totalTests"
Write-Host "Passed: $passedTests" -ForegroundColor Green
Write-Host "Failed: $failedTests" -ForegroundColor $(if ($failedTests -gt 0) { "Red" } else { "Green" })

Write-Host ""
Write-Host "Test Results:" -ForegroundColor Cyan
foreach ($result in $testResults) {
    $status = if ($result.Success) { "PASS" } else { "FAIL" }
    $color = if ($result.Success) { "Green" } else { "Red" }
    Write-Host "$($result.Name) : $status ($($result.Duration.TotalSeconds.ToString('F2'))s)" -ForegroundColor $color
}

# Check for code coverage if available
$coverageFile = Join-Path $buildDir "coverage.info"
if (Test-Path $coverageFile) {
    Write-Host ""
    Write-InfoOutput "Code coverage report available at: $coverageFile"
    
    # If lcov is available, generate HTML report
    if (Test-Command "lcov") {
        $coverageHtmlDir = Join-Path $buildDir "coverage_html"
        Write-VerboseOutput "Generating HTML coverage report..."
        & genhtml $coverageFile --output-directory $coverageHtmlDir 2>&1 | Out-Null
        if ($LASTEXITCODE -eq 0) {
            Write-InfoOutput "HTML coverage report generated at: $coverageHtmlDir\index.html"
        }
    }
}

# Exit with appropriate code
if ($failedTests -gt 0) {
    Write-ErrorOutput "$failedTests test(s) failed"
    exit 1
} else {
    Write-SuccessOutput "All tests passed!"
    exit 0
} 
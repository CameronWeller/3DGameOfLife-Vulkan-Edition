# Run all tests and generate reports
param(
    [switch]$UnitTests,
    [switch]$IntegrationTests,
    [switch]$Benchmarks,
    [switch]$PerformanceTests,
    [switch]$AllTests,
    [switch]$GenerateReports,
    [string]$BuildType = "Debug"
)

# Set error action preference
$ErrorActionPreference = "Stop"

# Function to create build directory if it doesn't exist
function Ensure-BuildDirectory {
    if (-not (Test-Path "build")) {
        New-Item -ItemType Directory -Path "build" | Out-Null
    }
}

# Function to configure CMake
function Configure-CMake {
    param(
        [string]$BuildType
    )
    
    Write-Host "Configuring CMake with build type: $BuildType"
    cmake -B build -S . -DCMAKE_BUILD_TYPE=$BuildType -DBUILD_TESTING=ON
    if ($LASTEXITCODE -ne 0) {
        throw "CMake configuration failed"
    }
}

# Function to build the project
function Build-Project {
    Write-Host "Building project..."
    cmake --build build --config $BuildType
    if ($LASTEXITCODE -ne 0) {
        throw "Build failed"
    }
}

# Function to run tests
function Run-Tests {
    param(
        [string]$TestType
    )
    
    Write-Host "Running $TestType tests..."
    ctest --test-dir build --output-on-failure -R "$TestType.*"
    if ($LASTEXITCODE -ne 0) {
        throw "$TestType tests failed"
    }
}

# Function to generate reports
function Generate-Reports {
    Write-Host "Generating test reports..."
    
    # Create reports directory if it doesn't exist
    if (-not (Test-Path "reports")) {
        New-Item -ItemType Directory -Path "reports" | Out-Null
    }
    
    # Generate coverage report if enabled
    if ($ENABLE_COVERAGE) {
        Write-Host "Generating coverage report..."
        # Add coverage report generation commands here
    }
    
    # Generate benchmark reports
    Write-Host "Generating benchmark reports..."
    # Add benchmark report generation commands here
}

# Main script execution
try {
    Ensure-BuildDirectory
    Configure-CMake -BuildType $BuildType
    Build-Project
    
    if ($AllTests -or $UnitTests) {
        Run-Tests -TestType "unit"
    }
    
    if ($AllTests -or $IntegrationTests) {
        Run-Tests -TestType "integration"
    }
    
    if ($AllTests -or $Benchmarks) {
        Run-Tests -TestType "benchmark"
    }
    
    if ($AllTests -or $PerformanceTests) {
        Run-Tests -TestType "performance"
    }
    
    if ($GenerateReports) {
        Generate-Reports
    }
    
    Write-Host "All operations completed successfully!"
}
catch {
    Write-Error "An error occurred: $_"
    exit 1
} 
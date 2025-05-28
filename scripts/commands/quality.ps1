# Quality command for C++ Vulkan/HIP Engine

param(
    [switch]$Verbose,
    [switch]$Fix
)

$ErrorActionPreference = "Stop"

# Get script directory and project root
$scriptDir = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$projectRoot = Split-Path -Parent $scriptDir

# Import common functions
. "$scriptDir\common\functions.ps1"

Write-InfoOutput "Running code quality checks..."

$hasIssues = $false

# Check for clang-format
if (Test-Command "clang-format") {
    Write-InfoOutput "Running clang-format..."
    
    # Find all C++ source files
    $sourceFiles = Get-ChildItem -Path $projectRoot -Include "*.cpp", "*.hpp", "*.h", "*.c" -Recurse | 
                   Where-Object { $_.FullName -notmatch "\\(build|vcpkg|vcpkg_installed|\.git)\\" }
    
    $formattingIssues = @()
    foreach ($file in $sourceFiles) {
        Write-VerboseOutput "Checking: $($file.FullName)"
        
        if ($Fix) {
            # Fix formatting
            & clang-format -i $file.FullName
        } else {
            # Check formatting
            $output = & clang-format --dry-run --Werror $file.FullName 2>&1
            if ($LASTEXITCODE -ne 0) {
                $formattingIssues += $file.FullName
            }
        }
    }
    
    if ($formattingIssues.Count -gt 0) {
        Write-WarningOutput "Formatting issues found in $($formattingIssues.Count) files:"
        foreach ($issue in $formattingIssues) {
            Write-Host "  - $issue"
        }
        Write-InfoOutput "Run with -Fix flag to automatically fix formatting issues"
        $hasIssues = $true
    } else {
        Write-SuccessOutput "All files are properly formatted"
    }
} else {
    Write-WarningOutput "clang-format not found. Skipping formatting checks."
}

# Check for clang-tidy
if (Test-Command "clang-tidy") {
    Write-InfoOutput "Running clang-tidy..."
    
    $buildDir = Join-Path $projectRoot "build"
    $compileCommandsFile = Join-Path $buildDir "compile_commands.json"
    
    if (Test-Path $compileCommandsFile) {
        # Run clang-tidy on source files
        $sourceFiles = Get-ChildItem -Path (Join-Path $projectRoot "src") -Include "*.cpp" -Recurse
        
        $tidyIssues = @()
        foreach ($file in $sourceFiles) {
            Write-VerboseOutput "Analyzing: $($file.FullName)"
            
            $output = & clang-tidy -p $buildDir $file.FullName 2>&1
            if ($output -match "(warning|error):") {
                $tidyIssues += @{
                    File = $file.FullName
                    Output = $output
                }
            }
        }
        
        if ($tidyIssues.Count -gt 0) {
            Write-WarningOutput "clang-tidy found issues in $($tidyIssues.Count) files"
            foreach ($issue in $tidyIssues) {
                Write-Host ""
                Write-Host "File: $($issue.File)" -ForegroundColor Yellow
                Write-Host $issue.Output
            }
            $hasIssues = $true
        } else {
            Write-SuccessOutput "No clang-tidy issues found"
        }
    } else {
        Write-WarningOutput "compile_commands.json not found. Please build the project first."
    }
} else {
    Write-WarningOutput "clang-tidy not found. Skipping static analysis."
}

# Check for cppcheck
if (Test-Command "cppcheck") {
    Write-InfoOutput "Running cppcheck..."
    
    $srcDir = Join-Path $projectRoot "src"
    $suppressFile = Join-Path $projectRoot "cppcheck.suppress"
    
    $cppcheckArgs = @(
        "--enable=all",
        "--inconclusive",
        "--std=c++20",
        "--template=vs",
        "--quiet",
        "--force"
    )
    
    if (Test-Path $suppressFile) {
        $cppcheckArgs += "--suppressions-list=$suppressFile"
    }
    
    $cppcheckArgs += $srcDir
    
    $output = & cppcheck $cppcheckArgs 2>&1
    if ($output) {
        Write-WarningOutput "cppcheck found issues:"
        Write-Host $output
        $hasIssues = $true
    } else {
        Write-SuccessOutput "No cppcheck issues found"
    }
} else {
    Write-WarningOutput "cppcheck not found. Skipping additional static analysis."
}

# Check CMakeLists.txt formatting
Write-InfoOutput "Checking CMake files..."
$cmakeFiles = Get-ChildItem -Path $projectRoot -Include "CMakeLists.txt", "*.cmake" -Recurse |
              Where-Object { $_.FullName -notmatch "\\(build|vcpkg|vcpkg_installed|\.git)\\" }

if (Test-Command "cmake-format") {
    foreach ($file in $cmakeFiles) {
        Write-VerboseOutput "Checking: $($file.FullName)"
        
        if ($Fix) {
            & cmake-format -i $file.FullName
        } else {
            $output = & cmake-format --check $file.FullName 2>&1
            if ($LASTEXITCODE -ne 0) {
                Write-WarningOutput "CMake formatting issue in: $($file.FullName)"
                $hasIssues = $true
            }
        }
    }
} else {
    Write-VerboseOutput "cmake-format not found. Skipping CMake formatting checks."
}

# Check for TODO/FIXME comments
Write-InfoOutput "Checking for TODO/FIXME comments..."
$todoPattern = "TODO|FIXME|HACK|XXX|BUG"
$todoFiles = Get-ChildItem -Path $projectRoot -Include "*.cpp", "*.hpp", "*.h", "*.c" -Recurse |
             Where-Object { $_.FullName -notmatch "\\(build|vcpkg|vcpkg_installed|\.git)\\" }

$todoCount = 0
foreach ($file in $todoFiles) {
    $content = Get-Content $file.FullName -ErrorAction SilentlyContinue
    $lineNumber = 0
    foreach ($line in $content) {
        $lineNumber++
        if ($line -match $todoPattern) {
            if ($todoCount -eq 0) {
                Write-WarningOutput "Found TODO/FIXME comments:"
            }
            Write-Host "  $($file.FullName):$lineNumber : $line" -ForegroundColor Yellow
            $todoCount++
        }
    }
}

if ($todoCount -gt 0) {
    Write-InfoOutput "Total TODO/FIXME comments: $todoCount"
}

# Summary
Write-Host ""
Write-Host "Quality Check Summary" -ForegroundColor Cyan
Write-Host "===================" -ForegroundColor Cyan

if ($hasIssues) {
    Write-WarningOutput "Code quality issues were found. Please review and fix them."
    if (-not $Fix) {
        Write-InfoOutput "Run with -Fix flag to automatically fix some issues"
    }
    exit 1
} else {
    Write-SuccessOutput "All quality checks passed!"
} 
# Simple Code Quality Check
Write-Host "Running Simple Code Quality Check..." -ForegroundColor Green

# Check if tools are available
Write-Host "`nChecking for code quality tools..." -ForegroundColor Cyan

# Check clang-format
try {
    $null = clang-format --version 2>$null
    Write-Host "✓ clang-format is available" -ForegroundColor Green
} catch {
    Write-Host "✗ clang-format is not available" -ForegroundColor Red
}

# Check cppcheck
try {
    $null = cppcheck --version 2>$null
    Write-Host "✓ cppcheck is available" -ForegroundColor Green
} catch {
    Write-Host "✗ cppcheck is not available" -ForegroundColor Red
}

# Check clang-tidy
try {
    $null = clang-tidy --version 2>$null
    Write-Host "✓ clang-tidy is available" -ForegroundColor Green
} catch {
    Write-Host "✗ clang-tidy is not available" -ForegroundColor Red
}

# Check source files exist
Write-Host "`nChecking source files..." -ForegroundColor Cyan
if (Test-Path "src") {
    $cppFiles = Get-ChildItem -Path "src" -Filter "*.cpp" -Recurse -ErrorAction SilentlyContinue
    $hFiles = Get-ChildItem -Path "src" -Filter "*.h" -Recurse -ErrorAction SilentlyContinue
    Write-Host "Found $($cppFiles.Count) .cpp files and $($hFiles.Count) .h files" -ForegroundColor Green
} else {
    Write-Host "✗ src directory not found" -ForegroundColor Red
}

# Check build directory
Write-Host "`nChecking build directory..." -ForegroundColor Cyan
if (Test-Path "build") {
    Write-Host "✓ build directory exists" -ForegroundColor Green
} else {
    Write-Host "✗ build directory not found" -ForegroundColor Red
}

Write-Host "`nSimple quality check complete!" -ForegroundColor Green 
# Verify Code Quality Tools
Write-Host "Verifying Code Quality Tools..." -ForegroundColor Green

# Check for required tools
$tools = @{
    "clang-format" = "clang-format --version"
    "cppcheck" = "cppcheck --version"
    "clang-tidy" = "clang-tidy --version"
}

$missingTools = @()

foreach ($tool in $tools.GetEnumerator()) {
    try {
        Invoke-Expression $tool.Value | Out-Null
        Write-Host "✓ $($tool.Key) is installed" -ForegroundColor Green
    }
    catch {
        Write-Host "✗ $($tool.Key) is not installed" -ForegroundColor Red
        $missingTools += $tool.Key
    }
}

if ($missingTools.Count -gt 0) {
    Write-Host "`nMissing tools:" -ForegroundColor Yellow
    foreach ($tool in $missingTools) {
        Write-Host "- $tool" -ForegroundColor Yellow
    }
    Write-Host "`nPlease install the missing tools to enable code quality checks." -ForegroundColor Yellow
    exit 1
}

# Run code quality checks
Write-Host "`nRunning Code Quality Checks..." -ForegroundColor Green

# Check formatting
Write-Host "`nChecking code formatting..." -ForegroundColor Cyan
$cppFiles = Get-ChildItem -Path "src" -Filter "*.cpp" -Recurse -ErrorAction SilentlyContinue
$hFiles = Get-ChildItem -Path "src" -Filter "*.h" -Recurse -ErrorAction SilentlyContinue
$allFiles = @($cppFiles) + @($hFiles)

$formatIssues = @()
foreach ($file in $allFiles) {
    try {
        $output = clang-format --dry-run --Werror $file.FullName 2>&1
        if ($LASTEXITCODE -ne 0) {
            $formatIssues += $file.Name
        }
    }
    catch {
        Write-Host "Warning: Could not check formatting for $($file.Name)" -ForegroundColor Yellow
    }
}

if ($formatIssues.Count -gt 0) {
    Write-Host "`nFormatting issues found in:" -ForegroundColor Red
    foreach ($file in $formatIssues) {
        Write-Host "- $file" -ForegroundColor Red
    }
}
else {
    Write-Host "✓ All files are properly formatted" -ForegroundColor Green
}

# Run cppcheck if available
Write-Host "`nRunning static analysis..." -ForegroundColor Cyan
try {
    cppcheck --enable=all --suppress=missingIncludeSystem --std=c++17 src/ 2>&1
}
catch {
    Write-Host "Warning: cppcheck not available or failed" -ForegroundColor Yellow
}

# Run clang-tidy if available
Write-Host "`nRunning clang-tidy..." -ForegroundColor Cyan
try {
    if (Test-Path "build") {
        clang-tidy -p build src/*.cpp src/*.h -- -std=c++17
    }
    else {
        Write-Host "Warning: build directory not found, skipping clang-tidy" -ForegroundColor Yellow
    }
}
catch {
    Write-Host "Warning: clang-tidy not available or failed" -ForegroundColor Yellow
}

Write-Host "`nCode quality verification complete!" -ForegroundColor Green 
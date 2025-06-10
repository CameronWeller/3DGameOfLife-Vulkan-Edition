#!/usr/bin/env pwsh
# UX-Mirror Overnight Optimization Runner
# Usage: .\run_overnight.ps1 [hours]

param(
    [int]$Hours = 8,
    [switch]$Background = $false,
    [switch]$Help = $false
)

if ($Help) {
    Write-Host @"
UX-Mirror Overnight Optimization Runner
=====================================

Usage: .\run_overnight.ps1 [OPTIONS]

Options:
  -Hours <number>     Hours to run optimization (1-24, default: 8)
  -Background         Run in background (detached process)
  -Help               Show this help message

Examples:
  .\run_overnight.ps1                    # Run for 8 hours
  .\run_overnight.ps1 -Hours 12          # Run for 12 hours  
  .\run_overnight.ps1 -Hours 1 -Background # Run 1 hour in background

Log files are saved as: optimization_log_YYYYMMDD_HHMMSS.txt
"@
    exit 0
}

# Validate hours parameter
if ($Hours -lt 1 -or $Hours -gt 24) {
    Write-Error "Hours must be between 1 and 24. Got: $Hours"
    exit 1
}

# Check if executable exists
$ExePath = ".\build_optimization\Release\minimal_overnight_optimizer.exe"
if (-not (Test-Path $ExePath)) {
    Write-Error "Optimizer executable not found at: $ExePath"
    Write-Host "Please build the project first with:"
    Write-Host "  cmake -B build_optimization -S ."
    Write-Host "  cmake --build build_optimization --config Release"
    exit 1
}

# Display startup information
Write-Host "🚀 UX-Mirror Overnight Optimization" -ForegroundColor Cyan
Write-Host "====================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "⏱️  Duration: $Hours hour$(if($Hours -ne 1){'s'})" -ForegroundColor Green
Write-Host "📁 Executable: $ExePath" -ForegroundColor Yellow
Write-Host "📝 Log files: optimization_log_*.txt" -ForegroundColor Yellow
Write-Host ""

if ($Background) {
    Write-Host "🔄 Starting in background..." -ForegroundColor Magenta
    Start-Process -FilePath $ExePath -ArgumentList $Hours -NoNewWindow -PassThru
    Write-Host "✅ Background process started!" -ForegroundColor Green
    Write-Host "Monitor progress with: Get-Content optimization_log_*.txt -Wait" -ForegroundColor Cyan
} else {
    Write-Host "🔄 Starting optimization (Press Ctrl+C to stop)..." -ForegroundColor Magenta
    Write-Host ""
    
    try {
        & $ExePath $Hours
        Write-Host ""
        Write-Host "✅ Optimization completed successfully!" -ForegroundColor Green
    } catch {
        Write-Host ""
        Write-Host "❌ Optimization interrupted or failed: $($_.Exception.Message)" -ForegroundColor Red
        exit 1
    }
}

Write-Host ""
Write-Host "📊 Check the latest log file for detailed results:" -ForegroundColor Cyan
$LatestLog = Get-ChildItem -Name "optimization_log_*.txt" | Sort-Object -Descending | Select-Object -First 1
if ($LatestLog) {
    Write-Host "   $LatestLog" -ForegroundColor White
} else {
    Write-Host "   No log files found" -ForegroundColor Yellow
} 
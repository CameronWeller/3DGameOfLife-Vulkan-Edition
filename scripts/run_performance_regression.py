#!/usr/bin/env python3

import argparse
import json
import os
import subprocess
import sys
from datetime import datetime
from pathlib import Path

def parse_args():
    parser = argparse.ArgumentParser(description='Run performance regression tests')
    parser.add_argument('--baseline', default='main',
                      help='Baseline branch or commit to compare against')
    parser.add_argument('--threshold', type=float, default=0.1,
                      help='Performance regression threshold (10% by default)')
    parser.add_argument('--output-dir', default='benchmark_results',
                      help='Directory to store benchmark results')
    return parser.parse_args()

def run_benchmarks():
    """Run all benchmark tests and return the results."""
    results = {}
    
    # Run compute shader benchmarks
    compute_results = subprocess.run(
        ['./compute_shader_benchmark', '--benchmark_format=json'],
        capture_output=True,
        text=True
    )
    results['compute_shader'] = json.loads(compute_results.stdout)
    
    # Run Vulkan performance tests
    vulkan_results = subprocess.run(
        ['./vulkan_performance_tests', '--benchmark_format=json'],
        capture_output=True,
        text=True
    )
    results['vulkan_performance'] = json.loads(vulkan_results.stdout)
    
    return results

def get_baseline_results(baseline):
    """Get benchmark results from the baseline branch/commit."""
    # Save current state
    current_branch = subprocess.run(
        ['git', 'rev-parse', '--abbrev-ref', 'HEAD'],
        capture_output=True,
        text=True
    ).stdout.strip()
    
    try:
        # Checkout baseline
        subprocess.run(['git', 'checkout', baseline], check=True)
        
        # Build and run benchmarks
        subprocess.run(['cmake', '--build', 'build', '--config', 'Release'], check=True)
        results = run_benchmarks()
        
        # Save results
        output_dir = Path('benchmark_results/baseline')
        output_dir.mkdir(parents=True, exist_ok=True)
        with open(output_dir / 'results.json', 'w') as f:
            json.dump(results, f, indent=2)
            
        return results
    finally:
        # Restore original state
        subprocess.run(['git', 'checkout', current_branch])

def compare_results(current, baseline, threshold):
    """Compare current results with baseline and report regressions."""
    regressions = []
    
    for test_suite in current:
        if test_suite not in baseline:
            print(f"Warning: Test suite {test_suite} not found in baseline")
            continue
            
        for current_benchmark in current[test_suite]['benchmarks']:
            # Find matching baseline benchmark
            baseline_benchmark = next(
                (b for b in baseline[test_suite]['benchmarks']
                 if b['name'] == current_benchmark['name']),
                None
            )
            
            if not baseline_benchmark:
                print(f"Warning: Benchmark {current_benchmark['name']} not found in baseline")
                continue
                
            # Calculate performance difference
            current_time = current_benchmark['real_time']
            baseline_time = baseline_benchmark['real_time']
            diff_percent = (current_time - baseline_time) / baseline_time * 100
            
            if diff_percent > threshold:
                regressions.append({
                    'name': current_benchmark['name'],
                    'current_time': current_time,
                    'baseline_time': baseline_time,
                    'diff_percent': diff_percent
                })
    
    return regressions

def generate_report(regressions, output_dir):
    """Generate HTML report of performance regression results."""
    report = f"""<!DOCTYPE html>
<html>
<head>
    <title>Performance Regression Report</title>
    <style>
        body {{ font-family: Arial, sans-serif; margin: 20px; }}
        table {{ border-collapse: collapse; width: 100%; }}
        th, td {{ border: 1px solid #ddd; padding: 8px; text-align: left; }}
        th {{ background-color: #f2f2f2; }}
        .regression {{ color: red; }}
    </style>
</head>
<body>
    <h1>Performance Regression Report</h1>
    <p>Generated: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}</p>
    
    <h2>Regressions</h2>
    <table>
        <tr>
            <th>Benchmark</th>
            <th>Current Time (ns)</th>
            <th>Baseline Time (ns)</th>
            <th>Difference (%)</th>
        </tr>
"""
    
    for reg in regressions:
        report += f"""
        <tr class="regression">
            <td>{reg['name']}</td>
            <td>{reg['current_time']:.2f}</td>
            <td>{reg['baseline_time']:.2f}</td>
            <td>{reg['diff_percent']:.2f}%</td>
        </tr>
"""
    
    report += """
    </table>
</body>
</html>
"""
    
    with open(Path(output_dir) / 'regression_report.html', 'w') as f:
        f.write(report)

def main():
    args = parse_args()
    
    # Create output directory
    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)
    
    # Get current results
    print("Running current benchmarks...")
    current_results = run_benchmarks()
    
    # Get baseline results
    print(f"Getting baseline results from {args.baseline}...")
    baseline_results = get_baseline_results(args.baseline)
    
    # Compare results
    print("Comparing results...")
    regressions = compare_results(current_results, baseline_results, args.threshold)
    
    # Generate report
    print("Generating report...")
    generate_report(regressions, output_dir)
    
    # Exit with error if there are regressions
    if regressions:
        print(f"\nFound {len(regressions)} performance regressions!")
        sys.exit(1)
    else:
        print("\nNo performance regressions found.")

if __name__ == '__main__':
    main() 
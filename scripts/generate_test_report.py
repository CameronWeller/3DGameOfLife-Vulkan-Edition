#!/usr/bin/env python3

import argparse
import json
import os
import re
from datetime import datetime
from pathlib import Path

def parse_args():
    parser = argparse.ArgumentParser(description='Generate test report from test results')
    parser.add_argument('--input', required=True,
                      help='Input test results log file')
    parser.add_argument('--output', required=True,
                      help='Output HTML report file')
    parser.add_argument('--coverage', default=None,
                      help='Coverage report directory')
    return parser.parse_args()

def parse_test_results(log_file):
    """Parse test results from log file."""
    results = {
        'unit': {'total': 0, 'passed': 0, 'failed': 0, 'tests': []},
        'integration': {'total': 0, 'passed': 0, 'failed': 0, 'tests': []},
        'performance': {'total': 0, 'passed': 0, 'failed': 0, 'tests': []},
        'stress': {'total': 0, 'passed': 0, 'failed': 0, 'tests': []},
        'vulkan_performance': {'total': 0, 'passed': 0, 'failed': 0, 'tests': []}
    }
    
    current_suite = None
    current_test = None
    
    with open(log_file, 'r') as f:
        for line in f:
            # Match test suite start
            suite_match = re.match(r'Test project .*/(\w+)_tests', line)
            if suite_match:
                current_suite = suite_match.group(1)
                continue
                
            # Match test result
            test_match = re.match(r'(\d+/\d+) Test #(\d+): (.*) \.\.\. (.*)', line)
            if test_match and current_suite:
                total, num, name, status = test_match.groups()
                test_result = {
                    'name': name,
                    'status': status,
                    'number': int(num)
                }
                results[current_suite]['tests'].append(test_result)
                results[current_suite]['total'] += 1
                if status == 'Passed':
                    results[current_suite]['passed'] += 1
                else:
                    results[current_suite]['failed'] += 1
                    
            # Match test output
            if current_suite and current_test and line.startswith('    '):
                current_test['output'] = current_test.get('output', '') + line[4:]
    
    return results

def parse_coverage_report(coverage_dir):
    """Parse coverage report from lcov/genhtml output."""
    if not coverage_dir:
        return None
        
    coverage = {
        'total': 0,
        'covered': 0,
        'files': []
    }
    
    # Parse lcov.info file
    lcov_file = Path(coverage_dir) / 'lcov.info'
    if lcov_file.exists():
        with open(lcov_file, 'r') as f:
            current_file = None
            for line in f:
                if line.startswith('SF:'):
                    current_file = {'name': line[3:].strip(), 'lines': 0, 'covered': 0}
                    coverage['files'].append(current_file)
                elif line.startswith('LH:') and current_file:
                    current_file['covered'] = int(line[3:])
                    coverage['covered'] += current_file['covered']
                elif line.startswith('LF:') and current_file:
                    current_file['lines'] = int(line[3:])
                    coverage['total'] += current_file['lines']
    
    return coverage

def generate_html_report(test_results, coverage, output_file):
    """Generate HTML report from test results and coverage data."""
    report = f"""<!DOCTYPE html>
<html>
<head>
    <title>Test Report</title>
    <style>
        body {{ font-family: Arial, sans-serif; margin: 20px; }}
        .summary {{ margin-bottom: 20px; }}
        .suite {{ margin-bottom: 30px; }}
        table {{ border-collapse: collapse; width: 100%; margin-bottom: 20px; }}
        th, td {{ border: 1px solid #ddd; padding: 8px; text-align: left; }}
        th {{ background-color: #f2f2f2; }}
        .passed {{ color: green; }}
        .failed {{ color: red; }}
        .coverage {{ margin-top: 30px; }}
        .progress {{ background-color: #f0f0f0; height: 20px; }}
        .progress-bar {{ background-color: #4CAF50; height: 100%; }}
    </style>
</head>
<body>
    <h1>Test Report</h1>
    <p>Generated: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}</p>
    
    <div class="summary">
        <h2>Summary</h2>
        <table>
            <tr>
                <th>Test Suite</th>
                <th>Total</th>
                <th>Passed</th>
                <th>Failed</th>
                <th>Success Rate</th>
            </tr>
"""
    
    # Add summary rows
    for suite, data in test_results.items():
        success_rate = (data['passed'] / data['total'] * 100) if data['total'] > 0 else 0
        report += f"""
            <tr>
                <td>{suite}</td>
                <td>{data['total']}</td>
                <td class="passed">{data['passed']}</td>
                <td class="failed">{data['failed']}</td>
                <td>{success_rate:.1f}%</td>
            </tr>
"""
    
    report += """
        </table>
    </div>
"""
    
    # Add detailed test results
    for suite, data in test_results.items():
        report += f"""
    <div class="suite">
        <h2>{suite.title()} Tests</h2>
        <table>
            <tr>
                <th>Test Name</th>
                <th>Status</th>
                <th>Details</th>
            </tr>
"""
        
        for test in data['tests']:
            status_class = 'passed' if test['status'] == 'Passed' else 'failed'
            report += f"""
            <tr>
                <td>{test['name']}</td>
                <td class="{status_class}">{test['status']}</td>
                <td>{test.get('output', '')}</td>
            </tr>
"""
        
        report += """
        </table>
    </div>
"""
    
    # Add coverage report if available
    if coverage:
        report += f"""
    <div class="coverage">
        <h2>Code Coverage</h2>
        <p>Total Coverage: {coverage['covered'] / coverage['total'] * 100:.1f}%</p>
        <div class="progress">
            <div class="progress-bar" style="width: {coverage['covered'] / coverage['total'] * 100}%"></div>
        </div>
        
        <h3>File Coverage</h3>
        <table>
            <tr>
                <th>File</th>
                <th>Lines</th>
                <th>Covered</th>
                <th>Coverage</th>
            </tr>
"""
        
        for file in coverage['files']:
            coverage_percent = file['covered'] / file['lines'] * 100 if file['lines'] > 0 else 0
            report += f"""
            <tr>
                <td>{file['name']}</td>
                <td>{file['lines']}</td>
                <td>{file['covered']}</td>
                <td>{coverage_percent:.1f}%</td>
            </tr>
"""
        
        report += """
        </table>
    </div>
"""
    
    report += """
</body>
</html>
"""
    
    with open(output_file, 'w') as f:
        f.write(report)

def main():
    args = parse_args()
    
    # Parse test results
    test_results = parse_test_results(args.input)
    
    # Parse coverage report if available
    coverage = parse_coverage_report(args.coverage)
    
    # Generate HTML report
    generate_html_report(test_results, coverage, args.output)

if __name__ == '__main__':
    main() 
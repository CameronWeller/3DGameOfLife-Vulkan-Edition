#!/usr/bin/env python3

import os
import subprocess
import json
import datetime
import argparse
from pathlib import Path

def run_command(command):
    try:
        result = subprocess.run(command, capture_output=True, text=True, check=True)
        return result.stdout
    except subprocess.CalledProcessError as e:
        print(f"Error running command: {command}")
        print(f"Error output: {e.stderr}")
        raise

def run_tests(build_dir, output_dir):
    # Create output directory if it doesn't exist
    os.makedirs(output_dir, exist_ok=True)

    # Get list of test executables
    test_executables = [
        "vulkan_pipeline_tests",
        "vulkan_memory_tests",
        "vulkan_shader_tests",
        "vulkan_command_buffer_tests"
    ]

    # Run each test and collect results
    all_results = []
    timestamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")

    for test_exe in test_executables:
        print(f"\nRunning {test_exe}...")
        
        # Run the test with benchmark output
        test_path = os.path.join(build_dir, test_exe)
        output = run_command([test_path, "--benchmark_format=json"])
        
        # Parse benchmark results
        try:
            benchmark_data = json.loads(output)
            for benchmark in benchmark_data.get("benchmarks", []):
                result = {
                    "name": f"{test_exe}_{benchmark['name']}",
                    "mean": benchmark["mean"],
                    "stddev": benchmark["stddev"],
                    "min": benchmark["min"],
                    "max": benchmark["max"],
                    "samples": benchmark.get("samples", [])
                }
                all_results.append(result)
        except json.JSONDecodeError:
            print(f"Warning: Could not parse JSON output from {test_exe}")
            continue

    # Generate HTML report
    report_path = os.path.join(output_dir, f"benchmark_report_{timestamp}.html")
    generate_html_report(report_path, all_results)
    print(f"\nBenchmark report generated: {report_path}")

    # Generate JSON report for programmatic analysis
    json_path = os.path.join(output_dir, f"benchmark_data_{timestamp}.json")
    with open(json_path, 'w') as f:
        json.dump(all_results, f, indent=2)
    print(f"Benchmark data saved: {json_path}")

def generate_html_report(output_path, results):
    html_template = """
    <!DOCTYPE html>
    <html>
    <head>
        <title>Vulkan Performance Benchmark Results</title>
        <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
        <style>
            body { font-family: Arial, sans-serif; margin: 20px; }
            .chart-container { width: 800px; height: 400px; margin: 20px 0; }
            table { border-collapse: collapse; width: 100%; margin: 20px 0; }
            th, td { border: 1px solid #ddd; padding: 8px; text-align: left; }
            th { background-color: #f2f2f2; }
            tr:nth-child(even) { background-color: #f9f9f9; }
        </style>
    </head>
    <body>
        <h1>Vulkan Performance Benchmark Results</h1>
        <div class="chart-container">
            <canvas id="benchmarkChart"></canvas>
        </div>
        <table>
            <tr>
                <th>Benchmark</th>
                <th>Mean (ms)</th>
                <th>StdDev (ms)</th>
                <th>Min (ms)</th>
                <th>Max (ms)</th>
            </tr>
            {table_rows}
        </table>
        <script>
            const ctx = document.getElementById('benchmarkChart').getContext('2d');
            new Chart(ctx, {
                type: 'bar',
                data: {
                    labels: {labels},
                    datasets: [{
                        label: 'Mean Execution Time (ms)',
                        data: {data},
                        backgroundColor: 'rgba(54, 162, 235, 0.5)',
                        borderColor: 'rgba(54, 162, 235, 1)',
                        borderWidth: 1
                    }]
                },
                options: {
                    responsive: true,
                    scales: {
                        y: {
                            beginAtZero: true,
                            title: {
                                display: true,
                                text: 'Time (ms)'
                            }
                        }
                    }
                }
            });
        </script>
    </body>
    </html>
    """

    # Generate table rows
    table_rows = ""
    for result in results:
        table_rows += f"""
            <tr>
                <td>{result['name']}</td>
                <td>{result['mean']:.3f}</td>
                <td>{result['stddev']:.3f}</td>
                <td>{result['min']:.3f}</td>
                <td>{result['max']:.3f}</td>
            </tr>
        """

    # Generate chart data
    labels = [result['name'] for result in results]
    data = [result['mean'] for result in results]

    # Write the HTML file
    with open(output_path, 'w') as f:
        f.write(html_template.format(
            table_rows=table_rows,
            labels=json.dumps(labels),
            data=json.dumps(data)
        ))

def main():
    parser = argparse.ArgumentParser(description='Run Vulkan performance tests and generate reports')
    parser.add_argument('--build-dir', default='build', help='Build directory containing test executables')
    parser.add_argument('--output-dir', default='benchmark_results', help='Output directory for benchmark results')
    args = parser.parse_args()

    run_tests(args.build_dir, args.output_dir)

if __name__ == '__main__':
    main() 
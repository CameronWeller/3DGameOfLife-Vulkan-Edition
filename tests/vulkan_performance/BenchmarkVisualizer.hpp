#pragma once

#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <map>

class BenchmarkVisualizer {
public:
    struct BenchmarkResult {
        std::string name;
        double mean;
        double stddev;
        double min;
        double max;
        std::vector<double> samples;
    };

    static void generateReport(const std::string& outputPath, const std::vector<BenchmarkResult>& results) {
        std::ofstream file(outputPath);
        if (!file.is_open()) {
            throw std::runtime_error("Failed to open output file for benchmark visualization");
        }

        // Write HTML header with Chart.js
        file << R"(
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
)";

        // Write table rows
        for (const auto& result : results) {
            file << "<tr>\n";
            file << "    <td>" << result.name << "</td>\n";
            file << "    <td>" << std::fixed << std::setprecision(3) << result.mean << "</td>\n";
            file << "    <td>" << std::fixed << std::setprecision(3) << result.stddev << "</td>\n";
            file << "    <td>" << std::fixed << std::setprecision(3) << result.min << "</td>\n";
            file << "    <td>" << std::fixed << std::setprecision(3) << result.max << "</td>\n";
            file << "</tr>\n";
        }

        // Write Chart.js initialization
        file << R"(
    </table>
    <script>
        const ctx = document.getElementById('benchmarkChart').getContext('2d');
        new Chart(ctx, {
            type: 'bar',
            data: {
                labels: [)";

        // Add labels
        for (size_t i = 0; i < results.size(); ++i) {
            file << "'" << results[i].name << "'";
            if (i < results.size() - 1) file << ", ";
        }

        file << R"(],
                datasets: [{
                    label: 'Mean Execution Time (ms)',
                    data: [)";

        // Add mean values
        for (size_t i = 0; i < results.size(); ++i) {
            file << results[i].mean;
            if (i < results.size() - 1) file << ", ";
        }

        file << R"(],
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
)";

        file.close();
    }

    static BenchmarkResult parseBenchmarkOutput(const std::string& benchmarkOutput) {
        BenchmarkResult result;
        std::istringstream iss(benchmarkOutput);
        std::string line;

        // Parse benchmark name
        if (std::getline(iss, line)) {
            result.name = line.substr(0, line.find("_"));
        }

        // Parse statistics
        while (std::getline(iss, line)) {
            if (line.find("mean") != std::string::npos) {
                result.mean = std::stod(line.substr(line.find(":") + 1));
            }
            else if (line.find("stddev") != std::string::npos) {
                result.stddev = std::stod(line.substr(line.find(":") + 1));
            }
            else if (line.find("min") != std::string::npos) {
                result.min = std::stod(line.substr(line.find(":") + 1));
            }
            else if (line.find("max") != std::string::npos) {
                result.max = std::stod(line.substr(line.find(":") + 1));
            }
            else if (line.find("samples") != std::string::npos) {
                // Parse individual samples
                std::string samplesStr = line.substr(line.find(":") + 1);
                std::istringstream samplesStream(samplesStr);
                double sample;
                while (samplesStream >> sample) {
                    result.samples.push_back(sample);
                }
            }
        }

        return result;
    }
}; 
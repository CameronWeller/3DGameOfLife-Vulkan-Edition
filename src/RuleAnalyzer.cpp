#include "RuleAnalyzer.h"
#include <fstream>
#include <algorithm>
#include <cmath>
#include <sstream>
#include <iomanip>

RuleAnalyzer::RuleAnalyzer() = default;
RuleAnalyzer::~RuleAnalyzer() = default;

RuleAnalyzer::AnalysisResult RuleAnalyzer::analyzeRule(
    const GameRules::RuleSet& rule,
    uint32_t width,
    uint32_t height,
    uint32_t depth) {
    
    AnalysisResult result;
    result.ruleName = rule.name;
    
    // Initialize test grid
    testGrid_ = std::make_unique<Grid3D>(width, height, depth);
    testGrid_->setRules(rule);
    testGrid_->randomize();
    
    // Track population history
    result.populationHistory.reserve(DEFAULT_ANALYSIS_GENERATIONS);
    
    // Run simulation for analysis
    for (int gen = 0; gen < DEFAULT_ANALYSIS_GENERATIONS; ++gen) {
        // Record current population
        uint32_t population = 0;
        for (uint32_t z = 0; z < depth; ++z) {
            for (uint32_t y = 0; y < height; ++y) {
                for (uint32_t x = 0; x < width; ++x) {
                    if (testGrid_->getCell(x, y, z)) {
                        population++;
                    }
                }
            }
        }
        result.populationHistory.push_back(static_cast<float>(population));
        
        // Update grid
        testGrid_->update();
        
        // Identify patterns
        auto patterns = identifyPatterns(*testGrid_);
        for (const auto& pattern : patterns) {
            result.observedPatterns.push_back(pattern.name);
            result.patternFrequencies[pattern.name]++;
        }
    }
    
    // Calculate metrics
    result.stability = calculateStability(*testGrid_, DEFAULT_ANALYSIS_GENERATIONS);
    result.growthRate = calculateGrowthRate(*testGrid_, DEFAULT_ANALYSIS_GENERATIONS);
    result.complexity = calculateComplexity(*testGrid_);
    
    return result;
}

std::vector<RuleAnalyzer::PatternType> RuleAnalyzer::identifyPatterns(const Grid3D& grid) {
    std::vector<PatternType> patterns;
    
    // Check for stability
    if (isStable(grid, 5)) {
        patterns.push_back({
            PatternType::Category::STABLE,
            "Stable Pattern",
            "A pattern that remains unchanged over time",
            1
        });
    }
    
    // Check for oscillators
    if (isOscillator(grid, MAX_OSCILLATOR_PERIOD)) {
        patterns.push_back({
            PatternType::Category::OSCILLATOR,
            "Oscillator",
            "A pattern that repeats in a cycle",
            MAX_OSCILLATOR_PERIOD
        });
    }
    
    // Check for spaceships
    if (isSpaceship(grid)) {
        patterns.push_back({
            PatternType::Category::SPACESHIP,
            "Spaceship",
            "A pattern that moves across the grid",
            0
        });
    }
    
    // Analyze growth/contraction
    float growthRate = calculateGrowthRate(grid, 10);
    if (growthRate > 0.1f) {
        patterns.push_back({
            PatternType::Category::GROWING,
            "Growing Pattern",
            "A pattern that expands over time",
            0
        });
    } else if (growthRate < -0.1f) {
        patterns.push_back({
            PatternType::Category::DYING,
            "Dying Pattern",
            "A pattern that contracts over time",
            0
        });
    }
    
    // Check for chaos
    float entropy = calculateEntropy(grid);
    if (entropy > 0.7f) {
        patterns.push_back({
            PatternType::Category::CHAOTIC,
            "Chaotic Pattern",
            "A pattern with high entropy and unpredictable behavior",
            0
        });
    }
    
    return patterns;
}

float RuleAnalyzer::calculateStability(const Grid3D& grid, int generations) {
    std::vector<uint32_t> populations;
    populations.reserve(generations);
    
    // Create a copy of the grid for testing
    auto testGrid = std::make_unique<Grid3D>(grid.getWidth(), grid.getHeight(), grid.getDepth());
    testGrid->setRules(grid.getRules());
    
    // Copy initial state
    for (uint32_t z = 0; z < grid.getDepth(); ++z) {
        for (uint32_t y = 0; y < grid.getHeight(); ++y) {
            for (uint32_t x = 0; x < grid.getWidth(); ++x) {
                testGrid->setCell(x, y, z, grid.getCell(x, y, z));
            }
        }
    }
    
    // Track population changes
    for (int gen = 0; gen < generations; ++gen) {
        uint32_t population = 0;
        for (uint32_t z = 0; z < grid.getDepth(); ++z) {
            for (uint32_t y = 0; y < grid.getHeight(); ++y) {
                for (uint32_t x = 0; x < grid.getWidth(); ++x) {
                    if (testGrid->getCell(x, y, z)) {
                        population++;
                    }
                }
            }
        }
        populations.push_back(population);
        testGrid->update();
    }
    
    // Calculate stability based on population variance
    float mean = 0.0f;
    for (uint32_t pop : populations) {
        mean += static_cast<float>(pop);
    }
    mean /= static_cast<float>(generations);
    
    float variance = 0.0f;
    for (uint32_t pop : populations) {
        float diff = static_cast<float>(pop) - mean;
        variance += diff * diff;
    }
    variance /= static_cast<float>(generations);
    
    // Convert variance to stability score (0-1)
    float maxVariance = static_cast<float>(grid.getWidth() * grid.getHeight() * grid.getDepth());
    return 1.0f - std::min(variance / maxVariance, 1.0f);
}

float RuleAnalyzer::calculateGrowthRate(const Grid3D& grid, int generations) {
    std::vector<uint32_t> populations;
    populations.reserve(generations);
    
    // Create a copy of the grid for testing
    auto testGrid = std::make_unique<Grid3D>(grid.getWidth(), grid.getHeight(), grid.getDepth());
    testGrid->setRules(grid.getRules());
    
    // Copy initial state
    for (uint32_t z = 0; z < grid.getDepth(); ++z) {
        for (uint32_t y = 0; y < grid.getHeight(); ++y) {
            for (uint32_t x = 0; x < grid.getWidth(); ++x) {
                testGrid->setCell(x, y, z, grid.getCell(x, y, z));
            }
        }
    }
    
    // Track population changes
    for (int gen = 0; gen < generations; ++gen) {
        uint32_t population = 0;
        for (uint32_t z = 0; z < grid.getDepth(); ++z) {
            for (uint32_t y = 0; y < grid.getHeight(); ++y) {
                for (uint32_t x = 0; x < grid.getWidth(); ++x) {
                    if (testGrid->getCell(x, y, z)) {
                        population++;
                    }
                }
            }
        }
        populations.push_back(population);
        testGrid->update();
    }
    
    // Calculate growth rate
    if (populations.size() < 2) {
        return 0.0f;
    }
    
    float initialPop = static_cast<float>(populations[0]);
    float finalPop = static_cast<float>(populations.back());
    float totalCells = static_cast<float>(grid.getWidth() * grid.getHeight() * grid.getDepth());
    
    return (finalPop - initialPop) / (initialPop * static_cast<float>(generations));
}

float RuleAnalyzer::calculateComplexity(const Grid3D& grid) {
    // Calculate complexity based on entropy and pattern diversity
    float entropy = calculateEntropy(grid);
    float patternDiversity = 0.0f;
    
    // Count different local patterns
    std::map<std::string, int> patternCounts;
    for (uint32_t z = 1; z < grid.getDepth() - 1; ++z) {
        for (uint32_t y = 1; y < grid.getHeight() - 1; ++y) {
            for (uint32_t x = 1; x < grid.getWidth() - 1; ++x) {
                std::string pattern;
                for (int dz = -1; dz <= 1; ++dz) {
                    for (int dy = -1; dy <= 1; ++dy) {
                        for (int dx = -1; dx <= 1; ++dx) {
                            pattern += grid.getCell(x + dx, y + dy, z + dz) ? '1' : '0';
                        }
                    }
                }
                patternCounts[pattern]++;
            }
        }
    }
    
    // Calculate pattern diversity
    float totalPatterns = static_cast<float>(patternCounts.size());
    float maxPossiblePatterns = 27.0f; // 3x3x3 neighborhood
    patternDiversity = totalPatterns / maxPossiblePatterns;
    
    // Combine metrics
    return (entropy + patternDiversity) * 0.5f;
}

void RuleAnalyzer::generateRuleReport(const AnalysisResult& result, const std::string& outputPath) {
    std::ofstream file(outputPath);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open output file for rule report");
    }
    
    // Write HTML report
    file << R"(
<!DOCTYPE html>
<html>
<head>
    <title>Rule Analysis Report - )" << result.ruleName << R"(</title>
    <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; }
        .chart-container { width: 800px; height: 400px; margin: 20px 0; }
        .metric { margin: 10px 0; }
        .pattern-list { margin: 20px 0; }
    </style>
</head>
<body>
    <h1>Rule Analysis Report: )" << result.ruleName << R"(</h1>
    
    <div class="metrics">
        <div class="metric">
            <h3>Stability Score: )" << std::fixed << std::setprecision(2) << result.stability << R"(</h3>
        </div>
        <div class="metric">
            <h3>Growth Rate: )" << result.growthRate << R"(</h3>
        </div>
        <div class="metric">
            <h3>Complexity: )" << result.complexity << R"(</h3>
        </div>
    </div>
    
    <div class="chart-container">
        <canvas id="populationChart"></canvas>
    </div>
    
    <div class="pattern-list">
        <h3>Observed Patterns:</h3>
        <ul>
)";
    
    for (const auto& pattern : result.observedPatterns) {
        file << "            <li>" << pattern << " (Frequency: " 
             << result.patternFrequencies[pattern] << ")</li>\n";
    }
    
    file << R"(
        </ul>
    </div>
    
    <script>
        const ctx = document.getElementById('populationChart').getContext('2d');
        new Chart(ctx, {
            type: 'line',
            data: {
                labels: Array.from({length: )" << result.populationHistory.size() << R"(}, (_, i) => i),
                datasets: [{
                    label: 'Population Over Time',
                    data: [)" << std::fixed << std::setprecision(2);
    
    for (size_t i = 0; i < result.populationHistory.size(); ++i) {
        file << result.populationHistory[i];
        if (i < result.populationHistory.size() - 1) {
            file << ", ";
        }
    }
    
    file << R"(],
                    borderColor: 'rgb(75, 192, 192)',
                    tension: 0.1
                }]
            },
            options: {
                responsive: true,
                scales: {
                    y: {
                        beginAtZero: true,
                        title: {
                            display: true,
                            text: 'Population'
                        }
                    },
                    x: {
                        title: {
                            display: true,
                            text: 'Generation'
                        }
                    }
                }
            }
        });
    </script>
</body>
</html>
)";
}

bool RuleAnalyzer::isStable(const Grid3D& grid, int generations) {
    auto testGrid = std::make_unique<Grid3D>(grid.getWidth(), grid.getHeight(), grid.getDepth());
    testGrid->setRules(grid.getRules());
    
    // Copy initial state
    for (uint32_t z = 0; z < grid.getDepth(); ++z) {
        for (uint32_t y = 0; y < grid.getHeight(); ++y) {
            for (uint32_t x = 0; x < grid.getWidth(); ++x) {
                testGrid->setCell(x, y, z, grid.getCell(x, y, z));
            }
        }
    }
    
    // Check if pattern remains unchanged
    for (int gen = 0; gen < generations; ++gen) {
        bool changed = false;
        testGrid->update();
        
        for (uint32_t z = 0; z < grid.getDepth() && !changed; ++z) {
            for (uint32_t y = 0; y < grid.getHeight() && !changed; ++y) {
                for (uint32_t x = 0; x < grid.getWidth() && !changed; ++x) {
                    if (testGrid->getCell(x, y, z) != grid.getCell(x, y, z)) {
                        changed = true;
                    }
                }
            }
        }
        
        if (!changed) {
            return true;
        }
    }
    
    return false;
}

bool RuleAnalyzer::isOscillator(const Grid3D& grid, int maxPeriod) {
    std::vector<std::vector<bool>> states;
    states.reserve(maxPeriod);
    
    // Create a copy of the grid for testing
    auto testGrid = std::make_unique<Grid3D>(grid.getWidth(), grid.getHeight(), grid.getDepth());
    testGrid->setRules(grid.getRules());
    
    // Copy initial state
    for (uint32_t z = 0; z < grid.getDepth(); ++z) {
        for (uint32_t y = 0; y < grid.getHeight(); ++y) {
            for (uint32_t x = 0; x < grid.getWidth(); ++x) {
                testGrid->setCell(x, y, z, grid.getCell(x, y, z));
            }
        }
    }
    
    // Store initial state
    std::vector<bool> initialState;
    initialState.reserve(grid.getWidth() * grid.getHeight() * grid.getDepth());
    for (uint32_t z = 0; z < grid.getDepth(); ++z) {
        for (uint32_t y = 0; y < grid.getHeight(); ++y) {
            for (uint32_t x = 0; x < grid.getWidth(); ++x) {
                initialState.push_back(testGrid->getCell(x, y, z));
            }
        }
    }
    states.push_back(initialState);
    
    // Check for oscillation
    for (int period = 1; period <= maxPeriod; ++period) {
        testGrid->update();
        
        std::vector<bool> currentState;
        currentState.reserve(grid.getWidth() * grid.getHeight() * grid.getDepth());
        for (uint32_t z = 0; z < grid.getDepth(); ++z) {
            for (uint32_t y = 0; y < grid.getHeight(); ++y) {
                for (uint32_t x = 0; x < grid.getWidth(); ++x) {
                    currentState.push_back(testGrid->getCell(x, y, z));
                }
            }
        }
        
        // Check if current state matches any previous state
        for (size_t i = 0; i < states.size(); ++i) {
            if (currentState == states[i]) {
                return true;
            }
        }
        
        states.push_back(currentState);
    }
    
    return false;
}

bool RuleAnalyzer::isSpaceship(const Grid3D& grid) {
    // Create a copy of the grid for testing
    auto testGrid = std::make_unique<Grid3D>(grid.getWidth(), grid.getHeight(), grid.getDepth());
    testGrid->setRules(grid.getRules());
    
    // Copy initial state
    for (uint32_t z = 0; z < grid.getDepth(); ++z) {
        for (uint32_t y = 0; y < grid.getHeight(); ++y) {
            for (uint32_t x = 0; x < grid.getWidth(); ++x) {
                testGrid->setCell(x, y, z, grid.getCell(x, y, z));
            }
        }
    }
    
    // Track center of mass
    std::vector<glm::vec3> centers;
    centers.reserve(10);
    
    for (int gen = 0; gen < 10; ++gen) {
        // Calculate center of mass
        glm::vec3 center(0.0f);
        int count = 0;
        
        for (uint32_t z = 0; z < grid.getDepth(); ++z) {
            for (uint32_t y = 0; y < grid.getHeight(); ++y) {
                for (uint32_t x = 0; x < grid.getWidth(); ++x) {
                    if (testGrid->getCell(x, y, z)) {
                        center += glm::vec3(x, y, z);
                        count++;
                    }
                }
            }
        }
        
        if (count > 0) {
            center /= static_cast<float>(count);
            centers.push_back(center);
        }
        
        testGrid->update();
    }
    
    // Check if center of mass is moving consistently
    if (centers.size() < 3) {
        return false;
    }
    
    glm::vec3 direction = centers[1] - centers[0];
    float speed = glm::length(direction);
    
    for (size_t i = 2; i < centers.size(); ++i) {
        glm::vec3 currentDirection = centers[i] - centers[i-1];
        float currentSpeed = glm::length(currentDirection);
        
        // Check if direction and speed are consistent
        if (glm::dot(glm::normalize(direction), glm::normalize(currentDirection)) < 0.9f ||
            std::abs(speed - currentSpeed) > 0.1f) {
            return false;
        }
    }
    
    return true;
}

float RuleAnalyzer::calculateEntropy(const Grid3D& grid) {
    // Calculate Shannon entropy of the grid
    std::map<std::string, int> patternCounts;
    int totalPatterns = 0;
    
    // Count 2x2x2 patterns
    for (uint32_t z = 0; z < grid.getDepth() - 1; ++z) {
        for (uint32_t y = 0; y < grid.getHeight() - 1; ++y) {
            for (uint32_t x = 0; x < grid.getWidth() - 1; ++x) {
                std::string pattern;
                for (int dz = 0; dz <= 1; ++dz) {
                    for (int dy = 0; dy <= 1; ++dy) {
                        for (int dx = 0; dx <= 1; ++dx) {
                            pattern += grid.getCell(x + dx, y + dy, z + dz) ? '1' : '0';
                        }
                    }
                }
                patternCounts[pattern]++;
                totalPatterns++;
            }
        }
    }
    
    // Calculate entropy
    float entropy = 0.0f;
    for (const auto& [pattern, count] : patternCounts) {
        float probability = static_cast<float>(count) / static_cast<float>(totalPatterns);
        entropy -= probability * std::log2(probability);
    }
    
    // Normalize entropy to 0-1 range
    float maxEntropy = std::log2(static_cast<float>(patternCounts.size()));
    return entropy / maxEntropy;
}

void RuleAnalyzer::generateComparisonReport(const std::vector<AnalysisResult>& results, const std::string& outputPath) {
    std::ofstream file(outputPath);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open output file for comparison report");
    }
    
    // Write HTML report
    file << R"(
<!DOCTYPE html>
<html>
<head>
    <title>Rule Comparison Report</title>
    <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; }
        .chart-container { width: 800px; height: 400px; margin: 20px 0; }
        .metric { margin: 10px 0; }
        .comparison-table { border-collapse: collapse; width: 100%; margin: 20px 0; }
        .comparison-table th, .comparison-table td { 
            border: 1px solid #ddd; 
            padding: 8px; 
            text-align: left; 
        }
        .comparison-table th { background-color: #f2f2f2; }
        .comparison-table tr:nth-child(even) { background-color: #f9f9f9; }
    </style>
</head>
<body>
    <h1>Rule Comparison Report</h1>
    
    <div class="chart-container">
        <canvas id="stabilityChart"></canvas>
    </div>
    
    <div class="chart-container">
        <canvas id="growthChart"></canvas>
    </div>
    
    <div class="chart-container">
        <canvas id="complexityChart"></canvas>
    </div>
    
    <table class="comparison-table">
        <tr>
            <th>Rule</th>
            <th>Stability</th>
            <th>Growth Rate</th>
            <th>Complexity</th>
            <th>Common Patterns</th>
        </tr>
)";
    
    // Write table rows
    for (const auto& result : results) {
        file << "        <tr>\n";
        file << "            <td>" << result.ruleName << "</td>\n";
        file << "            <td>" << std::fixed << std::setprecision(2) << result.stability << "</td>\n";
        file << "            <td>" << result.growthRate << "</td>\n";
        file << "            <td>" << result.complexity << "</td>\n";
        file << "            <td>";
        
        // List top 3 most frequent patterns
        std::vector<std::pair<std::string, int>> sortedPatterns;
        for (const auto& [pattern, frequency] : result.patternFrequencies) {
            sortedPatterns.emplace_back(pattern, frequency);
        }
        std::sort(sortedPatterns.begin(), sortedPatterns.end(),
            [](const auto& a, const auto& b) { return a.second > b.second; });
        
        for (size_t i = 0; i < std::min(size_t(3), sortedPatterns.size()); ++i) {
            if (i > 0) file << ", ";
            file << sortedPatterns[i].first;
        }
        
        file << "</td>\n";
        file << "        </tr>\n";
    }
    
    file << R"(
    </table>
    
    <script>
        // Stability comparison chart
        new Chart(document.getElementById('stabilityChart').getContext('2d'), {
            type: 'bar',
            data: {
                labels: [)" << std::fixed << std::setprecision(2);
    
    for (size_t i = 0; i < results.size(); ++i) {
        file << "'" << results[i].ruleName << "'";
        if (i < results.size() - 1) file << ", ";
    }
    
    file << R"(],
                datasets: [{
                    label: 'Stability Score',
                    data: [)";
    
    for (size_t i = 0; i < results.size(); ++i) {
        file << results[i].stability;
        if (i < results.size() - 1) file << ", ";
    }
    
    file << R"(],
                    backgroundColor: 'rgba(75, 192, 192, 0.5)',
                    borderColor: 'rgba(75, 192, 192, 1)',
                    borderWidth: 1
                }]
            },
            options: {
                responsive: true,
                scales: {
                    y: {
                        beginAtZero: true,
                        max: 1.0,
                        title: {
                            display: true,
                            text: 'Stability Score'
                        }
                    }
                }
            }
        });
        
        // Growth rate comparison chart
        new Chart(document.getElementById('growthChart').getContext('2d'), {
            type: 'bar',
            data: {
                labels: [)" << std::fixed << std::setprecision(2);
    
    for (size_t i = 0; i < results.size(); ++i) {
        file << "'" << results[i].ruleName << "'";
        if (i < results.size() - 1) file << ", ";
    }
    
    file << R"(],
                datasets: [{
                    label: 'Growth Rate',
                    data: [)";
    
    for (size_t i = 0; i < results.size(); ++i) {
        file << results[i].growthRate;
        if (i < results.size() - 1) file << ", ";
    }
    
    file << R"(],
                    backgroundColor: 'rgba(255, 99, 132, 0.5)',
                    borderColor: 'rgba(255, 99, 132, 1)',
                    borderWidth: 1
                }]
            },
            options: {
                responsive: true,
                scales: {
                    y: {
                        title: {
                            display: true,
                            text: 'Growth Rate'
                        }
                    }
                }
            }
        });
        
        // Complexity comparison chart
        new Chart(document.getElementById('complexityChart').getContext('2d'), {
            type: 'bar',
            data: {
                labels: [)" << std::fixed << std::setprecision(2);
    
    for (size_t i = 0; i < results.size(); ++i) {
        file << "'" << results[i].ruleName << "'";
        if (i < results.size() - 1) file << ", ";
    }
    
    file << R"(],
                datasets: [{
                    label: 'Complexity',
                    data: [)";
    
    for (size_t i = 0; i < results.size(); ++i) {
        file << results[i].complexity;
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
                        max: 1.0,
                        title: {
                            display: true,
                            text: 'Complexity Score'
                        }
                    }
                }
            }
        });
    </script>
</body>
</html>
)";
}

std::vector<std::pair<std::string, float>> RuleAnalyzer::analyzePatternInteractions(const Grid3D& grid) {
    std::vector<std::pair<std::string, float>> interactions;
    std::map<std::string, int> patternCounts;
    
    // Create a copy of the grid for testing
    auto testGrid = std::make_unique<Grid3D>(grid.getWidth(), grid.getHeight(), grid.getDepth());
    testGrid->setRules(grid.getRules());
    
    // Copy initial state
    for (uint32_t z = 0; z < grid.getDepth(); ++z) {
        for (uint32_t y = 0; y < grid.getHeight(); ++y) {
            for (uint32_t x = 0; x < grid.getWidth(); ++x) {
                testGrid->setCell(x, y, z, grid.getCell(x, y, z));
            }
        }
    }
    
    // Analyze pattern interactions over multiple generations
    for (int gen = 0; gen < DEFAULT_ANALYSIS_GENERATIONS; ++gen) {
        // Identify patterns in current state
        auto patterns = identifyPatterns(*testGrid);
        
        // Count pattern occurrences
        for (const auto& pattern : patterns) {
            patternCounts[pattern.name]++;
        }
        
        // Update grid
        testGrid->update();
    }
    
    // Calculate interaction scores
    for (const auto& [pattern, count] : patternCounts) {
        float interactionScore = calculatePatternInteractionScore(grid, pattern);
        interactions.emplace_back(pattern, interactionScore);
    }
    
    // Sort by interaction score
    std::sort(interactions.begin(), interactions.end(),
        [](const auto& a, const auto& b) { return a.second > b.second; });
    
    return interactions;
}

std::vector<std::pair<std::string, float>> RuleAnalyzer::predictPatternEvolution(const Grid3D& grid, int generations) {
    std::vector<std::pair<std::string, float>> predictions;
    std::map<std::string, int> patternCounts;
    
    // Create a copy of the grid for testing
    auto testGrid = std::make_unique<Grid3D>(grid.getWidth(), grid.getHeight(), grid.getDepth());
    testGrid->setRules(grid.getRules());
    
    // Copy initial state
    for (uint32_t z = 0; z < grid.getDepth(); ++z) {
        for (uint32_t y = 0; y < grid.getHeight(); ++y) {
            for (uint32_t x = 0; x < grid.getWidth(); ++x) {
                testGrid->setCell(x, y, z, grid.getCell(x, y, z));
            }
        }
    }
    
    // Track pattern evolution
    for (int gen = 0; gen < generations; ++gen) {
        // Predict next patterns
        auto nextPatterns = predictNextPatterns(*testGrid);
        
        // Update counts
        for (const auto& pattern : nextPatterns) {
            patternCounts[pattern]++;
        }
        
        // Update grid
        testGrid->update();
    }
    
    // Calculate prediction probabilities
    float totalPatterns = static_cast<float>(generations);
    for (const auto& [pattern, count] : patternCounts) {
        float probability = static_cast<float>(count) / totalPatterns;
        predictions.emplace_back(pattern, probability);
    }
    
    // Sort by probability
    std::sort(predictions.begin(), predictions.end(),
        [](const auto& a, const auto& b) { return a.second > b.second; });
    
    return predictions;
}

std::vector<std::string> RuleAnalyzer::classifyPatternStability(const Grid3D& grid) {
    std::vector<std::string> classifications;
    std::map<std::string, float> stabilityScores;
    
    // Create a copy of the grid for testing
    auto testGrid = std::make_unique<Grid3D>(grid.getWidth(), grid.getHeight(), grid.getDepth());
    testGrid->setRules(grid.getRules());
    
    // Copy initial state
    for (uint32_t z = 0; z < grid.getDepth(); ++z) {
        for (uint32_t y = 0; y < grid.getHeight(); ++y) {
            for (uint32_t x = 0; x < grid.getWidth(); ++x) {
                testGrid->setCell(x, y, z, grid.getCell(x, y, z));
            }
        }
    }
    
    // Analyze pattern stability over multiple generations
    for (int gen = 0; gen < DEFAULT_ANALYSIS_GENERATIONS; ++gen) {
        // Identify patterns
        auto patterns = identifyPatterns(*testGrid);
        
        // Calculate stability scores
        for (const auto& pattern : patterns) {
            float stabilityScore = calculatePatternStabilityScore(*testGrid, pattern.name);
            stabilityScores[pattern.name] = stabilityScore;
        }
        
        // Update grid
        testGrid->update();
    }
    
    // Classify patterns based on stability scores
    for (const auto& [pattern, score] : stabilityScores) {
        std::string classification;
        if (score > 0.8f) {
            classification = "Highly Stable";
        } else if (score > 0.5f) {
            classification = "Moderately Stable";
        } else if (score > 0.2f) {
            classification = "Unstable";
        } else {
            classification = "Highly Unstable";
        }
        classifications.push_back(pattern + ": " + classification);
    }
    
    return classifications;
}

std::map<std::string, std::vector<std::string>> RuleAnalyzer::generatePatternDependencyGraph(const Grid3D& grid) {
    std::map<std::string, std::vector<std::string>> dependencies;
    
    // Create a copy of the grid for testing
    auto testGrid = std::make_unique<Grid3D>(grid.getWidth(), grid.getHeight(), grid.getDepth());
    testGrid->setRules(grid.getRules());
    
    // Copy initial state
    for (uint32_t z = 0; z < grid.getDepth(); ++z) {
        for (uint32_t y = 0; y < grid.getHeight(); ++y) {
            for (uint32_t x = 0; x < grid.getWidth(); ++x) {
                testGrid->setCell(x, y, z, grid.getCell(x, y, z));
            }
        }
    }
    
    // Analyze pattern dependencies over multiple generations
    for (int gen = 0; gen < DEFAULT_ANALYSIS_GENERATIONS; ++gen) {
        // Identify current patterns
        auto patterns = identifyPatterns(*testGrid);
        
        // Find dependencies for each pattern
        for (const auto& pattern : patterns) {
            auto dependentPatterns = findDependentPatterns(*testGrid, pattern.name);
            dependencies[pattern.name] = dependentPatterns;
        }
        
        // Update grid
        testGrid->update();
    }
    
    return dependencies;
}

float RuleAnalyzer::calculatePatternInteractionScore(const Grid3D& grid, const std::string& pattern) {
    float score = 0.0f;
    int interactionCount = 0;
    
    // Create a copy of the grid for testing
    auto testGrid = std::make_unique<Grid3D>(grid.getWidth(), grid.getHeight(), grid.getDepth());
    testGrid->setRules(grid.getRules());
    
    // Copy initial state
    for (uint32_t z = 0; z < grid.getDepth(); ++z) {
        for (uint32_t y = 0; y < grid.getHeight(); ++y) {
            for (uint32_t x = 0; x < grid.getWidth(); ++x) {
                testGrid->setCell(x, y, z, grid.getCell(x, y, z));
            }
        }
    }
    
    // Analyze pattern interactions
    for (int gen = 0; gen < DEFAULT_ANALYSIS_GENERATIONS; ++gen) {
        // Check for pattern interactions within interaction radius
        for (uint32_t z = 0; z < grid.getDepth(); ++z) {
            for (uint32_t y = 0; y < grid.getHeight(); ++y) {
                for (uint32_t x = 0; x < grid.getWidth(); ++x) {
                    if (testGrid->getCell(x, y, z)) {
                        // Check surrounding cells for other patterns
                        for (int dz = -PATTERN_INTERACTION_RADIUS; dz <= PATTERN_INTERACTION_RADIUS; ++dz) {
                            for (int dy = -PATTERN_INTERACTION_RADIUS; dy <= PATTERN_INTERACTION_RADIUS; ++dy) {
                                for (int dx = -PATTERN_INTERACTION_RADIUS; dx <= PATTERN_INTERACTION_RADIUS; ++dx) {
                                    if (dx == 0 && dy == 0 && dz == 0) continue;
                                    
                                    uint32_t nx = x + dx;
                                    uint32_t ny = y + dy;
                                    uint32_t nz = z + dz;
                                    
                                    if (nx < grid.getWidth() && ny < grid.getHeight() && nz < grid.getDepth()) {
                                        if (testGrid->getCell(nx, ny, nz)) {
                                            interactionCount++;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        
        // Update grid
        testGrid->update();
    }
    
    // Calculate interaction score
    float totalCells = static_cast<float>(grid.getWidth() * grid.getHeight() * grid.getDepth());
    float maxPossibleInteractions = totalCells * (2 * PATTERN_INTERACTION_RADIUS + 1) * (2 * PATTERN_INTERACTION_RADIUS + 1) * (2 * PATTERN_INTERACTION_RADIUS + 1);
    score = static_cast<float>(interactionCount) / maxPossibleInteractions;
    
    return score;
}

std::vector<std::string> RuleAnalyzer::predictNextPatterns(const Grid3D& grid) {
    std::vector<std::string> predictions;
    
    // Create a copy of the grid for testing
    auto testGrid = std::make_unique<Grid3D>(grid.getWidth(), grid.getHeight(), grid.getDepth());
    testGrid->setRules(grid.getRules());
    
    // Copy initial state
    for (uint32_t z = 0; z < grid.getDepth(); ++z) {
        for (uint32_t y = 0; y < grid.getHeight(); ++y) {
            for (uint32_t x = 0; x < grid.getWidth(); ++x) {
                testGrid->setCell(x, y, z, grid.getCell(x, y, z));
            }
        }
    }
    
    // Update grid and identify new patterns
    testGrid->update();
    auto patterns = identifyPatterns(*testGrid);
    
    for (const auto& pattern : patterns) {
        predictions.push_back(pattern.name);
    }
    
    return predictions;
}

float RuleAnalyzer::calculatePatternStabilityScore(const Grid3D& grid, const std::string& pattern) {
    float score = 0.0f;
    int patternCount = 0;
    int stableCount = 0;
    
    // Create a copy of the grid for testing
    auto testGrid = std::make_unique<Grid3D>(grid.getWidth(), grid.getHeight(), grid.getDepth());
    testGrid->setRules(grid.getRules());
    
    // Copy initial state
    for (uint32_t z = 0; z < grid.getDepth(); ++z) {
        for (uint32_t y = 0; y < grid.getHeight(); ++y) {
            for (uint32_t x = 0; x < grid.getWidth(); ++x) {
                testGrid->setCell(x, y, z, grid.getCell(x, y, z));
            }
        }
    }
    
    // Analyze pattern stability
    for (int gen = 0; gen < DEFAULT_ANALYSIS_GENERATIONS; ++gen) {
        auto patterns = identifyPatterns(*testGrid);
        
        for (const auto& p : patterns) {
            if (p.name == pattern) {
                patternCount++;
                if (isStable(*testGrid, 5)) {
                    stableCount++;
                }
            }
        }
        
        testGrid->update();
    }
    
    if (patternCount > 0) {
        score = static_cast<float>(stableCount) / static_cast<float>(patternCount);
    }
    
    return score;
}

std::vector<std::string> RuleAnalyzer::findDependentPatterns(const Grid3D& grid, const std::string& pattern) {
    std::vector<std::string> dependencies;
    std::map<std::string, int> patternCounts;
    
    // Create a copy of the grid for testing
    auto testGrid = std::make_unique<Grid3D>(grid.getWidth(), grid.getHeight(), grid.getDepth());
    testGrid->setRules(grid.getRules());
    
    // Copy initial state
    for (uint32_t z = 0; z < grid.getDepth(); ++z) {
        for (uint32_t y = 0; y < grid.getHeight(); ++y) {
            for (uint32_t x = 0; x < grid.getWidth(); ++x) {
                testGrid->setCell(x, y, z, grid.getCell(x, y, z));
            }
        }
    }
    
    // Analyze pattern dependencies
    for (int gen = 0; gen < DEFAULT_ANALYSIS_GENERATIONS; ++gen) {
        auto patterns = identifyPatterns(*testGrid);
        
        // Check if target pattern exists
        bool hasTargetPattern = false;
        for (const auto& p : patterns) {
            if (p.name == pattern) {
                hasTargetPattern = true;
                break;
            }
        }
        
        if (hasTargetPattern) {
            // Count other patterns that appear with the target pattern
            for (const auto& p : patterns) {
                if (p.name != pattern) {
                    patternCounts[p.name]++;
                }
            }
        }
        
        testGrid->update();
    }
    
    // Convert counts to dependencies
    for (const auto& [p, count] : patternCounts) {
        if (count > DEFAULT_ANALYSIS_GENERATIONS / 2) {
            dependencies.push_back(p);
        }
    }
    
    return dependencies;
}

void RuleAnalyzer::generatePatternInteractionGraph(const std::map<std::string, std::vector<std::string>>& dependencies, const std::string& outputPath) {
    std::ofstream file(outputPath);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open output file for pattern interaction graph");
    }
    
    // Write DOT file for Graphviz
    file << "digraph PatternInteractions {\n";
    file << "    rankdir=LR;\n";
    file << "    node [shape=box, style=filled, fillcolor=lightblue];\n\n";
    
    // Add nodes and edges
    for (const auto& [pattern, deps] : dependencies) {
        file << "    \"" << pattern << "\" [label=\"" << pattern << "\"];\n";
        for (const auto& dep : deps) {
            file << "    \"" << pattern << "\" -> \"" << dep << "\";\n";
        }
    }
    
    file << "}\n";
}

void RuleAnalyzer::generateRuleSpaceMap(const std::vector<AnalysisResult>& results, const std::string& outputPath) {
    std::ofstream file(outputPath);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open output file for rule space map");
    }
    
    // Write HTML report with 3D visualization
    file << R"(
<!DOCTYPE html>
<html>
<head>
    <title>Rule Space Map</title>
    <script src="https://cdn.plot.ly/plotly-latest.min.js"></script>
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; }
        .chart-container { width: 100%; height: 800px; margin: 20px 0; }
    </style>
</head>
<body>
    <h1>Rule Space Map</h1>
    <div id="ruleSpaceMap" class="chart-container"></div>
    
    <script>
        const data = [{
            type: 'scatter3d',
            mode: 'markers+text',
            x: [)";
    
    // Add stability scores
    for (size_t i = 0; i < results.size(); ++i) {
        file << results[i].stability;
        if (i < results.size() - 1) file << ", ";
    }
    
    file << R"(],
            y: [)";
    
    // Add growth rates
    for (size_t i = 0; i < results.size(); ++i) {
        file << results[i].growthRate;
        if (i < results.size() - 1) file << ", ";
    }
    
    file << R"(],
            z: [)";
    
    // Add complexity scores
    for (size_t i = 0; i < results.size(); ++i) {
        file << results[i].complexity;
        if (i < results.size() - 1) file << ", ";
    }
    
    file << R"(],
            text: [)";
    
    // Add rule names
    for (size_t i = 0; i < results.size(); ++i) {
        file << "'" << results[i].ruleName << "'";
        if (i < results.size() - 1) file << ", ";
    }
    
    file << R"(],
            marker: {
                size: 12,
                color: [)";
    
    // Add colors based on complexity
    for (size_t i = 0; i < results.size(); ++i) {
        file << results[i].complexity;
        if (i < results.size() - 1) file << ", ";
    }
    
    file << R"(],
                colorscale: 'Viridis',
                showscale: true
            }
        }];
        
        const layout = {
            title: 'Rule Space Map',
            scene: {
                xaxis: { title: 'Stability' },
                yaxis: { title: 'Growth Rate' },
                zaxis: { title: 'Complexity' }
            },
            margin: {
                l: 0,
                r: 0,
                b: 0,
                t: 40
            }
        };
        
        Plotly.newPlot('ruleSpaceMap', data, layout);
    </script>
</body>
</html>
)"; 
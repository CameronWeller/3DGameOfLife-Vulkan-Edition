#pragma once

#include "GameRules.h"
#include <vector>
#include <map>
#include <string>
#include <memory>

// Forward declaration with proper namespace
namespace VulkanHIP {
    class Grid3D;
}

class RuleAnalyzer {
public:
    struct AnalysisResult {
        std::string ruleName;
        float stability;           // 0-1 score of pattern stability
        float growthRate;         // Average growth rate over time
        float complexity;         // Measure of pattern complexity
        std::vector<float> populationHistory;  // Population over time
        std::vector<std::string> observedPatterns;  // Types of patterns observed
        std::map<std::string, int> patternFrequencies;  // Frequency of different pattern types
        std::vector<std::pair<std::string, float>> patternInteractions;  // Pattern interaction scores
        std::vector<std::pair<std::string, float>> evolutionPredictions;  // Predicted pattern evolutions
    };

    struct PatternType {
        enum class Category {
            STABLE,      // Static patterns
            OSCILLATOR,  // Repeating patterns
            SPACESHIP,   // Moving patterns
            CHAOTIC,     // Unpredictable patterns
            GROWING,     // Expanding patterns
            DYING,       // Contracting patterns
            INTERACTING  // Patterns that interact with others
        };
        
        Category category;
        std::string name;
        std::string description;
        int period;  // For oscillators, period of oscillation
        float interactionScore;  // How likely this pattern is to interact with others
    };

    RuleAnalyzer();
    ~RuleAnalyzer();

    // Analysis methods
    AnalysisResult analyzeRule(const GameRules::RuleSet& rule, uint32_t width, uint32_t height, uint32_t depth);
    std::vector<PatternType> identifyPatterns(const VulkanHIP::Grid3D& grid);
    float calculateStability(const VulkanHIP::Grid3D& grid, int generations);
    float calculateGrowthRate(const VulkanHIP::Grid3D& grid, int generations);
    float calculateComplexity(const VulkanHIP::Grid3D& grid);

    // Advanced pattern analysis
    std::vector<std::pair<std::string, float>> analyzePatternInteractions(const VulkanHIP::Grid3D& grid);
    std::vector<std::pair<std::string, float>> predictPatternEvolution(const VulkanHIP::Grid3D& grid, int generations);
    std::vector<std::string> classifyPatternStability(const VulkanHIP::Grid3D& grid);
    std::map<std::string, std::vector<std::string>> generatePatternDependencyGraph(const VulkanHIP::Grid3D& grid);

    // Visualization helpers
    void generateRuleReport(const AnalysisResult& result, const std::string& outputPath);
    void generatePatternReport(const std::vector<PatternType>& patterns, const std::string& outputPath);
    void generateComparisonReport(const std::vector<AnalysisResult>& results, const std::string& outputPath);
    void generatePatternInteractionGraph(const std::map<std::string, std::vector<std::string>>& dependencies, const std::string& outputPath);
    void generateRuleSpaceMap(const std::vector<AnalysisResult>& results, const std::string& outputPath);

private:
    // Helper methods
    bool isStable(const VulkanHIP::Grid3D& grid, int generations);
    bool isOscillator(const VulkanHIP::Grid3D& grid, int maxPeriod);
    bool isSpaceship(const VulkanHIP::Grid3D& grid);
    float calculateEntropy(const VulkanHIP::Grid3D& grid);
    std::string categorizePattern(const VulkanHIP::Grid3D& grid);
    
    // Advanced analysis helpers
    float calculatePatternInteractionScore(const VulkanHIP::Grid3D& grid, const std::string& pattern);
    std::vector<std::string> predictNextPatterns(const VulkanHIP::Grid3D& grid);
    float calculatePatternStabilityScore(const VulkanHIP::Grid3D& grid, const std::string& pattern);
    std::vector<std::string> findDependentPatterns(const VulkanHIP::Grid3D& grid, const std::string& pattern);
    
    // Internal state
    std::unique_ptr<VulkanHIP::Grid3D> testGrid_;
    static constexpr int DEFAULT_ANALYSIS_GENERATIONS = 100;
    static constexpr int MAX_OSCILLATOR_PERIOD = 20;
    static constexpr int PATTERN_INTERACTION_RADIUS = 5;
    static constexpr int EVOLUTION_PREDICTION_STEPS = 10;
}; 
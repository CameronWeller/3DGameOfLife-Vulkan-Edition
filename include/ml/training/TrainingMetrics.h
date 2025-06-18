#pragma once

#include <vector>
#include <chrono>
#include <string>
#include <unordered_map>

namespace ML {

struct TrainingMetrics {
    // Current training state
    int currentEpoch = 0;
    float currentLoss = 0.0f;
    float validationLoss = 0.0f;
    float accuracy = 0.0f;
    float learningRate = 0.001f;
    int samplesProcessed = 0;
    
    // Timing metrics
    std::chrono::milliseconds trainingTime{0};
    std::chrono::milliseconds inferenceTime{0};
    std::chrono::milliseconds dataProcessingTime{0};
    
    // History
    std::vector<float> lossHistory;
    std::vector<float> accuracyHistory;
    std::vector<float> validationHistory;
    std::vector<float> learningRateHistory;
    
    // Performance metrics
    float gpuUtilization = 0.0f;
    float memoryUtilization = 0.0f;
    float dataThroughput = 0.0f;
    
    // Pattern-specific metrics
    std::unordered_map<std::string, float> patternAccuracies;
    std::unordered_map<std::string, float> patternConfidences;
    
    // User interaction metrics
    float interactionPredictionAccuracy = 0.0f;
    float ruleOptimizationScore = 0.0f;
    float visualAttentionAccuracy = 0.0f;
    
    // System metrics
    size_t totalMemoryUsed = 0;
    size_t peakMemoryUsed = 0;
    int numBatchesProcessed = 0;
    
    // Clear all metrics
    void reset() {
        currentEpoch = 0;
        currentLoss = 0.0f;
        validationLoss = 0.0f;
        accuracy = 0.0f;
        learningRate = 0.001f;
        samplesProcessed = 0;
        trainingTime = std::chrono::milliseconds{0};
        inferenceTime = std::chrono::milliseconds{0};
        dataProcessingTime = std::chrono::milliseconds{0};
        lossHistory.clear();
        accuracyHistory.clear();
        validationHistory.clear();
        learningRateHistory.clear();
        gpuUtilization = 0.0f;
        memoryUtilization = 0.0f;
        dataThroughput = 0.0f;
        patternAccuracies.clear();
        patternConfidences.clear();
        interactionPredictionAccuracy = 0.0f;
        ruleOptimizationScore = 0.0f;
        visualAttentionAccuracy = 0.0f;
        totalMemoryUsed = 0;
        peakMemoryUsed = 0;
        numBatchesProcessed = 0;
    }
    
    // Add current metrics to history
    void updateHistory() {
        lossHistory.push_back(currentLoss);
        accuracyHistory.push_back(accuracy);
        validationHistory.push_back(validationLoss);
        learningRateHistory.push_back(learningRate);
        
        // Keep history manageable
        const size_t maxHistorySize = 1000;
        if (lossHistory.size() > maxHistorySize) {
            lossHistory.erase(lossHistory.begin());
            accuracyHistory.erase(accuracyHistory.begin());
            validationHistory.erase(validationHistory.begin());
            learningRateHistory.erase(learningRateHistory.begin());
        }
    }
};

} // namespace ML 
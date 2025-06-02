#pragma once

#include <string>

namespace ML {

enum class ModelType {
    CONVOLUTIONAL_3D,
    RECURRENT_LSTM,
    TRANSFORMER,
    REINFORCEMENT_LEARNING,
    HYBRID_ENSEMBLE
};

enum class TrainingMode {
    PATTERN_RECOGNITION,
    BEHAVIOR_PREDICTION,
    RULE_OPTIMIZATION,
    VISUAL_ATTENTION,
    PERFORMANCE_OPTIMIZATION
};

struct TrainingConfig {
    // Model configuration
    ModelType modelType = ModelType::CONVOLUTIONAL_3D;
    TrainingMode mode = TrainingMode::PATTERN_RECOGNITION;
    
    // Training parameters
    float learningRate = 0.001f;
    int batchSize = 32;
    int maxEpochs = 1000;
    int validationInterval = 10;
    float validationSplit = 0.2f;
    
    // Feature configuration
    bool useSpatialFeatures = true;
    bool useTemporalFeatures = true;
    bool usePatternFeatures = true;
    bool useUserInteractionFeatures = true;
    
    // Training options
    bool enableRealTimeTraining = true;
    bool enableVisualization = true;
    bool saveIntermediateModels = true;
    bool useDataAugmentation = true;
    
    // Paths
    std::string modelSavePath = "models/";
    std::string logPath = "logs/training/";
    std::string dataPath = "data/";
    
    // Advanced options
    int numThreads = 4;
    bool useGPU = true;
    int gpuDeviceId = 0;
    float earlyStoppingPatience = 10.0f;
    float earlyStoppingThreshold = 0.001f;
    
    // Memory management
    size_t maxTrainingSamples = 10000;
    size_t maxValidationSamples = 2000;
    size_t maxPredictionQueue = 1000;
};

} // namespace ML 
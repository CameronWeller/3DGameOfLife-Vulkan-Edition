#pragma once

#include <memory>
#include <vector>
#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <unordered_map>
#include <queue>
#include "RuleAnalyzer.h"
#include "VoxelData.h"
#include "UI.h"

// Forward declarations
namespace VulkanHIP {
    class Grid3D;
}
class VulkanEngine;

class VisualFeedbackTrainingLoop {
public:
    enum class TrainingMode {
        PATTERN_RECOGNITION,    // Learn to recognize patterns in 3D space
        BEHAVIOR_PREDICTION,    // Predict user behavior from interactions
        RULE_OPTIMIZATION,      // Optimize cellular automata rules
        VISUAL_ATTENTION,       // Learn visual attention patterns
        PERFORMANCE_OPTIMIZATION // Optimize rendering/simulation performance
    };

    enum class ModelType {
        CONVOLUTIONAL_3D,      // 3D CNN for pattern recognition
        RECURRENT_LSTM,        // LSTM for sequence prediction  
        TRANSFORMER,           // Transformer for attention modeling
        REINFORCEMENT_LEARNING, // RL for optimization tasks
        HYBRID_ENSEMBLE        // Combination of multiple models
    };

    struct TrainingConfig {
        TrainingMode mode = TrainingMode::PATTERN_RECOGNITION;
        ModelType modelType = ModelType::CONVOLUTIONAL_3D;
        float learningRate = 0.001f;
        int batchSize = 32;
        int maxEpochs = 1000;
        int validationInterval = 10;
        float validationSplit = 0.2f;
        bool enableRealTimeTraining = true;
        bool enableVisualization = true;
        bool saveIntermediateModels = true;
        std::string modelSavePath = "models/";
        std::string logPath = "logs/training/";
    };

    struct TrainingMetrics {
        int currentEpoch = 0;
        float currentLoss = 0.0f;
        float validationLoss = 0.0f;
        float accuracy = 0.0f;
        float learningRate = 0.001f;
        std::chrono::milliseconds trainingTime{0};
        std::chrono::milliseconds inferenceTime{0};
        int samplesProcessed = 0;
        std::vector<float> lossHistory;
        std::vector<float> accuracyHistory;
        std::vector<float> validationHistory;
    };

    struct VisualState {
        VoxelData voxelData;
        glm::vec3 cameraPosition;
        glm::vec3 cameraTarget;
        float timestamp;
        uint32_t generation;
        uint32_t population;
        std::vector<RuleAnalyzer::PatternType> detectedPatterns;
        std::unordered_map<std::string, float> patternConfidences;
        std::vector<uint8_t> screenshot; // Raw image data
        int screenWidth;
        int screenHeight;
    };

    struct PredictionResult {
        std::vector<std::pair<std::string, float>> patternPredictions;
        std::vector<glm::vec3> suggestedActions; // Suggested voxel placements
        float confidence;
        std::string modelVersion;
        std::chrono::milliseconds inferenceTime;
    };

    // Callback types
    using OnTrainingUpdateCallback = std::function<void(const TrainingMetrics&)>;
    using OnPredictionCallback = std::function<void(const PredictionResult&)>;
    using OnVisualStateCallback = std::function<void(const VisualState&)>;

public:
    explicit VisualFeedbackTrainingLoop(VulkanEngine* engine);
    ~VisualFeedbackTrainingLoop();

    // Main control methods
    bool initialize(const TrainingConfig& config);
    void startTraining();
    void stopTraining();
    void pauseTraining();
    void resumeTraining();
    bool isTraining() const { return isTraining_.load(); }

    // Data collection
    void captureVisualState();
    void addUserInteraction(const glm::vec3& position, const std::string& action);
    void addPatternObservation(const std::vector<RuleAnalyzer::PatternType>& patterns);

    // Model management
    bool loadModel(const std::string& modelPath);
    bool saveModel(const std::string& modelPath);
    void resetModel();
    std::string getModelInfo() const;

    // Inference
    PredictionResult predictNextPatterns(const VisualState& state);
    std::vector<glm::vec3> suggestOptimalPlacements(const VoxelData& currentState);
    float evaluateRuleSet(const GameRules::RuleSet& rules);

    // Real-time feedback
    void enableRealTimeFeedback(bool enable);
    void setFeedbackCallback(OnPredictionCallback callback);
    void setTrainingUpdateCallback(OnTrainingUpdateCallback callback);
    void setVisualStateCallback(OnVisualStateCallback callback);

    // Training data management
    void exportTrainingData(const std::string& format, const std::string& outputPath);
    void importTrainingData(const std::string& dataPath);
    void augmentTrainingData(); // Generate synthetic training samples
    void clearTrainingData();

    // Metrics and monitoring
    TrainingMetrics getTrainingMetrics() const;
    std::vector<VisualState> getRecentStates(int count = 100) const;
    void generateTrainingReport(const std::string& outputPath);

    // Visualization
    void renderTrainingProgress();
    void renderPredictionVisualization();
    void renderModelArchitecture();

    // Configuration
    void updateConfig(const TrainingConfig& config);
    TrainingConfig getConfig() const { return config_; }

private:
    // Core components
    VulkanEngine* engine_;
    std::unique_ptr<RuleAnalyzer> ruleAnalyzer_;
    
    // Training state
    TrainingConfig config_;
    TrainingMetrics metrics_;
    std::atomic<bool> isTraining_{false};
    std::atomic<bool> isPaused_{false};
    std::atomic<bool> shouldStop_{false};
    
    // Threading
    std::unique_ptr<std::thread> trainingThread_;
    std::unique_ptr<std::thread> dataCollectionThread_;
    std::mutex dataMutex_;
    std::condition_variable dataCondition_;
    
    // Data storage
    std::queue<VisualState> visualStates_;
    std::queue<PredictionResult> predictions_;
    std::vector<VisualState> trainingDataset_;
    std::vector<VisualState> validationDataset_;
    
    // Callbacks
    OnTrainingUpdateCallback trainingUpdateCallback_;
    OnPredictionCallback predictionCallback_;
    OnVisualStateCallback visualStateCallback_;
    
    // Performance tracking
    std::chrono::high_resolution_clock::time_point lastUpdate_;
    std::chrono::high_resolution_clock::time_point trainingStartTime_;
    
    // Model state
    std::string currentModelPath_;
    std::string modelVersion_;
    bool modelLoaded_ = false;
    
    // Internal methods
    void trainingThreadFunction();
    void dataCollectionThreadFunction();
    void processTrainingBatch();
    void validateModel();
    void updateMetrics();
    void optimizeHyperparameters();
    
    // Data preprocessing
    void preprocessVisualState(VisualState& state);
    void normalizeData();
    void augmentData(const VisualState& originalState, std::vector<VisualState>& augmentedStates);
    
    // Model operations (interfaces to actual ML frameworks)
    bool initializeModel();
    bool trainStep(const std::vector<VisualState>& batch);
    float evaluateModel(const std::vector<VisualState>& validationData);
    PredictionResult runInference(const VisualState& state);
    
    // Utilities
    void saveCheckpoint();
    void loadCheckpoint();
    void logTrainingProgress();
    VisualState createVisualStateFromEngine();
    void splitDataset();
    
    // Constants
    static constexpr int MAX_VISUAL_STATES = 10000;
    static constexpr int MAX_PREDICTIONS = 1000;
    static constexpr float MIN_CONFIDENCE_THRESHOLD = 0.5f;
    static constexpr int CHECKPOINT_INTERVAL = 100; // epochs
}; 
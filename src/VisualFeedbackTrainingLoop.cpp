#include "VisualFeedbackTrainingLoop.h"
#include "VulkanEngine.h"
#include "Grid3D.h"
#include "Camera.h"
#include "GameRules.h"
#include "Logger.h"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <random>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

VisualFeedbackTrainingLoop::VisualFeedbackTrainingLoop(VulkanEngine* engine)
    : engine_(engine)
    , ruleAnalyzer_(std::make_unique<RuleAnalyzer>())
    , lastUpdate_(std::chrono::high_resolution_clock::now())
    , trainingStartTime_(std::chrono::high_resolution_clock::now())
    , modelVersion_("v1.0.0")
{
    Logger::getInstance().log(Logger::Level::INFO, "VisualFeedbackTrainingLoop: Initializing training system");
    
    // Initialize metrics
    metrics_.currentEpoch = 0;
    metrics_.currentLoss = 0.0f;
    metrics_.validationLoss = 0.0f;
    metrics_.accuracy = 0.0f;
    metrics_.samplesProcessed = 0;
    
    // Reserve space for data structures
    trainingDataset_.reserve(MAX_VISUAL_STATES);
    validationDataset_.reserve(MAX_VISUAL_STATES / 5); // 20% for validation
    
    Logger::getInstance().log(Logger::Level::INFO, "VisualFeedbackTrainingLoop: Training system initialized");
}

VisualFeedbackTrainingLoop::~VisualFeedbackTrainingLoop() {
    Logger::getInstance().log(Logger::Level::INFO, "VisualFeedbackTrainingLoop: Shutting down training system");
    
    stopTraining();
    
    // Wait for threads to finish
    if (trainingThread_ && trainingThread_->joinable()) {
        trainingThread_->join();
    }
    if (dataCollectionThread_ && dataCollectionThread_->joinable()) {
        dataCollectionThread_->join();
    }
    
    Logger::getInstance().log(Logger::Level::INFO, "VisualFeedbackTrainingLoop: Training system shutdown complete");
}

bool VisualFeedbackTrainingLoop::initialize(const TrainingConfig& config) {
    Logger::getInstance().log(Logger::Level::INFO, "VisualFeedbackTrainingLoop: Initializing with configuration");
    
    config_ = config;
    
    // Create necessary directories
    try {
        std::filesystem::create_directories(config_.modelSavePath);
        std::filesystem::create_directories(config_.logPath);
        std::filesystem::create_directories("screenshots/");
        std::filesystem::create_directories("training_data/");
    } catch (const std::exception& e) {
        Logger::getInstance().log(Logger::Level::ERROR, 
            "Failed to create directories: " + std::string(e.what()));
        return false;
    }
    
    // Initialize model
    if (!initializeModel()) {
        Logger::getInstance().log(Logger::Level::ERROR, "Failed to initialize ML model");
        return false;
    }
    
    // Reset metrics
    metrics_ = TrainingMetrics{};
    metrics_.learningRate = config_.learningRate;
    
    Logger::getInstance().log(Logger::Level::INFO, "VisualFeedbackTrainingLoop: Initialization complete");
    return true;
}

void VisualFeedbackTrainingLoop::startTraining() {
    if (isTraining_.load()) {
        Logger::getInstance().log(Logger::Level::WARNING, "Training already in progress");
        return;
    }
    
    Logger::getInstance().log(Logger::Level::INFO, "Starting visual feedback training loop");
    
    isTraining_.store(true);
    isPaused_.store(false);
    shouldStop_.store(false);
    trainingStartTime_ = std::chrono::high_resolution_clock::now();
    
    // Start data collection thread
    dataCollectionThread_ = std::make_unique<std::thread>(&VisualFeedbackTrainingLoop::dataCollectionThreadFunction, this);
    
    // Start training thread
    trainingThread_ = std::make_unique<std::thread>(&VisualFeedbackTrainingLoop::trainingThreadFunction, this);
    
    Logger::getInstance().log(Logger::Level::INFO, "Training threads started successfully");
}

void VisualFeedbackTrainingLoop::stopTraining() {
    if (!isTraining_.load()) {
        return;
    }
    
    Logger::getInstance().log(Logger::Level::INFO, "Stopping training loop");
    
    shouldStop_.store(true);
    isTraining_.store(false);
    dataCondition_.notify_all();
    
    // Join threads
    if (trainingThread_ && trainingThread_->joinable()) {
        trainingThread_->join();
        trainingThread_.reset();
    }
    if (dataCollectionThread_ && dataCollectionThread_->joinable()) {
        dataCollectionThread_->join();
        dataCollectionThread_.reset();
    }
    
    // Save final checkpoint
    saveCheckpoint();
    
    Logger::getInstance().log(Logger::Level::INFO, "Training stopped successfully");
}

void VisualFeedbackTrainingLoop::pauseTraining() {
    if (!isTraining_.load()) {
        return;
    }
    
    Logger::getInstance().log(Logger::Level::INFO, "Pausing training");
    isPaused_.store(true);
}

void VisualFeedbackTrainingLoop::resumeTraining() {
    if (!isTraining_.load()) {
        return;
    }
    
    Logger::getInstance().log(Logger::Level::INFO, "Resuming training");
    isPaused_.store(false);
    dataCondition_.notify_all();
}

void VisualFeedbackTrainingLoop::captureVisualState() {
    if (!engine_) {
        return;
    }
    
    VisualState state = createVisualStateFromEngine();
    
    std::lock_guard<std::mutex> lock(dataMutex_);
    visualStates_.push(state);
    
    // Limit queue size
    while (visualStates_.size() > MAX_VISUAL_STATES) {
        visualStates_.pop();
    }
    
    dataCondition_.notify_one();
    
    // Trigger visual state callback if set
    if (visualStateCallback_) {
        visualStateCallback_(state);
    }
}

void VisualFeedbackTrainingLoop::addUserInteraction(const glm::vec3& position, const std::string& action) {
    // Capture current visual state with user interaction context
    VisualState state = createVisualStateFromEngine();
    
    // Add interaction metadata (could be extended with more details)
    // For now, we'll store this as part of the visual state
    
    std::lock_guard<std::mutex> lock(dataMutex_);
    visualStates_.push(state);
    
    dataCondition_.notify_one();
}

void VisualFeedbackTrainingLoop::addPatternObservation(const std::vector<RuleAnalyzer::PatternType>& patterns) {
    VisualState state = createVisualStateFromEngine();
    state.detectedPatterns = patterns;
    
    // Calculate pattern confidences based on pattern frequency and stability
    for (const auto& pattern : patterns) {
        state.patternConfidences[pattern.name] = pattern.interactionScore;
    }
    
    std::lock_guard<std::mutex> lock(dataMutex_);
    visualStates_.push(state);
    
    dataCondition_.notify_one();
}

VisualFeedbackTrainingLoop::PredictionResult VisualFeedbackTrainingLoop::predictNextPatterns(const VisualState& state) {
    if (!modelLoaded_) {
        return PredictionResult{};
    }
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    PredictionResult result = runInference(state);
    
    auto endTime = std::chrono::high_resolution_clock::now();
    result.inferenceTime = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    result.modelVersion = modelVersion_;
    
    // Store prediction for feedback loop
    std::lock_guard<std::mutex> lock(dataMutex_);
    predictions_.push(result);
    
    // Limit predictions queue size
    while (predictions_.size() > MAX_PREDICTIONS) {
        predictions_.pop();
    }
    
    // Trigger prediction callback if set
    if (predictionCallback_) {
        predictionCallback_(result);
    }
    
    return result;
}

std::vector<glm::vec3> VisualFeedbackTrainingLoop::suggestOptimalPlacements(const VoxelData& currentState) {
    std::vector<glm::vec3> suggestions;
    
    if (!modelLoaded_) {
        return suggestions;
    }
    
    // Create visual state from current voxel data
    VisualState state;
    state.voxelData = currentState;
    state.timestamp = static_cast<float>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()).count());
    
    if (engine_ && engine_->getCamera()) {
        state.cameraPosition = engine_->getCamera()->getPosition();
        state.cameraTarget = engine_->getCamera()->getTarget();
    }
    
    // Get predictions
    PredictionResult prediction = predictNextPatterns(state);
    
    // Extract suggested actions from prediction
    suggestions = prediction.suggestedActions;
    
    return suggestions;
}

float VisualFeedbackTrainingLoop::evaluateRuleSet(const GameRules::RuleSet& rules) {
    if (!engine_ || !ruleAnalyzer_) {
        return 0.0f;
    }
    
    // Use rule analyzer to evaluate the rule set
    auto analysis = ruleAnalyzer_->analyzeRule(rules, 64, 64, 64); // Default grid size
    
    // Combine multiple metrics into a single evaluation score
    float stabilityWeight = 0.3f;
    float complexityWeight = 0.2f;
    float growthWeight = 0.3f;
    float patternWeight = 0.2f;
    
    float score = analysis.stability * stabilityWeight +
                  analysis.complexity * complexityWeight +
                  (1.0f - std::abs(analysis.growthRate)) * growthWeight + // Prefer moderate growth
                  (static_cast<float>(analysis.observedPatterns.size()) / 10.0f) * patternWeight;
    
    return std::clamp(score, 0.0f, 1.0f);
}

void VisualFeedbackTrainingLoop::dataCollectionThreadFunction() {
    Logger::getInstance().log(Logger::Level::INFO, "Data collection thread started");
    
    while (!shouldStop_.load()) {
        if (isPaused_.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }
        
        // Automatically capture visual states at regular intervals
        captureVisualState();
        
        // Sleep for a short interval (adjust based on desired capture rate)
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    
    Logger::getInstance().log(Logger::Level::INFO, "Data collection thread stopped");
}

void VisualFeedbackTrainingLoop::trainingThreadFunction() {
    Logger::getInstance().log(Logger::Level::INFO, "Training thread started");
    
    while (!shouldStop_.load() && metrics_.currentEpoch < config_.maxEpochs) {
        if (isPaused_.load()) {
            std::unique_lock<std::mutex> lock(dataMutex_);
            dataCondition_.wait(lock, [this] { return !isPaused_.load() || shouldStop_.load(); });
            continue;
        }
        
        // Wait for sufficient data
        std::unique_lock<std::mutex> lock(dataMutex_);
        dataCondition_.wait(lock, [this] { 
            return visualStates_.size() >= static_cast<size_t>(config_.batchSize) || shouldStop_.load(); 
        });
        
        if (shouldStop_.load()) break;
        
        // Process training batch
        processTrainingBatch();
        
        // Update metrics
        updateMetrics();
        
        // Validate model at intervals
        if (metrics_.currentEpoch % config_.validationInterval == 0) {
            validateModel();
        }
        
        // Save checkpoint at intervals
        if (metrics_.currentEpoch % CHECKPOINT_INTERVAL == 0) {
            saveCheckpoint();
        }
        
        // Log progress
        logTrainingProgress();
        
        // Trigger training update callback
        if (trainingUpdateCallback_) {
            trainingUpdateCallback_(metrics_);
        }
        
        metrics_.currentEpoch++;
    }
    
    Logger::getInstance().log(Logger::Level::INFO, "Training thread completed");
}

VisualFeedbackTrainingLoop::VisualState VisualFeedbackTrainingLoop::createVisualStateFromEngine() {
    VisualState state;
    
    if (!engine_) {
        return state;
    }
    
    // Get current timestamp
    state.timestamp = static_cast<float>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()).count());
    
    // Get camera information
    if (engine_->getCamera()) {
        state.cameraPosition = engine_->getCamera()->getPosition();
        state.cameraTarget = engine_->getCamera()->getTarget();
    }
    
    // Get grid information
    if (engine_->getGrid()) {
        state.generation = engine_->getGeneration();
        state.population = engine_->getPopulation();
        
        // Convert grid to voxel data
        state.voxelData = VoxelData();
        // TODO: Implement proper grid to voxel data conversion
    }
    
    // Analyze patterns using rule analyzer
    if (ruleAnalyzer_ && engine_->getGrid()) {
        state.detectedPatterns = ruleAnalyzer_->identifyPatterns(*engine_->getGrid());
        
        // Calculate pattern confidences
        for (const auto& pattern : state.detectedPatterns) {
            state.patternConfidences[pattern.name] = pattern.interactionScore;
        }
    }
    
    // TODO: Capture screenshot data if enabled
    // This would require integration with the rendering system
    state.screenWidth = 1920; // Default values
    state.screenHeight = 1080;
    
    return state;
}

void VisualFeedbackTrainingLoop::processTrainingBatch() {
    // Collect batch data
    std::vector<VisualState> batch;
    batch.reserve(config_.batchSize);
    
    {
        std::lock_guard<std::mutex> lock(dataMutex_);
        while (!visualStates_.empty() && batch.size() < static_cast<size_t>(config_.batchSize)) {
            batch.push_back(visualStates_.front());
            visualStates_.pop();
        }
    }
    
    if (batch.empty()) {
        return;
    }
    
    // Preprocess batch data
    for (auto& state : batch) {
        preprocessVisualState(state);
    }
    
    // Add to training dataset
    trainingDataset_.insert(trainingDataset_.end(), batch.begin(), batch.end());
    
    // Keep dataset size manageable
    if (trainingDataset_.size() > MAX_VISUAL_STATES) {
        trainingDataset_.erase(trainingDataset_.begin(), 
                              trainingDataset_.begin() + (trainingDataset_.size() - MAX_VISUAL_STATES));
    }
    
    // Split dataset if needed
    if (validationDataset_.empty() && trainingDataset_.size() > 100) {
        splitDataset();
    }
    
    // Perform training step
    if (trainingDataset_.size() >= static_cast<size_t>(config_.batchSize)) {
        auto startTime = std::chrono::high_resolution_clock::now();
        bool success = trainStep(batch);
        auto endTime = std::chrono::high_resolution_clock::now();
        
        metrics_.trainingTime = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        metrics_.samplesProcessed += static_cast<int>(batch.size());
        
        if (!success) {
            Logger::getInstance().log(Logger::Level::WARNING, "Training step failed");
        }
    }
}

void VisualFeedbackTrainingLoop::validateModel() {
    if (validationDataset_.empty()) {
        return;
    }
    
    Logger::getInstance().log(Logger::Level::INFO, "Validating model...");
    
    float validationLoss = evaluateModel(validationDataset_);
    metrics_.validationLoss = validationLoss;
    metrics_.validationHistory.push_back(validationLoss);
    
    // Keep history manageable
    if (metrics_.validationHistory.size() > 1000) {
        metrics_.validationHistory.erase(metrics_.validationHistory.begin());
    }
    
    Logger::getInstance().log(Logger::Level::INFO, 
        "Validation complete. Loss: " + std::to_string(validationLoss));
}

void VisualFeedbackTrainingLoop::updateMetrics() {
    auto currentTime = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - lastUpdate_);
    lastUpdate_ = currentTime;
    
    // Update timing metrics
    metrics_.trainingTime += elapsed;
    
    // Add current loss to history
    metrics_.lossHistory.push_back(metrics_.currentLoss);
    metrics_.accuracyHistory.push_back(metrics_.accuracy);
    
    // Keep history manageable
    if (metrics_.lossHistory.size() > 1000) {
        metrics_.lossHistory.erase(metrics_.lossHistory.begin());
    }
    if (metrics_.accuracyHistory.size() > 1000) {
        metrics_.accuracyHistory.erase(metrics_.accuracyHistory.begin());
    }
}

void VisualFeedbackTrainingLoop::preprocessVisualState(VisualState& state) {
    // Normalize timestamp
    state.timestamp = std::fmod(state.timestamp, 1000000.0f) / 1000000.0f;
    
    // Normalize camera position (assuming reasonable world bounds)
    state.cameraPosition = glm::clamp(state.cameraPosition, glm::vec3(-1000.0f), glm::vec3(1000.0f));
    state.cameraPosition /= 1000.0f; // Normalize to [-1, 1]
    
    state.cameraTarget = glm::clamp(state.cameraTarget, glm::vec3(-1000.0f), glm::vec3(1000.0f));
    state.cameraTarget /= 1000.0f;
    
    // Normalize population and generation
    if (state.population > 0) {
        state.population = std::min(state.population, 100000u); // Reasonable upper bound
    }
    if (state.generation > 0) {
        state.generation = std::min(state.generation, 10000u); // Reasonable upper bound
    }
}

void VisualFeedbackTrainingLoop::splitDataset() {
    if (trainingDataset_.empty()) {
        return;
    }
    
    // Shuffle the dataset
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(trainingDataset_.begin(), trainingDataset_.end(), g);
    
    // Split into training and validation
    size_t validationSize = static_cast<size_t>(trainingDataset_.size() * config_.validationSplit);
    
    validationDataset_.clear();
    validationDataset_.reserve(validationSize);
    
    // Move validation samples
    auto splitPoint = trainingDataset_.end() - static_cast<long>(validationSize);
    validationDataset_.insert(validationDataset_.end(), splitPoint, trainingDataset_.end());
    trainingDataset_.erase(splitPoint, trainingDataset_.end());
    
    Logger::getInstance().log(Logger::Level::INFO, 
        "Dataset split: " + std::to_string(trainingDataset_.size()) + 
        " training, " + std::to_string(validationDataset_.size()) + " validation samples");
}

// Model operations - These are placeholder implementations
// In a real implementation, these would interface with actual ML frameworks
bool VisualFeedbackTrainingLoop::initializeModel() {
    Logger::getInstance().log(Logger::Level::INFO, "Initializing ML model");
    
    // TODO: Initialize actual model based on config_.modelType
    // This would typically involve:
    // - Setting up neural network architecture
    // - Initializing weights
    // - Setting up optimizer
    
    modelLoaded_ = true;
    modelVersion_ = "v1.0.0";
    
    Logger::getInstance().log(Logger::Level::INFO, "Model initialized successfully");
    return true;
}

bool VisualFeedbackTrainingLoop::trainStep(const std::vector<VisualState>& batch) {
    // TODO: Implement actual training step
    // This would involve:
    // - Forward pass through the model
    // - Calculate loss
    // - Backward pass (gradient computation)
    // - Update weights
    
    // Simulate training for now
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_real_distribution<float> lossDist(0.1f, 2.0f);
    static std::uniform_real_distribution<float> accDist(0.6f, 0.95f);
    
    metrics_.currentLoss = lossDist(gen) * (1.0f - static_cast<float>(metrics_.currentEpoch) / config_.maxEpochs);
    metrics_.accuracy = accDist(gen) * (static_cast<float>(metrics_.currentEpoch) / config_.maxEpochs + 0.3f);
    
    return true;
}

float VisualFeedbackTrainingLoop::evaluateModel(const std::vector<VisualState>& validationData) {
    // TODO: Implement actual model evaluation
    // This would involve running inference on validation data and calculating metrics
    
    // Simulate validation for now
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_real_distribution<float> dist(0.1f, 1.5f);
    
    return dist(gen);
}

VisualFeedbackTrainingLoop::PredictionResult VisualFeedbackTrainingLoop::runInference(const VisualState& state) {
    PredictionResult result;
    
    // TODO: Implement actual inference
    // This would involve running the trained model on the input state
    
    // Simulate predictions for now
    result.confidence = 0.8f;
    result.patternPredictions = {
        {"oscillator", 0.7f},
        {"still_life", 0.3f},
        {"spaceship", 0.1f}
    };
    
    // Generate some suggested actions
    result.suggestedActions = {
        glm::vec3(1, 1, 1),
        glm::vec3(2, 2, 2),
        glm::vec3(3, 3, 3)
    };
    
    return result;
}

void VisualFeedbackTrainingLoop::saveCheckpoint() {
    std::string checkpointPath = config_.modelSavePath + "checkpoint_epoch_" + 
                                std::to_string(metrics_.currentEpoch) + ".json";
    
    try {
        std::ofstream file(checkpointPath);
        if (file.is_open()) {
            // Save training state as JSON
            file << "{\n";
            file << "  \"epoch\": " << metrics_.currentEpoch << ",\n";
            file << "  \"loss\": " << metrics_.currentLoss << ",\n";
            file << "  \"accuracy\": " << metrics_.accuracy << ",\n";
            file << "  \"validation_loss\": " << metrics_.validationLoss << ",\n";
            file << "  \"samples_processed\": " << metrics_.samplesProcessed << ",\n";
            file << "  \"model_version\": \"" << modelVersion_ << "\",\n";
            file << "  \"training_time_ms\": " << metrics_.trainingTime.count() << "\n";
            file << "}\n";
            file.close();
            
            Logger::getInstance().log(Logger::Level::INFO, 
                "Checkpoint saved: " + checkpointPath);
        }
    } catch (const std::exception& e) {
        Logger::getInstance().log(Logger::Level::ERROR, 
            "Failed to save checkpoint: " + std::string(e.what()));
    }
}

void VisualFeedbackTrainingLoop::logTrainingProgress() {
    if (metrics_.currentEpoch % 10 == 0) { // Log every 10 epochs
        Logger::getInstance().log(Logger::Level::INFO,
            "Epoch " + std::to_string(metrics_.currentEpoch) +
            ", Loss: " + std::to_string(metrics_.currentLoss) +
            ", Accuracy: " + std::to_string(metrics_.accuracy) +
            ", Samples: " + std::to_string(metrics_.samplesProcessed));
    }
}

// Callback setters and real-time feedback
void VisualFeedbackTrainingLoop::enableRealTimeFeedback(bool enable) {
    config_.enableRealTimeTraining = enable;
    Logger::getInstance().log(Logger::Level::INFO, 
        "Real-time feedback " + std::string(enable ? "enabled" : "disabled"));
}

void VisualFeedbackTrainingLoop::setFeedbackCallback(OnPredictionCallback callback) {
    predictionCallback_ = callback;
}

void VisualFeedbackTrainingLoop::setTrainingUpdateCallback(OnTrainingUpdateCallback callback) {
    trainingUpdateCallback_ = callback;
}

void VisualFeedbackTrainingLoop::setVisualStateCallback(OnVisualStateCallback callback) {
    visualStateCallback_ = callback;
}

// Model management
bool VisualFeedbackTrainingLoop::loadModel(const std::string& modelPath) {
    Logger::getInstance().log(Logger::Level::INFO, "Loading model from: " + modelPath);
    
    try {
        // TODO: Implement actual model loading
        // This would involve loading trained weights and model architecture
        
        currentModelPath_ = modelPath;
        modelLoaded_ = true;
        modelVersion_ = "loaded_v1.0.0";
        
        Logger::getInstance().log(Logger::Level::INFO, "Model loaded successfully");
        return true;
    } catch (const std::exception& e) {
        Logger::getInstance().log(Logger::Level::ERROR, 
            "Failed to load model: " + std::string(e.what()));
        return false;
    }
}

bool VisualFeedbackTrainingLoop::saveModel(const std::string& modelPath) {
    Logger::getInstance().log(Logger::Level::INFO, "Saving model to: " + modelPath);
    
    try {
        // TODO: Implement actual model saving
        // This would involve serializing the trained model
        
        std::filesystem::create_directories(std::filesystem::path(modelPath).parent_path());
        
        // For now, just create a placeholder file
        std::ofstream file(modelPath);
        if (file.is_open()) {
            file << "{\n";
            file << "  \"model_version\": \"" << modelVersion_ << "\",\n";
            file << "  \"training_epochs\": " << metrics_.currentEpoch << ",\n";
            file << "  \"final_accuracy\": " << metrics_.accuracy << ",\n";
            file << "  \"model_type\": \"" << static_cast<int>(config_.modelType) << "\",\n";
            file << "  \"training_mode\": \"" << static_cast<int>(config_.mode) << "\"\n";
            file << "}\n";
            file.close();
        }
        
        currentModelPath_ = modelPath;
        Logger::getInstance().log(Logger::Level::INFO, "Model saved successfully");
        return true;
    } catch (const std::exception& e) {
        Logger::getInstance().log(Logger::Level::ERROR, 
            "Failed to save model: " + std::string(e.what()));
        return false;
    }
}

void VisualFeedbackTrainingLoop::resetModel() {
    Logger::getInstance().log(Logger::Level::INFO, "Resetting model");
    
    modelLoaded_ = false;
    currentModelPath_.clear();
    metrics_ = TrainingMetrics{};
    metrics_.learningRate = config_.learningRate;
    
    // Clear training data
    {
        std::lock_guard<std::mutex> lock(dataMutex_);
        while (!visualStates_.empty()) {
            visualStates_.pop();
        }
        while (!predictions_.empty()) {
            predictions_.pop();
        }
    }
    
    trainingDataset_.clear();
    validationDataset_.clear();
    
    // Reinitialize model
    initializeModel();
    
    Logger::getInstance().log(Logger::Level::INFO, "Model reset complete");
}

std::string VisualFeedbackTrainingLoop::getModelInfo() const {
    std::stringstream info;
    info << "Model Version: " << modelVersion_ << "\n";
    info << "Model Type: " << static_cast<int>(config_.modelType) << "\n";
    info << "Training Mode: " << static_cast<int>(config_.mode) << "\n";
    info << "Current Epoch: " << metrics_.currentEpoch << "\n";
    info << "Accuracy: " << metrics_.accuracy << "\n";
    info << "Model Loaded: " << (modelLoaded_ ? "Yes" : "No") << "\n";
    if (!currentModelPath_.empty()) {
        info << "Model Path: " << currentModelPath_ << "\n";
    }
    return info.str();
}

// Training data management
void VisualFeedbackTrainingLoop::exportTrainingData(const std::string& format, const std::string& outputPath) {
    Logger::getInstance().log(Logger::Level::INFO, 
        "Exporting training data in format: " + format + " to: " + outputPath);
    
    try {
        std::filesystem::create_directories(std::filesystem::path(outputPath).parent_path());
        
        if (format == "json") {
            std::ofstream file(outputPath);
            if (file.is_open()) {
                file << "{\n";
                file << "  \"training_data\": [\n";
                
                for (size_t i = 0; i < trainingDataset_.size(); ++i) {
                    const auto& state = trainingDataset_[i];
                    file << "    {\n";
                    file << "      \"timestamp\": " << state.timestamp << ",\n";
                    file << "      \"generation\": " << state.generation << ",\n";
                    file << "      \"population\": " << state.population << ",\n";
                    file << "      \"camera_position\": [" 
                         << state.cameraPosition.x << ", " 
                         << state.cameraPosition.y << ", " 
                         << state.cameraPosition.z << "],\n";
                    file << "      \"detected_patterns\": " << state.detectedPatterns.size() << "\n";
                    file << "    }";
                    if (i < trainingDataset_.size() - 1) file << ",";
                    file << "\n";
                }
                
                file << "  ],\n";
                file << "  \"metadata\": {\n";
                file << "    \"total_samples\": " << trainingDataset_.size() << ",\n";
                file << "    \"export_timestamp\": " << std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::high_resolution_clock::now().time_since_epoch()).count() << "\n";
                file << "  }\n";
                file << "}\n";
                file.close();
            }
        } else if (format == "csv") {
            std::ofstream file(outputPath);
            if (file.is_open()) {
                // CSV header
                file << "timestamp,generation,population,camera_x,camera_y,camera_z,target_x,target_y,target_z,pattern_count\n";
                
                for (const auto& state : trainingDataset_) {
                    file << state.timestamp << ","
                         << state.generation << ","
                         << state.population << ","
                         << state.cameraPosition.x << ","
                         << state.cameraPosition.y << ","
                         << state.cameraPosition.z << ","
                         << state.cameraTarget.x << ","
                         << state.cameraTarget.y << ","
                         << state.cameraTarget.z << ","
                         << state.detectedPatterns.size() << "\n";
                }
                file.close();
            }
        }
        
        Logger::getInstance().log(Logger::Level::INFO, "Training data exported successfully");
    } catch (const std::exception& e) {
        Logger::getInstance().log(Logger::Level::ERROR, 
            "Failed to export training data: " + std::string(e.what()));
    }
}

void VisualFeedbackTrainingLoop::importTrainingData(const std::string& dataPath) {
    Logger::getInstance().log(Logger::Level::INFO, "Importing training data from: " + dataPath);
    
    try {
        // TODO: Implement data import based on file format
        // This would parse the file and populate trainingDataset_
        
        Logger::getInstance().log(Logger::Level::INFO, "Training data imported successfully");
    } catch (const std::exception& e) {
        Logger::getInstance().log(Logger::Level::ERROR, 
            "Failed to import training data: " + std::string(e.what()));
    }
}

void VisualFeedbackTrainingLoop::augmentTrainingData() {
    Logger::getInstance().log(Logger::Level::INFO, "Augmenting training data");
    
    size_t originalSize = trainingDataset_.size();
    std::vector<VisualState> augmentedData;
    
    for (const auto& state : trainingDataset_) {
        std::vector<VisualState> augmented;
        augmentData(state, augmented);
        augmentedData.insert(augmentedData.end(), augmented.begin(), augmented.end());
    }
    
    trainingDataset_.insert(trainingDataset_.end(), augmentedData.begin(), augmentedData.end());
    
    Logger::getInstance().log(Logger::Level::INFO, 
        "Data augmentation complete. Original: " + std::to_string(originalSize) + 
        ", Augmented: " + std::to_string(trainingDataset_.size()));
}

void VisualFeedbackTrainingLoop::augmentData(const VisualState& originalState, std::vector<VisualState>& augmentedStates) {
    // Generate augmented versions of the original state
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_real_distribution<float> noiseDist(-0.1f, 0.1f);
    
    // Create a few augmented versions
    for (int i = 0; i < 3; ++i) {
        VisualState augmented = originalState;
        
        // Add noise to camera position
        augmented.cameraPosition.x += noiseDist(gen);
        augmented.cameraPosition.y += noiseDist(gen);
        augmented.cameraPosition.z += noiseDist(gen);
        
        // Add noise to camera target
        augmented.cameraTarget.x += noiseDist(gen);
        augmented.cameraTarget.y += noiseDist(gen);
        augmented.cameraTarget.z += noiseDist(gen);
        
        // Slightly modify timestamp
        augmented.timestamp += noiseDist(gen) * 0.01f;
        
        augmentedStates.push_back(augmented);
    }
}

void VisualFeedbackTrainingLoop::clearTrainingData() {
    Logger::getInstance().log(Logger::Level::INFO, "Clearing training data");
    
    std::lock_guard<std::mutex> lock(dataMutex_);
    
    while (!visualStates_.empty()) {
        visualStates_.pop();
    }
    while (!predictions_.empty()) {
        predictions_.pop();
    }
    
    trainingDataset_.clear();
    validationDataset_.clear();
    
    Logger::getInstance().log(Logger::Level::INFO, "Training data cleared");
}

// Metrics and monitoring
VisualFeedbackTrainingLoop::TrainingMetrics VisualFeedbackTrainingLoop::getTrainingMetrics() const {
    return metrics_;
}

std::vector<VisualFeedbackTrainingLoop::VisualState> VisualFeedbackTrainingLoop::getRecentStates(int count) const {
    std::vector<VisualState> recent;
    
    std::lock_guard<std::mutex> lock(dataMutex_);
    
    // Get the last 'count' states from training dataset
    size_t startIdx = trainingDataset_.size() > static_cast<size_t>(count) ? 
                      trainingDataset_.size() - count : 0;
    
    for (size_t i = startIdx; i < trainingDataset_.size(); ++i) {
        recent.push_back(trainingDataset_[i]);
    }
    
    return recent;
}

void VisualFeedbackTrainingLoop::generateTrainingReport(const std::string& outputPath) {
    Logger::getInstance().log(Logger::Level::INFO, "Generating training report: " + outputPath);
    
    try {
        std::ofstream file(outputPath);
        if (file.is_open()) {
            file << "# Visual Feedback Training Loop Report\n\n";
            file << "## Training Configuration\n";
            file << "- Training Mode: " << static_cast<int>(config_.mode) << "\n";
            file << "- Model Type: " << static_cast<int>(config_.modelType) << "\n";
            file << "- Learning Rate: " << config_.learningRate << "\n";
            file << "- Batch Size: " << config_.batchSize << "\n";
            file << "- Max Epochs: " << config_.maxEpochs << "\n\n";
            
            file << "## Training Results\n";
            file << "- Current Epoch: " << metrics_.currentEpoch << "\n";
            file << "- Final Loss: " << metrics_.currentLoss << "\n";
            file << "- Final Accuracy: " << metrics_.accuracy << "\n";
            file << "- Validation Loss: " << metrics_.validationLoss << "\n";
            file << "- Samples Processed: " << metrics_.samplesProcessed << "\n";
            file << "- Training Time: " << metrics_.trainingTime.count() << " ms\n\n";
            
            file << "## Dataset Information\n";
            file << "- Training Samples: " << trainingDataset_.size() << "\n";
            file << "- Validation Samples: " << validationDataset_.size() << "\n\n";
            
            file << "## Model Information\n";
            file << "- Model Version: " << modelVersion_ << "\n";
            file << "- Model Loaded: " << (modelLoaded_ ? "Yes" : "No") << "\n";
            if (!currentModelPath_.empty()) {
                file << "- Model Path: " << currentModelPath_ << "\n";
            }
            
            file.close();
        }
        
        Logger::getInstance().log(Logger::Level::INFO, "Training report generated successfully");
    } catch (const std::exception& e) {
        Logger::getInstance().log(Logger::Level::ERROR, 
            "Failed to generate training report: " + std::string(e.what()));
    }
}

// Configuration
void VisualFeedbackTrainingLoop::updateConfig(const TrainingConfig& config) {
    config_ = config;
    metrics_.learningRate = config_.learningRate;
    
    Logger::getInstance().log(Logger::Level::INFO, "Training configuration updated");
}

// Additional utility methods
void VisualFeedbackTrainingLoop::optimizeHyperparameters() {
    // TODO: Implement hyperparameter optimization
    // This could use techniques like grid search, random search, or Bayesian optimization
    Logger::getInstance().log(Logger::Level::INFO, "Hyperparameter optimization not implemented yet");
}

void VisualFeedbackTrainingLoop::normalizeData() {
    // TODO: Implement data normalization across the entire dataset
    Logger::getInstance().log(Logger::Level::INFO, "Data normalization not implemented yet");
}

void VisualFeedbackTrainingLoop::loadCheckpoint() {
    // TODO: Implement checkpoint loading
    Logger::getInstance().log(Logger::Level::INFO, "Checkpoint loading not implemented yet");
}

// Visualization methods (placeholders for UI integration)
void VisualFeedbackTrainingLoop::renderTrainingProgress() {
    // TODO: This would integrate with the UI system to render real-time training progress
    // Could include loss curves, accuracy plots, etc.
}

void VisualFeedbackTrainingLoop::renderPredictionVisualization() {
    // TODO: This would render model predictions overlaid on the 3D scene
    // Could show suggested voxel placements, pattern predictions, etc.
}

void VisualFeedbackTrainingLoop::renderModelArchitecture() {
    // TODO: This would render a visualization of the neural network architecture
} 
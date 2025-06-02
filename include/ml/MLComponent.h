#pragma once

#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <glm/glm.hpp>

namespace ML {

// Forward declarations
class Model;
class Dataset;
class TrainingConfig;
class TrainingMetrics;

class MLComponent {
public:
    MLComponent();
    virtual ~MLComponent();

    // Core ML functionality
    virtual bool initialize(const TrainingConfig& config) = 0;
    virtual bool train(const Dataset& dataset) = 0;
    virtual bool predict(const std::vector<float>& input, std::vector<float>& output) = 0;
    virtual bool saveModel(const std::string& path) = 0;
    virtual bool loadModel(const std::string& path) = 0;

    // Training control
    virtual void startTraining() = 0;
    virtual void stopTraining() = 0;
    virtual void pauseTraining() = 0;
    virtual void resumeTraining() = 0;

    // Status and metrics
    virtual bool isTraining() const = 0;
    virtual bool isPaused() const = 0;
    virtual TrainingMetrics getMetrics() const = 0;

    // Callbacks
    using TrainingCallback = std::function<void(const TrainingMetrics&)>;
    using PredictionCallback = std::function<void(const std::vector<float>&)>;
    
    virtual void setTrainingCallback(TrainingCallback callback) = 0;
    virtual void setPredictionCallback(PredictionCallback callback) = 0;

protected:
    std::unique_ptr<Model> model_;
    TrainingConfig config_;
    TrainingMetrics metrics_;
    bool isTraining_;
    bool isPaused_;
};

} // namespace ML 
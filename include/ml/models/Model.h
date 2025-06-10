#pragma once

#include <vector>
#include <string>
#include <memory>
#include "../training/TrainingConfig.h"

namespace ML {

class Model {
public:
    Model();
    virtual ~Model();
    
    // Model lifecycle
    virtual bool initialize(const TrainingConfig& config) = 0;
    virtual bool train(const std::vector<float>& input, const std::vector<float>& target) = 0;
    virtual bool predict(const std::vector<float>& input, std::vector<float>& output) = 0;
    virtual bool save(const std::string& path) const = 0;
    virtual bool load(const std::string& path) = 0;
    
    // Model information
    virtual ModelType getType() const = 0;
    virtual std::string getName() const = 0;
    virtual size_t getInputSize() const = 0;
    virtual size_t getOutputSize() const = 0;
    
    // Model state
    virtual bool isInitialized() const = 0;
    virtual bool isTraining() const = 0;
    virtual float getLoss() const = 0;
    virtual float getAccuracy() const = 0;
    
    // Model parameters
    virtual void setLearningRate(float rate) = 0;
    virtual float getLearningRate() const = 0;
    virtual void setBatchSize(size_t size) = 0;
    virtual size_t getBatchSize() const = 0;
    
protected:
    TrainingConfig config_;
    bool isInitialized_ = false;
    bool isTraining_ = false;
    float currentLoss_ = 0.0f;
    float currentAccuracy_ = 0.0f;
};

// Factory function for creating models
std::unique_ptr<Model> createModel(ModelType type);

} // namespace ML 
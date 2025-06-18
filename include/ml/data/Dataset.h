#pragma once

#include <vector>
#include <string>
#include <memory>
#include <glm/glm.hpp>

namespace ML {

struct DataPoint {
    // Input features
    std::vector<float> features;
    std::vector<float> spatialFeatures;
    std::vector<float> temporalFeatures;
    std::vector<float> patternFeatures;
    std::vector<float> userInteractionFeatures;
    
    // Target values
    std::vector<float> targets;
    std::vector<float> patternLabels;
    std::vector<float> interactionLabels;
    
    // Metadata
    std::string id;
    std::chrono::system_clock::time_point timestamp;
    bool isValid = true;
    
    // Clear all data
    void reset() {
        features.clear();
        spatialFeatures.clear();
        temporalFeatures.clear();
        patternFeatures.clear();
        userInteractionFeatures.clear();
        targets.clear();
        patternLabels.clear();
        interactionLabels.clear();
        id.clear();
        timestamp = std::chrono::system_clock::time_point{};
        isValid = true;
    }
};

class Dataset {
public:
    Dataset();
    virtual ~Dataset();
    
    // Data management
    virtual bool load(const std::string& path) = 0;
    virtual bool save(const std::string& path) const = 0;
    virtual void clear() = 0;
    
    // Data access
    virtual size_t size() const = 0;
    virtual const DataPoint& get(size_t index) const = 0;
    virtual DataPoint& get(size_t index) = 0;
    
    // Data splitting
    virtual std::unique_ptr<Dataset> split(float ratio) = 0;
    virtual std::unique_ptr<Dataset> getValidationSet(float ratio) = 0;
    
    // Data augmentation
    virtual void augment() = 0;
    virtual void normalize() = 0;
    
    // Batch processing
    virtual std::vector<DataPoint> getBatch(size_t start, size_t size) const = 0;
    virtual void shuffle() = 0;
    
    // Statistics
    virtual void computeStatistics() = 0;
    virtual const std::vector<float>& getFeatureMeans() const = 0;
    virtual const std::vector<float>& getFeatureStds() const = 0;
    
protected:
    std::vector<DataPoint> data_;
    std::vector<float> featureMeans_;
    std::vector<float> featureStds_;
    bool isNormalized_ = false;
};

} // namespace ML 
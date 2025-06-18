# Visual Feedback Training Loop Documentation

## Overview

The Visual Feedback Training Loop is a comprehensive machine learning system integrated into the 3D Game of Life Vulkan Engine. It provides real-time pattern recognition, behavior prediction, and rule optimization capabilities through advanced ML techniques.

## Features

### Core Capabilities
- **Pattern Recognition**: Learn to identify and classify 3D cellular automata patterns
- **Behavior Prediction**: Predict user interactions and system evolution
- **Rule Optimization**: Optimize cellular automata rules for desired outcomes
- **Visual Attention**: Model and predict visual attention patterns
- **Performance Optimization**: Learn to optimize rendering and simulation parameters

### Model Types
- **3D CNN**: Convolutional Neural Networks for spatial pattern recognition
- **LSTM**: Long Short-Term Memory networks for sequence prediction
- **Transformer**: Attention-based models for complex pattern relationships
- **Reinforcement Learning**: For optimization and decision-making tasks
- **Hybrid Ensemble**: Combination of multiple model types

## Getting Started

### 1. Basic Initialization

```cpp
#include "VisualFeedbackTrainingLoop.h"

// Initialize training loop with engine reference
auto trainingLoop = std::make_unique<VisualFeedbackTrainingLoop>(vulkanEngine);

// Configure training parameters
VisualFeedbackTrainingLoop::TrainingConfig config;
config.mode = VisualFeedbackTrainingLoop::TrainingMode::PATTERN_RECOGNITION;
config.modelType = VisualFeedbackTrainingLoop::ModelType::CONVOLUTIONAL_3D;
config.learningRate = 0.001f;
config.batchSize = 32;
config.maxEpochs = 1000;

// Initialize with configuration
bool success = trainingLoop->initialize(config);
```

### 2. Setting Up Callbacks

```cpp
// Training progress callback
trainingLoop->setTrainingUpdateCallback(
    [](const VisualFeedbackTrainingLoop::TrainingMetrics& metrics) {
        std::cout << "Epoch: " << metrics.currentEpoch 
                  << ", Loss: " << metrics.currentLoss 
                  << ", Accuracy: " << metrics.accuracy << std::endl;
    });

// Prediction callback
trainingLoop->setFeedbackCallback(
    [](const VisualFeedbackTrainingLoop::PredictionResult& prediction) {
        std::cout << "Confidence: " << prediction.confidence << std::endl;
        for (const auto& [pattern, confidence] : prediction.patternPredictions) {
            std::cout << "Pattern " << pattern << ": " << confidence << std::endl;
        }
    });

// Visual state callback
trainingLoop->setVisualStateCallback(
    [](const VisualFeedbackTrainingLoop::VisualState& state) {
        std::cout << "Captured state at generation: " << state.generation << std::endl;
    });
```

### 3. Training Control

```cpp
// Start training
trainingLoop->startTraining();

// Monitor training progress
while (trainingLoop->isTraining()) {
    auto metrics = trainingLoop->getTrainingMetrics();
    
    // Check if we should stop based on convergence criteria
    if (metrics.currentLoss < 0.01f || metrics.accuracy > 0.95f) {
        trainingLoop->stopTraining();
        break;
    }
    
    std::this_thread::sleep_for(std::chrono::seconds(1));
}

// Save trained model
trainingLoop->saveModel("models/trained_pattern_recognizer.json");
```

## Configuration Options

### Training Modes

| Mode | Description | Use Case |
|------|-------------|----------|
| `PATTERN_RECOGNITION` | Learn to identify 3D patterns | Classification of cellular structures |
| `BEHAVIOR_PREDICTION` | Predict user actions | UI optimization and assistance |
| `RULE_OPTIMIZATION` | Optimize CA rules | Finding interesting rule sets |
| `VISUAL_ATTENTION` | Model attention patterns | UI layout optimization |
| `PERFORMANCE_OPTIMIZATION` | Optimize system performance | Adaptive quality settings |

### Model Types

| Type | Description | Best For |
|------|-------------|----------|
| `CONVOLUTIONAL_3D` | 3D CNNs for spatial patterns | Pattern recognition in 3D grids |
| `RECURRENT_LSTM` | LSTMs for sequences | Time-series prediction |
| `TRANSFORMER` | Attention mechanisms | Complex pattern relationships |
| `REINFORCEMENT_LEARNING` | RL for optimization | Decision making and control |
| `HYBRID_ENSEMBLE` | Multiple models combined | Maximum accuracy and robustness |

### Training Parameters

```cpp
struct TrainingConfig {
    TrainingMode mode = TrainingMode::PATTERN_RECOGNITION;
    ModelType modelType = ModelType::CONVOLUTIONAL_3D;
    float learningRate = 0.001f;          // Learning rate for optimizer
    int batchSize = 32;                   // Training batch size
    int maxEpochs = 1000;                 // Maximum training epochs
    int validationInterval = 10;          // Validation frequency
    float validationSplit = 0.2f;         // Validation data percentage
    bool enableRealTimeTraining = true;   // Train during simulation
    bool enableVisualization = true;      // Show training progress
    bool saveIntermediateModels = true;   // Save checkpoints
    std::string modelSavePath = "models/"; // Model save directory
    std::string logPath = "logs/training/"; // Log directory
};
```

## Data Management

### Data Collection

```cpp
// Manual data capture
trainingLoop->captureVisualState();

// Add user interaction context
trainingLoop->addUserInteraction(glm::vec3(10, 10, 10), "place_voxel");

// Add pattern observation
std::vector<RuleAnalyzer::PatternType> patterns = analyzer->identifyPatterns(grid);
trainingLoop->addPatternObservation(patterns);
```

### Data Export/Import

```cpp
// Export training data
trainingLoop->exportTrainingData("json", "training_data/export.json");
trainingLoop->exportTrainingData("csv", "training_data/export.csv");

// Import existing data
trainingLoop->importTrainingData("training_data/previous_session.json");

// Data augmentation
trainingLoop->augmentTrainingData(); // Generate synthetic variations
```

### Data Formats

#### JSON Format
```json
{
  "training_data": [
    {
      "timestamp": 1234567890.5,
      "generation": 42,
      "population": 1337,
      "camera_position": [10.0, 15.0, 20.0],
      "detected_patterns": 3
    }
  ],
  "metadata": {
    "total_samples": 1000,
    "export_timestamp": 1234567890123
  }
}
```

#### CSV Format
```csv
timestamp,generation,population,camera_x,camera_y,camera_z,target_x,target_y,target_z,pattern_count
1234567890.5,42,1337,10.0,15.0,20.0,0.0,0.0,0.0,3
```

## Model Management

### Saving and Loading

```cpp
// Save trained model
bool success = trainingLoop->saveModel("models/my_model.json");

// Load existing model
bool loaded = trainingLoop->loadModel("models/my_model.json");

// Reset to untrained state
trainingLoop->resetModel();

// Get model information
std::string info = trainingLoop->getModelInfo();
```

### Model File Format

```json
{
  "model_version": "v1.0.0",
  "training_epochs": 500,
  "final_accuracy": 0.87,
  "model_type": "0",
  "training_mode": "0"
}
```

## Real-time Inference

### Pattern Prediction

```cpp
// Get current visual state
auto currentState = trainingLoop->createVisualStateFromEngine();

// Predict patterns
auto prediction = trainingLoop->predictNextPatterns(currentState);

// Process results
for (const auto& [pattern, confidence] : prediction.patternPredictions) {
    if (confidence > 0.8f) {
        std::cout << "High confidence prediction: " << pattern << std::endl;
    }
}
```

### Optimal Placement Suggestions

```cpp
// Get current voxel data
VoxelData currentData = engine->getVoxelData();

// Get placement suggestions
auto suggestions = trainingLoop->suggestOptimalPlacements(currentData);

// Apply suggestions (optional)
for (const auto& position : suggestions) {
    engine->placeVoxel(position);
}
```

### Rule Evaluation

```cpp
// Evaluate different rule sets
GameRules::RuleSet rule1 = {5, 7, 6, 6}; // Birth 5-7, Survival 6
GameRules::RuleSet rule2 = {4, 5, 5, 5}; // Birth 4-5, Survival 5

float score1 = trainingLoop->evaluateRuleSet(rule1);
float score2 = trainingLoop->evaluateRuleSet(rule2);

if (score1 > score2) {
    engine->setRuleSet(rule1);
} else {
    engine->setRuleSet(rule2);
}
```

## UI Integration

### Training Window Controls

The system provides a comprehensive UI through the ImGui interface:

1. **Training Control Panel**
   - Start/Stop/Pause/Resume buttons
   - Real-time progress indicators
   - Configuration sliders and dropdowns

2. **Metrics Dashboard**
   - Loss and accuracy plots
   - Training progress bars
   - Performance metrics

3. **Model Management**
   - Save/Load model buttons
   - Model information display
   - Data export/import controls

4. **Visualization Panel**
   - Real-time training visualizations
   - Prediction overlays
   - Model architecture diagrams

### Accessing the UI

```cpp
// In your main render loop
ui->render(); // This will include the training window if enabled

// Programmatically show the training window
ui->showTrainingWindow_ = true;
```

## Performance Considerations

### Memory Usage

- Training datasets are limited to 10,000 visual states by default
- Prediction queues are capped at 1,000 results
- Regular cleanup prevents memory leaks

### Threading

- Training runs in a separate thread to avoid blocking the main render loop
- Data collection runs in its own thread for continuous capture
- Thread-safe queues handle data exchange

### Optimization Tips

1. **Batch Size**: Larger batches improve GPU utilization but require more memory
2. **Learning Rate**: Start with 0.001 and adjust based on convergence
3. **Data Augmentation**: Use to improve generalization with limited data
4. **Model Checkpoints**: Save regularly to prevent loss of progress

## Troubleshooting

### Common Issues

1. **Training Not Starting**
   - Ensure model is properly initialized
   - Check that sufficient training data is available
   - Verify GPU memory availability

2. **Poor Training Performance**
   - Increase batch size if memory allows
   - Adjust learning rate (try 0.0001 or 0.01)
   - Add more training data
   - Try data augmentation

3. **Memory Issues**
   - Reduce batch size
   - Clear training data periodically
   - Monitor memory usage in Performance window

4. **Slow Inference**
   - Use smaller model types for real-time applications
   - Optimize model architecture
   - Consider model quantization

### Debug Mode

```cpp
// Enable detailed logging
trainingLoop->setDebugMode(true);

// Monitor training metrics
auto metrics = trainingLoop->getTrainingMetrics();
std::cout << "Loss history size: " << metrics.lossHistory.size() << std::endl;
```

## Integration with User Metrics Tracker

The Visual Feedback Training Loop integrates seamlessly with the User Metrics Tracker system:

```cpp
// Capture user interactions for training
tracker.trackCustomEvent("voxel_placed", {
    {"position", position},
    {"pattern_context", currentPatterns}
});

// Feed interaction data to training loop
trainingLoop->addUserInteraction(position, "voxel_placed");
```

## Advanced Usage

### Custom Model Integration

For advanced users wanting to integrate custom ML frameworks:

```cpp
// Override model operations in derived class
class CustomTrainingLoop : public VisualFeedbackTrainingLoop {
protected:
    bool initializeModel() override {
        // Initialize your custom model here
        return true;
    }
    
    bool trainStep(const std::vector<VisualState>& batch) override {
        // Implement custom training logic
        return true;
    }
    
    PredictionResult runInference(const VisualState& state) override {
        // Implement custom inference
        return PredictionResult{};
    }
};
```

### Multi-Model Ensemble

```cpp
// Train multiple models for different tasks
auto patternRecognizer = std::make_unique<VisualFeedbackTrainingLoop>(engine);
auto behaviorPredictor = std::make_unique<VisualFeedbackTrainingLoop>(engine);

// Configure for different tasks
patternRecognizer->initialize({TrainingMode::PATTERN_RECOGNITION, ModelType::CONVOLUTIONAL_3D});
behaviorPredictor->initialize({TrainingMode::BEHAVIOR_PREDICTION, ModelType::RECURRENT_LSTM});

// Train simultaneously
patternRecognizer->startTraining();
behaviorPredictor->startTraining();
```

## Future Enhancements

The training loop system is designed for extensibility. Planned enhancements include:

- Integration with TensorFlow/PyTorch backends
- Distributed training support
- Advanced visualization capabilities
- Automatic hyperparameter optimization
- Real-time model adaptation
- Integration with cloud ML services

## Example Applications

### 1. Pattern Library Generation

```cpp
// Generate and classify interesting patterns
auto trainingLoop = std::make_unique<VisualFeedbackTrainingLoop>(engine);
trainingLoop->initialize({TrainingMode::PATTERN_RECOGNITION, ModelType::CONVOLUTIONAL_3D});

// Train on known pattern types
trainingLoop->startTraining();

// Use to classify new discoveries
auto prediction = trainingLoop->predictNextPatterns(currentState);
if (prediction.confidence > 0.9f) {
    savePattern("discovered_pattern_" + std::to_string(generation));
}
```

### 2. Adaptive UI

```cpp
// Learn user preferences and adapt interface
auto trainingLoop = std::make_unique<VisualFeedbackTrainingLoop>(engine);
trainingLoop->initialize({TrainingMode::BEHAVIOR_PREDICTION, ModelType::RECURRENT_LSTM});

// Predict what user might want to do next
auto suggestions = trainingLoop->suggestOptimalPlacements(currentState);
ui->showSuggestions(suggestions);
```

### 3. Rule Discovery

```cpp
// Discover interesting rule combinations
auto trainingLoop = std::make_unique<VisualFeedbackTrainingLoop>(engine);
trainingLoop->initialize({TrainingMode::RULE_OPTIMIZATION, ModelType::REINFORCEMENT_LEARNING});

// Evaluate and optimize rules
float bestScore = 0.0f;
GameRules::RuleSet bestRule;

for (auto rule : ruleSpace) {
    float score = trainingLoop->evaluateRuleSet(rule);
    if (score > bestScore) {
        bestScore = score;
        bestRule = rule;
    }
}
``` 
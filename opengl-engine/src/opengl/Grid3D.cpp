#include "opengl/Grid3D.h"
#include "../../../include/GameRules.h"
#include "../../../include/PatternManager.h"
#include "../../../include/Logger.h"
#include "compute/OpenGLCompute.h"
#include "rendering/OpenGLVoxelRenderer.h"
#include <stdexcept>
#include <algorithm>
#include <random>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <ctime>

namespace OpenGLHIP {

// Helper function to map rule set to uint32_t index for GPU
static uint32_t getRuleSetIndex(const GameRules::RuleSet& ruleSet) {
    if (ruleSet.name == "5766") return 4;
    else if (ruleSet.name == "4555") return 5;
    else if (ruleSet.name == "2333") return 0;  // Classic
    else if (ruleSet.name == "3444") return 1;  // HighLife
    else if (ruleSet.name == "6777") return 2;  // Day & Night
    else return 3; // Custom
}

Grid3D::Grid3D(uint32_t width, uint32_t height, uint32_t depth)
    : width_(width), height_(height), depth_(depth),
      generation_(0), population_(0),
      currentRuleSet_(GameRules::RULE_5766),
      rules_(GameRules::RULE_5766),
      boundaryType_(GameRules::BoundaryType::TOROIDAL),
      needsStateSync_(false),
      isInitialized_(false) {
    
    initialize();
}

Grid3D::~Grid3D() {
    cleanup();
}

void Grid3D::initialize() {
    if (isInitialized_) return;
    
    // Initialize state vectors
    currentState_.resize(width_ * height_ * depth_, false);
    nextState_.resize(width_ * height_ * depth_, false);
    
    // Create OpenGL compute resources
    compute_ = std::make_unique<OpenGLCompute>();
    
    // Load compute shader
    std::string shaderPath = "opengl-engine/shaders/compute/game_of_life_3d_opengl.comp";
    if (!std::filesystem::exists(shaderPath)) {
        // Try alternative paths
        shaderPath = "shaders/compute/game_of_life_3d_opengl.comp";
        if (!std::filesystem::exists(shaderPath)) {
            shaderPath = "../opengl-engine/shaders/compute/game_of_life_3d_opengl.comp";
            if (!std::filesystem::exists(shaderPath)) {
                shaderPath = "../../opengl-engine/shaders/compute/game_of_life_3d_opengl.comp";
            }
        }
    }
    
    // Load shader file and compile
    if (!compute_->loadShader(shaderPath)) {
        throw std::runtime_error("Failed to load compute shader from: " + shaderPath);
    }
    
    // Load shader source manually for compilation
    std::ifstream shaderFile(shaderPath);
    if (!shaderFile.is_open()) {
        throw std::runtime_error("Failed to open compute shader file: " + shaderPath);
    }
    
    std::stringstream shaderSource;
    shaderSource << shaderFile.rdbuf();
    shaderFile.close();
    
    // Compile shader
    if (!compute_->compileShader(shaderSource.str())) {
        throw std::runtime_error("Failed to compile compute shader");
    }
    
    // Link program
    if (!compute_->linkProgram()) {
        throw std::runtime_error("Failed to link compute program");
    }
    
    // Create state buffers
    compute_->createStateBuffers(width_, height_, depth_);
    
    // Create uniform buffer for push constants
    compute_->createUniformBuffer();
    
    // Sync initial state to GPU
    syncStateToGPU();
    
    updatePopulation();
    
    isInitialized_ = true;
    VulkanHIP::Logger::getInstance().log(VulkanHIP::Logger::LogLevel::Info,
        "OpenGL Grid3D initialized: " + std::to_string(width_) + "x" + 
        std::to_string(height_) + "x" + std::to_string(depth_));
}

void Grid3D::cleanup() {
    if (!isInitialized_) return;
    
    if (compute_) {
        compute_->destroyStateBuffers();
        compute_->destroyUniformBuffer();
    }
    compute_.reset();
    
    currentState_.clear();
    nextState_.clear();
    
    isInitialized_ = false;
}

void Grid3D::resize(uint32_t width, uint32_t height, uint32_t depth) {
    if (width == width_ && height == height_ && depth == depth_) {
        return;
    }
    
    cleanup();
    width_ = width;
    height_ = height;
    depth_ = depth;
    initialize();
}

void Grid3D::setCell(uint32_t x, uint32_t y, uint32_t z, bool alive) {
    if (!isValidPosition(x, y, z)) {
        return;
    }
    
    uint32_t index = getIndex(x, y, z);
    currentState_[index] = alive;
    needsStateSync_ = true;
}

bool Grid3D::getCell(uint32_t x, uint32_t y, uint32_t z) const {
    if (!isValidPosition(x, y, z)) {
        return false;
    }
    
    uint32_t index = getIndex(x, y, z);
    return currentState_[index];
}

void Grid3D::clear() {
    std::fill(currentState_.begin(), currentState_.end(), false);
    std::fill(nextState_.begin(), nextState_.end(), false);
    population_ = 0;
    generation_ = 0;
    needsStateSync_ = true;
    syncStateToGPU();
}

void Grid3D::randomize(float density) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dis(0.0f, 1.0f);
    
    for (size_t i = 0; i < currentState_.size(); ++i) {
        currentState_[i] = dis(gen) < density;
    }
    
    needsStateSync_ = true;
    updatePopulation();
    syncStateToGPU();
}

void Grid3D::setRuleSet(const GameRules::RuleSet& ruleSet) {
    currentRuleSet_ = ruleSet;
    rules_ = ruleSet;
}

void Grid3D::setRules(const GameRules::RuleSet& rules) {
    rules_ = rules;
    currentRuleSet_ = rules;
}

void Grid3D::setBoundaryType(GameRules::BoundaryType type) {
    boundaryType_ = type;
}

uint32_t Grid3D::getIndex(uint32_t x, uint32_t y, uint32_t z) const {
    return z * width_ * height_ + y * width_ + x;
}

bool Grid3D::isValidPosition(uint32_t x, uint32_t y, uint32_t z) const {
    return x < width_ && y < height_ && z < depth_;
}

bool Grid3D::getWrappedCell(int x, int y, int z) const {
    if (boundaryType_ == GameRules::BoundaryType::TOROIDAL) {
        x = (x + static_cast<int>(width_)) % static_cast<int>(width_);
        y = (y + static_cast<int>(height_)) % static_cast<int>(height_);
        z = (z + static_cast<int>(depth_)) % static_cast<int>(depth_);
    } else if (boundaryType_ == GameRules::BoundaryType::FIXED) {
        return false;  // Fixed boundary is always dead
    }
    
    if (x < 0 || x >= static_cast<int>(width_) ||
        y < 0 || y >= static_cast<int>(height_) ||
        z < 0 || z >= static_cast<int>(depth_)) {
        return false;
    }
    
    return currentState_[getIndex(static_cast<uint32_t>(x), static_cast<uint32_t>(y), static_cast<uint32_t>(z))];
}

uint8_t Grid3D::countNeighbors(uint32_t x, uint32_t y, uint32_t z) const {
    uint8_t count = 0;
    
    for (int dz = -1; dz <= 1; dz++) {
        for (int dy = -1; dy <= 1; dy++) {
            for (int dx = -1; dx <= 1; dx++) {
                if (dx == 0 && dy == 0 && dz == 0) continue;
                if (getWrappedCell(x + dx, y + dy, z + dz)) {
                    count++;
                }
            }
        }
    }
    
    return count;
}

void Grid3D::update() {
    if (!isInitialized_ || !compute_) {
        return;
    }
    
    // Sync state to GPU if needed
    if (needsStateSync_) {
        syncStateToGPU();
        needsStateSync_ = false;
    }
    
    // Update push constants
    ComputePushConstants constants;
    constants.width = width_;
    constants.height = height_;
    constants.depth = depth_;
    constants.time = static_cast<float>(generation_) * 0.1f;
    constants.ruleSet = getRuleSetIndex(currentRuleSet_);
    constants.surviveMin = currentRuleSet_.survivalRange[0];
    constants.surviveMax = currentRuleSet_.survivalRange[1];
    constants.birthCount = currentRuleSet_.birthRange[0];  // Simplified for now
    constants.padding[0] = 0;
    constants.padding[1] = 0;
    constants.padding[2] = 0;
    
    compute_->updatePushConstants(constants);
    
    // Dispatch compute shader
    compute_->dispatch(width_, height_, depth_);
    
    // Wait for completion
    compute_->waitForCompletion();
    
    // Read back state from GPU (from next state buffer which is the result)
    syncStateFromGPU();
    
    // For next iteration, current state is now updated, so we're ready
    
    generation_++;
    updatePopulation();
}

void Grid3D::updatePopulation() {
    population_ = std::count(currentState_.begin(), currentState_.end(), true);
}

void Grid3D::syncStateToGPU() {
    if (!compute_) return;
    
    // Convert bool vector to uint32_t for GPU
    std::vector<uint32_t> gpuState(currentState_.size());
    for (size_t i = 0; i < currentState_.size(); ++i) {
        gpuState[i] = currentState_[i] ? 1 : 0;
    }
    
    compute_->updateStateBuffer(gpuState, true);
}

void Grid3D::syncStateFromGPU() {
    if (!compute_) return;
    
    // Read state from GPU (from next state buffer after compute)
    std::vector<uint32_t> gpuState;
    compute_->readStateBuffer(gpuState, false);  // Read from next state buffer
    
    // Convert back to bool vector
    if (gpuState.size() == currentState_.size()) {
        for (size_t i = 0; i < currentState_.size(); ++i) {
            currentState_[i] = (gpuState[i] != 0);
        }
        
        // Update current state buffer for next iteration
        syncStateToGPU();
    }
}

bool Grid3D::loadPattern(const std::string& filename) {
    try {
        // Use existing PatternManager (it should work with minimal changes)
        auto pattern = ::PatternManager::loadPattern(filename);
        if (!pattern) {
            return false;
        }
        
        clear();
        
        // Resize grid to match pattern if needed
        if (pattern->width != width_ || pattern->height != height_ || pattern->depth != depth_) {
            resize(pattern->width, pattern->height, pattern->depth);
        }
        
        // Load pattern data into grid
        if (pattern->cells.size() == width_ * height_ * depth_) {
            for (size_t i = 0; i < pattern->cells.size(); ++i) {
                currentState_[i] = pattern->cells[i];
            }
        } else {
            // Fallback: if cells is a position list, parse it
            // For now, assume cells is bool vector
            for (size_t i = 0; i < currentState_.size() && i < pattern->cells.size(); ++i) {
                currentState_[i] = pattern->cells[i];
            }
        }
        
        syncStateToGPU();
        updatePopulation();
        
        VulkanHIP::Logger::getInstance().log(VulkanHIP::Logger::LogLevel::Info,
            "Pattern loaded: " + filename + " (" + std::to_string(pattern->cells.size()) + " cells)");
        return true;
    } catch (const std::exception& e) {
        VulkanHIP::Logger::getInstance().log(VulkanHIP::Logger::LogLevel::Error,
            std::string("Failed to load pattern: ") + e.what());
        return false;
    }
}

bool Grid3D::savePattern(const std::string& filename) const {
    try {
        ::PatternManager::Pattern pattern(
            std::filesystem::path(filename).stem().string(),
            "3D Game of Life Pattern",
            width_, height_, depth_,
            currentState_,  // Use current state as pattern data
            getRuleSetIndex(currentRuleSet_),
            static_cast<uint32_t>(boundaryType_),
            static_cast<uint32_t>(population_),
            static_cast<uint32_t>(generation_)
        );
        
        bool result = ::PatternManager::savePattern(filename, pattern);
        
        if (result) {
            VulkanHIP::Logger::getInstance().log(VulkanHIP::Logger::LogLevel::Info,
                "Pattern saved: " + filename + " (" + std::to_string(currentState_.size()) + " cells)");
        }
        
        return result;
    } catch (const std::exception& e) {
        VulkanHIP::Logger::getInstance().log(VulkanHIP::Logger::LogLevel::Error,
            std::string("Failed to save pattern: ") + e.what());
        return false;
    }
}

::PatternManager::Pattern Grid3D::getCurrentPattern() const {
    ::PatternManager::Pattern pattern(
        "Current Grid",
        "Current simulation state",
        width_, height_, depth_,
        currentState_,
        getRuleSetIndex(currentRuleSet_),
        static_cast<uint32_t>(boundaryType_),
        static_cast<uint32_t>(population_),
        static_cast<uint32_t>(generation_)
    );
    
    return pattern;
}

void Grid3D::updateVoxelRenderer(OpenGLVoxelRenderer& renderer) const {
    std::vector<VoxelInstance> instances;
    
    // Convert grid state to voxel instances
    for (uint32_t z = 0; z < depth_; ++z) {
        for (uint32_t y = 0; y < height_; ++y) {
            for (uint32_t x = 0; x < width_; ++x) {
                if (getCell(x, y, z)) {
                    VoxelInstance instance;
                    instance.position = glm::vec3(static_cast<float>(x), 
                                                  static_cast<float>(y), 
                                                  static_cast<float>(z));
                    instance.color = glm::vec4(0.2f, 0.8f, 0.2f, 1.0f);  // Default green
                    instance.age = static_cast<float>(generation_);
                    instance.lod = 0.0f;  // Will be calculated by renderer
                    instances.push_back(instance);
                }
            }
        }
    }
    
    renderer.updateInstances(instances);
}

void Grid3D::applyRules() {
    // CPU fallback rule application (if GPU compute not available)
    // This is mainly for validation - GPU compute is preferred
    for (uint32_t z = 0; z < depth_; ++z) {
        for (uint32_t y = 0; y < height_; ++y) {
            for (uint32_t x = 0; x < width_; ++x) {
                bool currentCell = getCell(x, y, z);
                uint8_t neighbors = countNeighbors(x, y, z);
                bool nextCell = GameRules::getNextState(currentCell, neighbors, rules_);
                
                uint32_t index = getIndex(x, y, z);
                nextState_[index] = nextCell;
            }
        }
    }
    
    std::swap(currentState_, nextState_);
    generation_++;
    updatePopulation();
}

} // namespace OpenGLHIP

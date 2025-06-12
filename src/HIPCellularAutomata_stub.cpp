// DEPRECATED: HIP implementation replaced with CPU version for cross-platform compatibility
// This file is kept for backward compatibility but redirects to CPUCellularAutomata

#include "HIPCellularAutomata.hpp"
#include "CPUCellularAutomata.hpp"
#include <iostream>
#include <memory>

namespace GameOfLife3D {
    
    // HIP implementation now uses CPU backend for cross-platform compatibility
    class HIPCellularAutomata::Impl {
    public:
        std::unique_ptr<CPUCellularAutomata> cpuEngine;
        
        Impl() : cpuEngine(std::make_unique<CPUCellularAutomata>()) {}
    };
    
    HIPCellularAutomata::HIPCellularAutomata() : initialized(false), pImpl(std::make_unique<Impl>()) {
        std::cout << "🔄 [GameOfLife] Using CPU backend for cross-platform compatibility" << std::endl;
    }
    
    HIPCellularAutomata::~HIPCellularAutomata() { cleanup(); }
    
    bool HIPCellularAutomata::initialize(const glm::ivec3& gridSize) {
        if (pImpl->cpuEngine->initialize(gridSize)) {
            gridDimensions = gridSize;
            totalCells = gridSize.x * gridSize.y * gridSize.z;
            gridSizeBytes = totalCells * sizeof(CellState);
            initialized = true;
            
            // Copy metrics from CPU engine
            const auto& cpuMetrics = pImpl->cpuEngine->getMetrics();
            metrics.throughputCellsPerSecond = cpuMetrics.throughputCellsPerSecond.load();
            metrics.memoryEfficiency = cpuMetrics.memoryEfficiency.load();
            
            std::cout << "🎮 [GameOfLife] Initialized " << totalCells << " cells (CPU backend)" << std::endl;
            return true;
        }
        return false;
    }
    
    void HIPCellularAutomata::cleanup() { 
        if (pImpl && pImpl->cpuEngine) {
            pImpl->cpuEngine->cleanup();
        }
        initialized = false; 
    }
    
    bool HIPCellularAutomata::resizeGrid(const glm::ivec3& newSize) { 
        return pImpl->cpuEngine->resizeGrid(newSize);
    }
    
    bool HIPCellularAutomata::setCellState(const glm::ivec3& position, bool alive, uint32_t energy) { 
        return pImpl->cpuEngine->setCellState(position, alive, energy);
    }
    
    bool HIPCellularAutomata::getCellState(const glm::ivec3& position, CellState& state) const { 
        return pImpl->cpuEngine->getCellState(position, state);
    }
    
    bool HIPCellularAutomata::loadPattern(const std::vector<glm::ivec3>& pattern, const glm::ivec3& offset) { 
        return pImpl->cpuEngine->loadPattern(pattern, offset);
    }
    
    bool HIPCellularAutomata::savePattern(const glm::ivec3& start, const glm::ivec3& end, std::vector<glm::ivec3>& pattern) { 
        return pImpl->cpuEngine->savePattern(start, end, pattern);
    }
    
    bool HIPCellularAutomata::stepSimulation() { 
        bool result = pImpl->cpuEngine->stepSimulation();
        
        // Update our metrics from CPU engine
        const auto& cpuMetrics = pImpl->cpuEngine->getMetrics();
        metrics.throughputCellsPerSecond = cpuMetrics.throughputCellsPerSecond.load();
        metrics.memoryEfficiency = cpuMetrics.memoryEfficiency.load();
        
        return result;
    }
    
    bool HIPCellularAutomata::runSimulation(uint32_t steps) { 
        return pImpl->cpuEngine->runSimulation(steps);
    }
    
    void HIPCellularAutomata::pauseSimulation() {
        pImpl->cpuEngine->pauseSimulation();
    }
    
    void HIPCellularAutomata::resetSimulation() {
        pImpl->cpuEngine->resetSimulation();
    }
    
    void HIPCellularAutomata::setSimulationParams(const SimulationParams& params) { 
        simParams = params;
        pImpl->cpuEngine->setSimulationParams(params);
    }
    
    void HIPCellularAutomata::optimizeMemoryLayout() {
        pImpl->cpuEngine->optimizeMemoryLayout();
    }
    
    bool HIPCellularAutomata::validateMemoryCoalescing() { 
        return pImpl->cpuEngine->validateCacheEfficiency();
    }
    
    float HIPCellularAutomata::getMememoryEfficiency() const { 
        return pImpl->cpuEngine->getMemoryEfficiency();
    }
    
    void HIPCellularAutomata::resetMetrics() {
        pImpl->cpuEngine->resetMetrics();
    }
}

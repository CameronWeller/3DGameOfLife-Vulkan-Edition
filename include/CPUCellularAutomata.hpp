#pragma once

#include "HIPCellularAutomata.hpp"  // For shared types
#include <vector>
#include <memory>
#include <atomic>
#include <functional>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <chrono>
#include <string>

namespace GameOfLife3D {

    /**
     * @brief SE001: Cross-Platform CPU Cellular Automata Implementation
     * Optimized for multi-threading and SIMD operations where possible
     */

    // Forward declarations - using types from HIPCellularAutomata.hpp
    struct CellState;
    struct SimulationParams;
    struct KernelMetrics;

    /**
     * @brief High-performance CPU-accelerated 3D Game of Life engine
     * Implements multi-threading with optimal work distribution
     */
    class CPUCellularAutomata {
    public:
        static constexpr uint32_t DEFAULT_THREAD_COUNT = 0; // Use hardware_concurrency
        static constexpr uint32_t SIMD_ALIGNMENT = 16;
        static constexpr uint32_t CACHE_LINE_SIZE = 64;

        CPUCellularAutomata();
        ~CPUCellularAutomata();

        // SE001: Core initialization and management
        bool initialize(const glm::ivec3& gridSize);
        void cleanup();
        bool isInitialized() const { return initialized; }

        // Grid management with cache-friendly memory layout
        bool resizeGrid(const glm::ivec3& newSize);
        bool setCellState(const glm::ivec3& position, bool alive, uint32_t energy = 100);
        bool getCellState(const glm::ivec3& position, CellState& state) const;
        
        // Pattern loading and management
        bool loadPattern(const std::vector<glm::ivec3>& pattern, const glm::ivec3& offset);
        bool savePattern(const glm::ivec3& start, const glm::ivec3& end, std::vector<glm::ivec3>& pattern);
        
        // Simulation execution
        bool stepSimulation();
        bool runSimulation(uint32_t steps);
        void pauseSimulation();
        void resetSimulation();
        
        // Parameter management
        void setSimulationParams(const SimulationParams& params);
        const SimulationParams& getSimulationParams() const { return simParams; }
        
        // Performance optimization
        void optimizeMemoryLayout();
        bool validateCacheEfficiency();
        float getMemoryEfficiency() const;
        void setThreadCount(uint32_t threadCount);
        
        // Data access for UX-Mirror integration
        const CellState* getGridData() const { return currentGrid.data(); }
        size_t getGridSizeBytes() const { return gridSizeBytes; }
        
        // Metrics and profiling
        const KernelMetrics& getMetrics() const { return metrics; }
        void resetMetrics();
        
        // Event callbacks for UX integration
        using CellUpdateCallback = std::function<void(const glm::ivec3&, const CellState&)>;
        using PerformanceCallback = std::function<void(const KernelMetrics&)>;
        
        void setCellUpdateCallback(CellUpdateCallback callback) { cellUpdateCallback = callback; }
        void setPerformanceCallback(PerformanceCallback callback) { perfCallback = callback; }

    private:
        bool initialized = false;
        
        // Grid data
        glm::ivec3 gridDimensions;
        size_t gridSizeBytes;
        size_t totalCells;
        
        // CPU memory (double buffered for efficient updates)
        std::vector<CellState> currentGrid;
        std::vector<CellState> nextGrid;
        
        // Simulation parameters
        SimulationParams simParams;
        
        // Threading
        uint32_t threadCount;
        std::vector<std::thread> workerThreads;
        std::vector<std::future<void>> threadFutures;
        
        // Performance tracking
        KernelMetrics metrics;
        std::chrono::high_resolution_clock::time_point lastStepTime;
        
        // Callbacks
        CellUpdateCallback cellUpdateCallback;
        PerformanceCallback perfCallback;
        
        // Internal methods
        bool allocateGridMemory();
        void deallocateGridMemory();
        void processGridChunk(size_t startIdx, size_t endIdx);
        void swapGrids();
        
        // Cache optimization helpers
        size_t calculateIndex(const glm::ivec3& pos) const;
        size_t calculateIndex(int x, int y, int z) const;
        glm::ivec3 calculatePosition(size_t index) const;
        
        // Neighbor processing
        uint32_t countNeighbors(const glm::ivec3& pos) const;
        bool isValidPosition(const glm::ivec3& pos) const;
        
        // SIMD-optimized operations (where available)
        void processNeighbors_SIMD(size_t startIdx, size_t count);
        void updateCellStates_SIMD(size_t startIdx, size_t count);
        
        // Performance measurement
        void updatePerformanceMetrics(std::chrono::microseconds processingTime);
        void logPerformanceMetrics();
    };

    /**
     * @brief Pattern utilities for CPU implementation
     */
    namespace PatternUtils {
        // Efficient pattern operations
        std::vector<uint32_t> compressPattern(const std::vector<glm::ivec3>& pattern);
        std::vector<glm::ivec3> decompressPattern(const std::vector<uint32_t>& compressed);
        
        // Pattern analysis
        glm::ivec3 calculatePatternBounds(const std::vector<glm::ivec3>& pattern);
        bool validatePattern(const std::vector<glm::ivec3>& pattern, const glm::ivec3& gridSize);
        
        // Memory-efficient pattern operations
        std::vector<glm::ivec3> optimizePatternForCacheAccess(const std::vector<glm::ivec3>& pattern);
    }

} // namespace GameOfLife3D 
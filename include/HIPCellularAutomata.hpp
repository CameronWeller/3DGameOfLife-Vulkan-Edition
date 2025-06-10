#pragma once

#ifdef _WIN32
// Windows fallback - use CUDA or stub definitions
typedef void* hipDevice_t;
typedef void* hipStream_t;  
typedef void* hipEvent_t;
typedef int hipError_t;
#define hipSuccess 0
#else
#include <hip/hip_runtime.h>
#endif
#include <glm/glm.hpp>
#include <vector>
#include <memory>
#include <atomic>
#include <functional>

namespace GameOfLife3D {

    /**
     * @brief SE001: HIP Cellular Automata Kernel Implementation
     * Optimized for memory coalescing and high-performance computation
     */

    // Cell state structure optimized for GPU memory access
    struct alignas(16) CellState {
        union {
            struct {
                uint32_t alive : 1;
                uint32_t age : 15;
                uint32_t energy : 8;
                uint32_t neighbors : 8;
            } bits;
            uint32_t packed;
        };
        
        glm::vec3 position;
        uint32_t metadata;
    };

    // Simulation parameters
    struct SimulationParams {
        glm::ivec3 gridDimensions;
        uint32_t ruleSet;
        float energyDecay;
        float reproductionThreshold;
        uint32_t maxAge;
        glm::vec3 boundaryConditions; // 0=dead, 1=wrap, 2=mirror
    };

    // Performance metrics for SE001
    struct KernelMetrics {
        std::atomic<uint64_t> kernelLaunchCount{0};
        std::atomic<uint64_t> totalKernelTime{0};
        std::atomic<uint64_t> cellsProcessed{0};
        std::atomic<float> throughputCellsPerSecond{0.0f};
        
        // Memory performance tracking (unblock hip_memory_coalescing)
        std::atomic<uint64_t> coalescedAccesses{0};
        std::atomic<uint64_t> uncoalescedAccesses{0};
        std::atomic<float> memoryEfficiency{0.0f};
    };

    /**
     * @brief High-performance HIP-accelerated 3D Game of Life engine
     * Implements workgroup tiling and optimized memory access patterns
     */
    class HIPCellularAutomata {
    public:
        static constexpr uint32_t WARP_SIZE = 64;
        static constexpr uint32_t WORKGROUP_SIZE_X = 8;
        static constexpr uint32_t WORKGROUP_SIZE_Y = 8;
        static constexpr uint32_t WORKGROUP_SIZE_Z = 8;
        static constexpr uint32_t TILE_SIZE = WORKGROUP_SIZE_X;

        HIPCellularAutomata();
        ~HIPCellularAutomata();

        // SE001: Core initialization and management
        bool initialize(const glm::ivec3& gridSize);
        void cleanup();
        bool isInitialized() const { return initialized; }

        // Grid management with optimized memory layout
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
        
        // Performance optimization (unblock hip_memory_coalescing)
        void optimizeMemoryLayout();
        bool validateMemoryCoalescing();
        float getMememoryEfficiency() const;
        
        // Data access for UX-Mirror integration
        const CellState* getGridData() const { return d_currentGrid; }
        size_t getGridSizeBytes() const { return gridSizeBytes; }
        hipDeviceptr_t getDevicePointer() const { return reinterpret_cast<hipDeviceptr_t>(d_currentGrid); }
        
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
        
        // Device memory (double buffered)
        CellState* d_currentGrid = nullptr;
        CellState* d_nextGrid = nullptr;
        CellState* h_gridBuffer = nullptr; // Host buffer for transfers
        
        // Simulation parameters
        SimulationParams simParams;
        SimulationParams* d_simParams = nullptr;
        
        // HIP streams for overlapped execution
        hipStream_t computeStream;
        hipStream_t transferStream;
        
        // Events for synchronization
        hipEvent_t kernelStart;
        hipEvent_t kernelEnd;
        
        // Performance tracking
        KernelMetrics metrics;
        
        // Callbacks
        CellUpdateCallback cellUpdateCallback;
        PerformanceCallback perfCallback;
        
        // Internal methods
        bool allocateDeviceMemory();
        void deallocateDeviceMemory();
        bool launchKernel();
        void swapGrids();
        
        // Memory optimization helpers
        bool analyzeMemoryAccessPatterns();
        glm::ivec3 calculateOptimalWorkgroupSize();
        bool setupMemoryCoalescing();
        
        // Kernel parameter calculation
        dim3 calculateGridDim() const;
        dim3 calculateBlockDim() const;
        
        // Error handling
        void checkHIPError(hipError_t error, const std::string& operation);
        void logPerformanceMetrics();
    };

    /**
     * @brief Kernel function declarations (implemented in .cu file)
     * These implement the core cellular automata rules with optimization
     */
    extern "C" {
        // Main simulation kernel with memory coalescing optimization
        __global__ void gameOfLifeKernel(
            const CellState* __restrict__ currentGrid,
            CellState* __restrict__ nextGrid,
            const SimulationParams* __restrict__ params,
            uint64_t* __restrict__ metrics_coalescedAccesses,
            uint64_t* __restrict__ metrics_uncoalescedAccesses
        );
        
        // Neighbor counting kernel with shared memory optimization
        __global__ void countNeighborsKernel(
            const CellState* __restrict__ grid,
            uint32_t* __restrict__ neighborCounts,
            const SimulationParams* __restrict__ params
        );
        
        // Pattern loading kernel
        __global__ void loadPatternKernel(
            CellState* __restrict__ grid,
            const glm::ivec3* __restrict__ pattern,
            uint32_t patternSize,
            const glm::ivec3 offset,
            const SimulationParams* __restrict__ params
        );
        
        // Memory access pattern analysis kernel
        __global__ void analyzeMemoryAccessKernel(
            const CellState* __restrict__ grid,
            uint64_t* __restrict__ accessMetrics,
            const SimulationParams* __restrict__ params
        );
    }

    /**
     * @brief Utility functions for pattern management and optimization
     */
    namespace PatternUtils {
        // Pattern compression using run-length encoding
        std::vector<uint32_t> compressPattern(const std::vector<glm::ivec3>& pattern);
        std::vector<glm::ivec3> decompressPattern(const std::vector<uint32_t>& compressed);
        
        // Pattern analysis
        glm::ivec3 calculatePatternBounds(const std::vector<glm::ivec3>& pattern);
        bool validatePattern(const std::vector<glm::ivec3>& pattern, const glm::ivec3& gridSize);
        
        // Memory layout optimization
        std::vector<glm::ivec3> optimizePatternForMemoryAccess(const std::vector<glm::ivec3>& pattern);
    }

} // namespace GameOfLife3D 
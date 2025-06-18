#pragma once

#include <atomic>
#include <array>
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <vulkan/vulkan.hpp>
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

namespace UXMirror {

    /**
     * @brief UX001: Real-time Metrics Pipeline Implementation
     * Implements lockfree circular buffer and binary metrics format
     */

    // Binary metrics format for high-performance serialization
    struct alignas(64) MetricsFrame {
        // Timestamp (8 bytes)
        uint64_t timestamp;
        
        // Performance metrics (32 bytes)
        struct {
            float gpuUtilization;
            uint64_t memoryUsage;
            float frameTime;
            float computeTime;
            uint32_t queueDepth;
            uint32_t padding[3]; // Align to 32 bytes
        } performance;
        
        // Interaction metrics (32 bytes)
        struct {
            glm::vec2 mousePosition;
            uint32_t mouseButtons;
            uint32_t keyboardState;
            float interactionIntensity;
            uint32_t eventCount;
            uint32_t padding[3]; // Align to 32 bytes
        } interaction;
        
        // Visual metrics (32 bytes)
        struct {
            float averageLuminance;
            float contrast;
            uint32_t pixelsChanged;
            uint32_t trianglesRendered;
            glm::vec4 dominantColor;
            uint32_t padding[2]; // Align to 32 bytes
        } visual;
        
        // UX analysis results (32 bytes)
        struct {
            float usabilityScore;
            float engagementLevel;
            float frustrationIndex;
            uint32_t patternCount;
            uint64_t totalInteractionTime;
            uint32_t padding[2]; // Align to 32 bytes
        } ux;
        
        // Total: 136 bytes, cache-line aligned
    };
    static_assert(sizeof(MetricsFrame) == 136, "MetricsFrame must be exactly 136 bytes");

    /**
     * @brief Lockfree circular buffer for high-performance metrics collection
     * Unblocks metrics_serialization_overhead bottleneck
     */
    template<size_t Size>
    class LockfreeCircularBuffer {
    private:
        static constexpr size_t BUFFER_SIZE = Size;
        static constexpr size_t BUFFER_MASK = Size - 1;
        static_assert((Size & BUFFER_MASK) == 0, "Buffer size must be power of 2");
        
        alignas(64) std::atomic<uint64_t> writeIndex{0};
        alignas(64) std::atomic<uint64_t> readIndex{0};
        alignas(64) MetricsFrame buffer[BUFFER_SIZE];
        
    public:
        bool tryWrite(const MetricsFrame& frame) {
            const uint64_t currentWrite = writeIndex.load(std::memory_order_relaxed);
            const uint64_t nextWrite = currentWrite + 1;
            
            if (nextWrite - readIndex.load(std::memory_order_acquire) >= BUFFER_SIZE) {
                return false; // Buffer full
            }
            
            buffer[currentWrite & BUFFER_MASK] = frame;
            writeIndex.store(nextWrite, std::memory_order_release);
            return true;
        }
        
        bool tryRead(MetricsFrame& frame) {
            const uint64_t currentRead = readIndex.load(std::memory_order_relaxed);
            
            if (currentRead == writeIndex.load(std::memory_order_acquire)) {
                return false; // Buffer empty
            }
            
            frame = buffer[currentRead & BUFFER_MASK];
            readIndex.store(currentRead + 1, std::memory_order_release);
            return true;
        }
        
        size_t size() const {
            return writeIndex.load(std::memory_order_acquire) - readIndex.load(std::memory_order_acquire);
        }
        
        bool empty() const { return size() == 0; }
        bool full() const { return size() >= BUFFER_SIZE; }
    };

    /**
     * @brief GPU timestamp collection for precise performance measurement
     */
    class GPUTimestampCollector {
    public:
        struct TimestampPair {
            VkQueryPool vulkanPool;
            uint32_t startQuery;
            uint32_t endQuery;
            hipEvent_t hipStart;
            hipEvent_t hipEnd;
            bool isActive;
        };
        
        GPUTimestampCollector(VkDevice device, uint32_t maxQueries = 1024);
        ~GPUTimestampCollector();
        
        bool initialize();
        void cleanup();
        
        // Timestamp management
        uint32_t beginTimestamp();
        void endTimestamp(uint32_t id);
        bool getTimestampResult(uint32_t id, double& timeMs);
        
        // Batch operations
        void beginFrame();
        void endFrame();
        std::vector<double> getFrameTimestamps();

    private:
        VkDevice device;
        VkQueryPool queryPool;
        std::vector<TimestampPair> timestamps;
        std::atomic<uint32_t> nextQuery{0};
        uint32_t maxQueries;
        double timestampPeriod;
    };

    /**
     * @brief Real-time metrics collection and processing pipeline
     */
    class MetricsPipeline {
    public:
        static constexpr size_t BUFFER_SIZE = 8192; // Must be power of 2
        
        MetricsPipeline();
        ~MetricsPipeline();
        
        // UX001: Core pipeline methods
        bool initialize(VkDevice device, VkPhysicalDevice physicalDevice);
        void cleanup();
        bool isRunning() const { return running; }
        
        // Data collection
        void collectFrame();
        void recordPerformanceMetrics(float gpuUtil, uint64_t memUsage, float frameTime, float computeTime);
        void recordInteractionMetrics(const glm::vec2& mousePos, uint32_t mouseButtons, uint32_t keyState);
        void recordVisualMetrics(float luminance, float contrast, uint32_t pixelsChanged, uint32_t triangles);
        void recordUXMetrics(float usability, float engagement, float frustration, uint32_t patterns);
        
        // Data consumption
        bool getMetricsFrame(MetricsFrame& frame);
        std::vector<MetricsFrame> getAllPendingFrames();
        void clearPendingFrames();
        
        // Binary serialization (unblock metrics_serialization_overhead)
        std::vector<uint8_t> serializeFrames(const std::vector<MetricsFrame>& frames);
        bool deserializeFrames(const std::vector<uint8_t>& data, std::vector<MetricsFrame>& frames);
        
        // Performance monitoring
        struct PipelineMetrics {
            std::atomic<uint64_t> framesProcessed{0};
            std::atomic<uint64_t> droppedFrames{0};
            std::atomic<uint64_t> totalProcessingTime{0};
            std::atomic<double> averageLatency{0.0};
            std::atomic<size_t> bufferUtilization{0};
        };
        
        const PipelineMetrics& getPipelineMetrics() const { return pipelineMetrics; }
        void resetPipelineMetrics();
        
        // Callbacks for real-time processing
        using MetricsCallback = std::function<void(const MetricsFrame&)>;
        using BatchCallback = std::function<void(const std::vector<MetricsFrame>&)>;
        
        void setRealTimeCallback(MetricsCallback callback) { realtimeCallback = callback; }
        void setBatchCallback(BatchCallback callback) { batchCallback = callback; }
        
        // Configuration
        void setCollectionRate(uint32_t hz);
        void setBatchSize(uint32_t size);
        void setCompressionEnabled(bool enabled);

    private:
        // Core components
        LockfreeCircularBuffer<BUFFER_SIZE> metricsBuffer;
        std::unique_ptr<GPUTimestampCollector> timestampCollector;
        
        // Current frame being built
        MetricsFrame currentFrame;
        std::atomic<bool> frameInProgress{false};
        
        // Threading and synchronization
        std::atomic<bool> running{false};
        std::thread processingThread;
        std::thread collectionThread;
        
        // Vulkan resources
        VkDevice vulkanDevice;
        VkPhysicalDevice physicalDevice;
        
        // Configuration
        std::atomic<uint32_t> collectionRateHz{1000};
        std::atomic<uint32_t> batchSize{100};
        std::atomic<bool> compressionEnabled{true};
        
        // Performance tracking
        PipelineMetrics pipelineMetrics;
        std::chrono::high_resolution_clock::time_point lastFrameTime;
        
        // Callbacks
        MetricsCallback realtimeCallback;
        BatchCallback batchCallback;
        
        // Internal processing methods
        void processingThreadFunction();
        void collectionThreadFunction();
        void processFrameBatch(const std::vector<MetricsFrame>& frames);
        
        // Timestamp utilities
        uint64_t getCurrentTimestamp() const;
        double calculateLatency(uint64_t timestamp) const;
        
        // Compression/decompression
        std::vector<uint8_t> compressFrameData(const std::vector<MetricsFrame>& frames);
        bool decompressFrameData(const std::vector<uint8_t>& compressed, std::vector<MetricsFrame>& frames);
        
        // Error handling
        void handleProcessingError(const std::string& error);
        void logMetrics();
    };

    /**
     * @brief Metrics aggregation and analysis utilities
     */
    namespace MetricsAnalysis {
        // Statistical analysis
        struct Statistics {
            double mean;
            double variance;
            double min;
            double max;
            double percentile95;
            uint64_t sampleCount;
        };
        
        Statistics calculateStatistics(const std::vector<MetricsFrame>& frames, 
                                     std::function<double(const MetricsFrame&)> extractor);
        
        // Performance analysis
        bool detectPerformanceRegression(const std::vector<MetricsFrame>& frames, double threshold);
        std::vector<uint64_t> findPerformanceSpikes(const std::vector<MetricsFrame>& frames, double threshold);
        
        // UX analysis
        double calculateEngagementTrend(const std::vector<MetricsFrame>& frames);
        std::vector<glm::vec2> extractInteractionHotspots(const std::vector<MetricsFrame>& frames);
        
        // Export utilities
        std::string exportToCSV(const std::vector<MetricsFrame>& frames);
        std::vector<uint8_t> exportToBinary(const std::vector<MetricsFrame>& frames);
    }

} // namespace UXMirror 
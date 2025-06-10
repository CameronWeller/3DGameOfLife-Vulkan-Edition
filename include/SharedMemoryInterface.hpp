#pragma once

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
#include <memory>
#include <vector>
#include <functional>
#include <atomic>
#include <mutex>

namespace UXMirror {

    // Forward declarations
    class VulkanContext;
    
    /**
     * @brief Shared memory buffer for Vulkan-HIP interoperability
     * Implements SA001: Initialize Vulkan-HIP Shared Memory
     */
    struct SharedBuffer {
        // Vulkan resources
        VkBuffer vulkanBuffer{VK_NULL_HANDLE};
        VkDeviceMemory vulkanMemory{VK_NULL_HANDLE};
        VkExternalMemoryHandleTypeFlagBits handleType;
        
        // HIP resources
        hipDeviceptr_t hipPointer{nullptr};
        hipExternalMemory_t hipExternalMemory{nullptr};
        
        // Buffer properties
        size_t size{0};
        VkBufferUsageFlags usage{0};
        bool isMapped{false};
        void* mappedPtr{nullptr};
        
        // Synchronization
        VkSemaphore vulkanSemaphore{VK_NULL_HANDLE};
        hipExternalSemaphore_t hipSemaphore{nullptr};
        std::atomic<uint64_t> timelineValue{0};
    };

    /**
     * @brief Cell state data structure for Game of Life simulation
     * Optimized for GPU memory access patterns
     */
    struct CellStateData {
        struct {
            uint32_t state : 1;        // alive/dead
            uint32_t age : 15;         // cell age
            uint32_t energy : 8;       // energy level
            uint32_t neighbors : 8;    // neighbor count
        } packed;
        
        glm::vec3 position;
        uint32_t metadata;
    };

    /**
     * @brief UX feedback data structure
     * Contains heatmap and interaction data
     */
    struct UXFeedbackData {
        struct HeatmapPoint {
            glm::vec2 screenPos;
            float intensity;
            uint32_t timestamp;
        };
        
        struct InteractionEvent {
            glm::vec3 worldPos;
            uint32_t eventType;
            float duration;
            uint32_t timestamp;
        };
        
        std::vector<HeatmapPoint> heatmap;
        std::vector<InteractionEvent> interactions;
        std::vector<VkCommandBuffer> optimizationHints;
    };

    /**
     * @brief Main shared memory interface class
     * Manages Vulkan-HIP interoperability for UX-Mirror integration
     */
    class SharedMemoryInterface {
    public:
        SharedMemoryInterface(VulkanContext* context);
        ~SharedMemoryInterface();

        // SA001: Core initialization methods
        bool initialize();
        void cleanup();
        
        // Buffer management
        bool createSharedBuffer(const std::string& name, size_t size, 
                              VkBufferUsageFlags usage);
        SharedBuffer* getBuffer(const std::string& name);
        bool destroyBuffer(const std::string& name);
        
        // Synchronization methods (unblock vulkan_timeline_semaphore_implementation)
        bool createTimelineSemaphore(const std::string& name);
        bool signalSemaphore(const std::string& name, uint64_t value);
        bool waitSemaphore(const std::string& name, uint64_t value, uint64_t timeout);
        
        // Data transfer methods
        bool writeSimulationState(const std::vector<CellStateData>& cellData);
        bool readSimulationState(std::vector<CellStateData>& cellData);
        bool writeUXFeedback(const UXFeedbackData& feedback);
        bool readUXFeedback(UXFeedbackData& feedback);
        
        // HIP integration
        bool mapBufferToHIP(const std::string& bufferName);
        bool unmapBufferFromHIP(const std::string& bufferName);
        hipDeviceptr_t getHIPPointer(const std::string& bufferName);
        
        // Performance monitoring
        struct PerformanceMetrics {
            std::atomic<uint64_t> transferCount{0};
            std::atomic<uint64_t> totalTransferTime{0};
            std::atomic<uint64_t> lastTransferTime{0};
            std::atomic<size_t> totalBytesTransferred{0};
        };
        
        const PerformanceMetrics& getMetrics() const { return metrics; }
        void resetMetrics();

    private:
        VulkanContext* vulkanContext;
        VkDevice device;
        VkPhysicalDevice physicalDevice;
        
        // Buffer storage
        std::unordered_map<std::string, std::unique_ptr<SharedBuffer>> buffers;
        std::mutex bufferMutex;
        
        // Semaphore storage
        std::unordered_map<std::string, VkSemaphore> semaphores;
        std::unordered_map<std::string, hipExternalSemaphore_t> hipSemaphores;
        std::mutex semaphoreMutex;
        
        // Performance tracking
        PerformanceMetrics metrics;
        
        // Internal helper methods
        bool checkVulkanExtensions();
        bool checkHIPCapabilities();
        bool setupExternalMemoryHandles();
        VkExternalMemoryHandleTypeFlagBits selectBestHandleType();
        
        // Error handling
        void logError(const std::string& message);
        bool validateBuffer(const SharedBuffer* buffer);
    };

    /**
     * @brief RAII wrapper for shared buffer operations
     * Ensures proper synchronization and cleanup
     */
    class SharedBufferGuard {
    public:
        SharedBufferGuard(SharedMemoryInterface* interface, const std::string& bufferName);
        ~SharedBufferGuard();
        
        SharedBuffer* get() { return buffer; }
        bool isValid() const { return buffer != nullptr; }
        
        // Synchronization helpers
        bool waitForVulkan(uint64_t timeout = UINT64_MAX);
        bool signalHIP();
        
    private:
        SharedMemoryInterface* interface;
        SharedBuffer* buffer;
        std::string name;
        bool ownsSync;
    };

} // namespace UXMirror 
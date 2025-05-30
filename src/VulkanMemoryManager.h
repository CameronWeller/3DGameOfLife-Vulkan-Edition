#pragma once

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <vector>
#include <memory>
#include <mutex>
#include <chrono>

namespace VulkanHIP {

class VulkanMemoryManager {
public:
    struct BufferAllocation {
        VkBuffer buffer;
        VmaAllocation allocation;
        VmaAllocationInfo allocationInfo;
        VkDeviceSize size;
        void* mappedData;  // For host-visible buffers

        // Operator overloads
        bool operator==(const BufferAllocation& other) const {
            return buffer == other.buffer && allocation == other.allocation;
        }

        bool operator!=(const BufferAllocation& other) const {
            return !(*this == other);
        }

        operator bool() const {
            return buffer != VK_NULL_HANDLE && allocation != nullptr;
        }
    };

    struct StagingBuffer {
        VkBuffer buffer;
        VmaAllocation allocation;
        VmaAllocationInfo allocationInfo;
        void* mappedData;
        VkDeviceSize size;
        bool inUse{false};  // Track if buffer is currently in use

        // Operator overloads
        bool operator==(const StagingBuffer& other) const {
            return buffer == other.buffer && allocation == other.allocation;
        }

        bool operator!=(const StagingBuffer& other) const {
            return !(*this == other);
        }

        operator bool() const {
            return buffer != VK_NULL_HANDLE && allocation != nullptr;
        }
    };

    struct ImageAllocation {
        VkImage image;
        VmaAllocation allocation;
        VkDeviceSize size;
        bool inUse;

        // Operator overloads
        bool operator==(const ImageAllocation& other) const {
            return image == other.image && allocation == other.allocation;
        }

        bool operator!=(const ImageAllocation& other) const {
            return !(*this == other);
        }

        operator bool() const {
            return image != VK_NULL_HANDLE && allocation != nullptr;
        }
    };

    // Double buffering support
    struct DoubleBuffer {
        BufferAllocation buffers[2];
        uint32_t currentBuffer{0};
        std::mutex bufferMutex;

        DoubleBuffer() = default;
        ~DoubleBuffer() = default;
        DoubleBuffer(const DoubleBuffer&) = delete;
        DoubleBuffer& operator=(const DoubleBuffer&) = delete;
        DoubleBuffer(DoubleBuffer&&) = default;
        DoubleBuffer& operator=(DoubleBuffer&&) = default;

        void swap() {
            std::lock_guard<std::mutex> lock(bufferMutex);
            currentBuffer = (currentBuffer + 1) % 2;
        }

        BufferAllocation& getCurrent() {
            std::lock_guard<std::mutex> lock(bufferMutex);
            return buffers[currentBuffer];
        }

        BufferAllocation& getNext() {
            std::lock_guard<std::mutex> lock(bufferMutex);
            return buffers[(currentBuffer + 1) % 2];
        }
    };

    // Memory pool for instanced rendering
    struct InstanceBufferPool {
        std::vector<BufferAllocation> buffers;
        std::vector<bool> inUse;
        VkDeviceSize bufferSize;
        uint32_t maxInstances;
        std::mutex poolMutex;

        InstanceBufferPool(VkDeviceSize size, uint32_t maxInst) 
            : bufferSize(size), maxInstances(maxInst) {}
        ~InstanceBufferPool() = default;
        InstanceBufferPool(const InstanceBufferPool&) = delete;
        InstanceBufferPool& operator=(const InstanceBufferPool&) = delete;
        InstanceBufferPool(InstanceBufferPool&&) = default;
        InstanceBufferPool& operator=(InstanceBufferPool&&) = default;
    };

    // Streaming buffer support
    struct StreamingBuffer {
        BufferAllocation buffer;
        VkDeviceSize size;
        VkDeviceSize offset;
        bool inUse;
        uint32_t frameIndex;
        std::mutex bufferMutex;

        StreamingBuffer() : size(0), offset(0), inUse(false), frameIndex(0) {}
    };

    // Enhanced memory pool with size-based allocation
    struct MemoryPool {
        std::vector<BufferAllocation> buffers;
        std::vector<bool> inUse;
        VkDeviceSize bufferSize;
        VmaMemoryUsage memoryUsage;
        VkBufferUsageFlags usageFlags;
        std::mutex poolMutex;

        MemoryPool(VkDeviceSize size, VmaMemoryUsage memUsage, VkBufferUsageFlags bufferUsage) 
            : bufferSize(size), memoryUsage(memUsage), usageFlags(bufferUsage) {}
    };

    // Performance monitoring
    class MemoryStats {
    public:
        MemoryStats() : totalAllocations(0), totalDeallocations(0), 
                       peakMemoryUsage(0), currentMemoryUsage(0) {
            lastReset = std::chrono::steady_clock::now();
        }

        // Delete copy constructor and assignment operator
        MemoryStats(const MemoryStats&) = delete;
        MemoryStats& operator=(const MemoryStats&) = delete;

        // Allow move operations
        MemoryStats(MemoryStats&& other) noexcept 
            : totalAllocations(other.totalAllocations),
              totalDeallocations(other.totalDeallocations),
              peakMemoryUsage(other.peakMemoryUsage),
              currentMemoryUsage(other.currentMemoryUsage),
              lastReset(other.lastReset) {
            other.totalAllocations = 0;
            other.totalDeallocations = 0;
            other.peakMemoryUsage = 0;
            other.currentMemoryUsage = 0;
        }

        MemoryStats& operator=(MemoryStats&& other) noexcept {
            if (this != &other) {
                totalAllocations = other.totalAllocations;
                totalDeallocations = other.totalDeallocations;
                peakMemoryUsage = other.peakMemoryUsage;
                currentMemoryUsage = other.currentMemoryUsage;
                lastReset = other.lastReset;

                other.totalAllocations = 0;
                other.totalDeallocations = 0;
                other.peakMemoryUsage = 0;
                other.currentMemoryUsage = 0;
            }
            return *this;
        }

        void update(size_t allocationSize, bool isAllocation) {
            std::lock_guard<std::mutex> lock(statsMutex);
            if (isAllocation) {
                totalAllocations++;
                currentMemoryUsage += allocationSize;
                peakMemoryUsage = std::max(peakMemoryUsage, currentMemoryUsage);
            } else {
                totalDeallocations++;
                currentMemoryUsage -= allocationSize;
            }
        }

        size_t getTotalAllocations() const {
            std::lock_guard<std::mutex> lock(statsMutex);
            return totalAllocations;
        }

        size_t getTotalDeallocations() const {
            std::lock_guard<std::mutex> lock(statsMutex);
            return totalDeallocations;
        }

        size_t getPeakMemoryUsage() const {
            std::lock_guard<std::mutex> lock(statsMutex);
            return peakMemoryUsage;
        }

        size_t getCurrentMemoryUsage() const {
            std::lock_guard<std::mutex> lock(statsMutex);
            return currentMemoryUsage;
        }

    private:
        size_t totalAllocations;
        size_t totalDeallocations;
        size_t peakMemoryUsage;
        size_t currentMemoryUsage;
        std::chrono::steady_clock::time_point lastReset;
        mutable std::mutex statsMutex;
    };

    // Timeline semaphore support
    struct TimelineSemaphore {
        VkSemaphore semaphore;
        uint64_t currentValue;
        std::mutex semaphoreMutex;

        TimelineSemaphore() : semaphore(VK_NULL_HANDLE), currentValue(0) {}
    };

    VulkanMemoryManager(VkDevice device, VkPhysicalDevice physicalDevice);
    ~VulkanMemoryManager();

    // Buffer management
    BufferAllocation createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage);
    void destroyBuffer(const BufferAllocation& allocation);
    void* mapMemory(const BufferAllocation& allocation);
    void unmapMemory(const BufferAllocation& allocation);

    // Staging buffer management
    StagingBuffer createStagingBuffer(VkDeviceSize size);
    void destroyStagingBuffer(StagingBuffer& stagingBuffer);
    void* mapStagingBuffer(StagingBuffer& stagingBuffer);
    void unmapStagingBuffer(StagingBuffer& stagingBuffer);

    // Memory pool management
    void createStagingPool(VkDeviceSize size);
    void destroyStagingPool();
    StagingBuffer allocateFromStagingPool(VkDeviceSize size);
    void freeStagingBuffer(StagingBuffer& stagingBuffer);

    // Image management
    ImageAllocation allocateImage(uint32_t width, uint32_t height, VkFormat format, 
                                VkImageTiling tiling, VkImageUsageFlags usage, 
                                VkMemoryPropertyFlags properties);
    void freeImage(const ImageAllocation& allocation);
    void createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags,
                        VkImageView& imageView);
    void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout,
                             VkImageLayout newLayout);
    void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
    void generateMipmaps(VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels);

    // Memory type finding
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;

    // Getters
    VmaAllocator getAllocator() const { return allocator_; }
    VkDevice getDevice() const { return device_; }
    VkPhysicalDevice getPhysicalDevice() const { return physicalDevice_; }
    VkCommandPool getCommandPool() const { return commandPool_; }
    VkQueue getGraphicsQueue() const { return graphicsQueue_; }

    // Command buffer management
    VkCommandBuffer beginSingleTimeCommands();
    void endSingleTimeCommands(VkCommandBuffer commandBuffer);

    // Cleanup
    void cleanup();

    // Compatibility stub
    BufferAllocation allocateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage) {
        return createBuffer(size, usage, memoryUsage);
    }

    // Double buffering methods
    DoubleBuffer createDoubleBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage);
    void destroyDoubleBuffer(DoubleBuffer& doubleBuffer);

    // Instance buffer pool methods
    InstanceBufferPool* createInstanceBufferPool(VkDeviceSize bufferSize, uint32_t maxInstances);
    BufferAllocation allocateFromInstancePool(InstanceBufferPool* pool);
    void freeInstanceBuffer(InstanceBufferPool* pool, const BufferAllocation& allocation);
    void destroyInstanceBufferPool(InstanceBufferPool* pool);

    // Methods for streaming buffers
    StreamingBuffer createStreamingBuffer(VkDeviceSize size, VkBufferUsageFlags usage);
    void destroyStreamingBuffer(StreamingBuffer& buffer);
    void* mapStreamingBuffer(StreamingBuffer& buffer);
    void unmapStreamingBuffer(StreamingBuffer& buffer);
    void updateStreamingBuffer(StreamingBuffer& buffer, const void* data, VkDeviceSize size, VkDeviceSize offset = 0);

    // Methods for enhanced memory pools
    MemoryPool* createMemoryPool(VkDeviceSize bufferSize, VmaMemoryUsage memoryUsage, VkBufferUsageFlags usage);
    BufferAllocation allocateFromPool(MemoryPool* pool);
    void freeToPool(MemoryPool* pool, const BufferAllocation& allocation);
    void destroyMemoryPool(MemoryPool* pool);

    // Methods for performance monitoring
    void resetMemoryStats();
    MemoryStats getMemoryStats() const;
    void updateMemoryStats(size_t allocationSize, bool isAllocation);

    // Methods for timeline semaphores
    TimelineSemaphore createTimelineSemaphore(uint64_t initialValue = 0);
    void destroyTimelineSemaphore(TimelineSemaphore& semaphore);
    void waitTimelineSemaphore(const TimelineSemaphore& semaphore, uint64_t value);
    void signalTimelineSemaphore(TimelineSemaphore& semaphore, uint64_t value);

private:
    VkDevice device_;
    VkPhysicalDevice physicalDevice_;
    VmaAllocator allocator_;
    VkCommandPool commandPool_;
    VkQueue graphicsQueue_;
    
    std::vector<BufferAllocation> bufferPool_;
    std::vector<StagingBuffer> stagingPool_;
    std::mutex stagingMutex_;
    VkDeviceSize maxStagingSize_;

    // Track image views for cleanup
    struct ImageViewInfo {
        VkImage image;
        VkImageView view;
    };
    std::vector<ImageViewInfo> imageViews_;

    // Memory pools
    std::vector<std::unique_ptr<InstanceBufferPool>> instancePools_;
    std::mutex instancePoolsMutex_;

    // Streaming buffers
    std::vector<StreamingBuffer> streamingBuffers_;
    std::mutex streamingMutex_;
    uint32_t currentFrameIndex_;

    // Performance monitoring
    MemoryStats memoryStats_;
    std::mutex statsMutex_;

    // Timeline semaphores
    std::vector<TimelineSemaphore> timelineSemaphores_;
    std::mutex semaphoreMutex_;

    // Memory pools
    std::vector<MemoryPool*> memoryPools_;
    std::mutex memoryPoolsMutex_;
    
    // Staging buffers
    std::vector<StagingBuffer> stagingBuffers_;
    std::mutex stagingBuffersMutex_;

    void createAllocator();
    void destroyAllocator();
    void cleanupStagingBuffers();

    // Helper methods
    void cleanupInstancePools();
    void cleanupMemoryPools();
    void cleanupStreamingBuffers();
    void cleanupTimelineSemaphores();
    VkDeviceSize alignSize(VkDeviceSize size, VkDeviceSize alignment) const;
};

} // namespace VulkanHIP 
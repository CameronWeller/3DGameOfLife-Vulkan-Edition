#pragma once

// GPU buffers and where their memory comes from.
//
// A Vulkan buffer is only a handle; its bytes live in a separate memory
// allocation bound to it. A GPU offers several memory types (fast GPU-only
// memory, memory the CPU can map, ...), each with property flags, and a buffer
// can only use the types its requirements allow.
//
// Every buffer gets its own memory allocation. That is wasteful for many small
// buffers, but the game has a few dozen, mostly large ones, and it keeps
// ownership obvious. Host-visible buffers stay mapped for their whole life.

#include <atomic>
#include <cstdint>
#include <initializer_list>
#include <optional>

#include <volk.h>

namespace gol3d {

// A buffer, its memory and, when host-visible, its persistent CPU mapping.
struct GpuBuffer {
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    void* mapped = nullptr; // null for device-only memory
    VkDeviceSize size = 0;

    // The CPU mapping viewed as an array of T; null for device-only memory.
    template <typename T>
    T* as() const {
        return static_cast<T*>(mapped);
    }
};

// Memory property sets to ask for; callers list them most preferred first.
namespace memory {
// CPU-visible and coherent: no flushes needed.
constexpr VkMemoryPropertyFlags HOST =
    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
// CPU-visible and cached: fast for the CPU to read back.
constexpr VkMemoryPropertyFlags HOST_CACHED = HOST | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
// GPU memory the CPU can also map ("resizable BAR"): fast for the GPU, and the
// CPU can still write small edits directly.
constexpr VkMemoryPropertyFlags DEVICE_HOST = HOST | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
// GPU-only memory.
constexpr VkMemoryPropertyFlags DEVICE = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
} // namespace memory

// Common usage combinations.
namespace usage {
constexpr VkBufferUsageFlags STORAGE = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
// Storage that is also copied to and from (pool growth, readbacks).
constexpr VkBufferUsageFlags STORAGE_COPY = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                            VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                                            VK_BUFFER_USAGE_TRANSFER_DST_BIT;
} // namespace usage

// Creates and frees GpuBuffers on one device, and keeps a running total of the
// memory they use. Two threads may create and free buffers at once (the chunk
// pool is allocated ahead of time on a worker thread).
class BufferAllocator {
public:
    void init(VkDevice device, const VkPhysicalDeviceMemoryProperties& memoryProperties);

    // Creates a buffer in the first memory property set of `preferences` that
    // exists and has room; a full resizable-BAR heap falls back to the next
    // choice. Returns false when no choice has room.
    bool tryCreate(GpuBuffer& out, VkDeviceSize size, VkBufferUsageFlags usage,
                   std::initializer_list<VkMemoryPropertyFlags> preferences);
    // Like tryCreate, but throws "Out of GPU memory".
    void create(GpuBuffer& out, VkDeviceSize size, VkBufferUsageFlags usage,
                std::initializer_list<VkMemoryPropertyFlags> preferences);
    // Frees the buffer and resets it to empty; empty buffers are ignored.
    void destroy(GpuBuffer& buffer);

    // A memory type allowed by `typeBits` with all `wanted` properties, skipping
    // the types in the `skip` bit mask.
    std::optional<uint32_t> findMemoryType(uint32_t typeBits, VkMemoryPropertyFlags wanted,
                                           uint32_t skip = 0) const;

    // Bytes currently allocated through this allocator (shown by F3 and --bench).
    VkDeviceSize bytesAllocated() const { return bytesAllocated_.load(); }

private:
    VkDevice device_ = VK_NULL_HANDLE;
    VkPhysicalDeviceMemoryProperties memoryProperties_{};
    std::atomic<VkDeviceSize> bytesAllocated_ = 0;
};

} // namespace gol3d

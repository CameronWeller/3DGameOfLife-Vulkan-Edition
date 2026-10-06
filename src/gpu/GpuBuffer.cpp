#include "gpu/GpuBuffer.h"

#include <algorithm>
#include <stdexcept>
#include <string>

#include "util/Units.h"

namespace gol3d {

void BufferAllocator::init(VkDevice device,
                           const VkPhysicalDeviceMemoryProperties& memoryProperties) {
    device_ = device;
    memoryProperties_ = memoryProperties;
}

std::optional<uint32_t> BufferAllocator::findMemoryType(uint32_t typeBits,
                                                        VkMemoryPropertyFlags wanted,
                                                        uint32_t skip) const {
    for (uint32_t i = 0; i < memoryProperties_.memoryTypeCount; ++i) {
        bool skipped = skip & (1u << i);
        bool allowed = typeBits & (1u << i);
        bool hasProperties = (memoryProperties_.memoryTypes[i].propertyFlags & wanted) == wanted;
        if (!skipped && allowed && hasProperties) return i;
    }
    return std::nullopt;
}

bool BufferAllocator::tryCreate(GpuBuffer& out, VkDeviceSize size, VkBufferUsageFlags usage,
                                std::initializer_list<VkMemoryPropertyFlags> preferences) {
    VkBufferCreateInfo bufferInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(device_, &bufferInfo, nullptr, &out.buffer) != VK_SUCCESS) {
        throw std::runtime_error("Could not create buffer.");
    }
    VkMemoryRequirements requirements;
    vkGetBufferMemoryRequirements(device_, out.buffer, &requirements);

    for (VkMemoryPropertyFlags wanted : preferences) {
        // Several memory types may match; try each until one has room.
        uint32_t tried = 0;
        while (std::optional<uint32_t> type =
                   findMemoryType(requirements.memoryTypeBits, wanted, tried)) {
            tried |= 1u << *type;
            VkMemoryAllocateInfo allocInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
            allocInfo.allocationSize = requirements.size;
            allocInfo.memoryTypeIndex = *type;
            if (vkAllocateMemory(device_, &allocInfo, nullptr, &out.memory) != VK_SUCCESS) continue;

            vkBindBufferMemory(device_, out.buffer, out.memory, 0);
            out.mapped = nullptr;
            bool hostVisible = wanted & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
            if (hostVisible &&
                vkMapMemory(device_, out.memory, 0, VK_WHOLE_SIZE, 0, &out.mapped) != VK_SUCCESS) {
                throw std::runtime_error("Could not map buffer memory.");
            }
            out.size = size;
            bytesAllocated_ += requirements.size;
            return true;
        }
    }
    vkDestroyBuffer(device_, out.buffer, nullptr);
    out = GpuBuffer{};
    return false;
}

void BufferAllocator::create(GpuBuffer& out, VkDeviceSize size, VkBufferUsageFlags usage,
                             std::initializer_list<VkMemoryPropertyFlags> preferences) {
    if (!tryCreate(out, size, usage, preferences)) {
        throw std::runtime_error("Out of GPU memory (" + std::to_string(toMegabytes(size)) +
                                 " MB buffer)");
    }
}

void BufferAllocator::destroy(GpuBuffer& buffer) {
    if (buffer.buffer == VK_NULL_HANDLE) return;
    VkMemoryRequirements requirements;
    vkGetBufferMemoryRequirements(device_, buffer.buffer, &requirements);
    bytesAllocated_ -= std::min<VkDeviceSize>(bytesAllocated_, requirements.size);
    vkDestroyBuffer(device_, buffer.buffer, nullptr);
    vkFreeMemory(device_, buffer.memory, nullptr);
    buffer = GpuBuffer{};
}

} // namespace gol3d

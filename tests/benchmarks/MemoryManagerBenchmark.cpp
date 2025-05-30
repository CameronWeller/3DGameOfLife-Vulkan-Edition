#include <benchmark/benchmark.h>
#include "VulkanMemoryManager.h"
#include <vulkan/vulkan.h>

class MemoryManagerFixture : public benchmark::Fixture {
protected:
    void SetUp(const benchmark::State&) override {
        VkInstanceCreateInfo instanceInfo{};
        instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        if (vkCreateInstance(&instanceInfo, nullptr, &instance) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create Vulkan instance");
        }

        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
        if (deviceCount == 0) {
            throw std::runtime_error("Failed to find GPUs with Vulkan support");
        }

        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());
        physicalDevice = devices[0];

        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = 0;
        queueCreateInfo.queueCount = 1;
        float queuePriority = 1.0f;
        queueCreateInfo.pQueuePriorities = &queuePriority;

        VkDeviceCreateInfo deviceInfo{};
        deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        deviceInfo.queueCreateInfoCount = 1;
        deviceInfo.pQueueCreateInfos = &queueCreateInfo;

        if (vkCreateDevice(physicalDevice, &deviceInfo, nullptr, &device) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create logical device");
        }

        memoryManager = std::make_unique<VulkanMemoryManager>(device, physicalDevice);
    }

    void TearDown(const benchmark::State&) override {
        memoryManager.reset();
        vkDestroyDevice(device, nullptr);
        vkDestroyInstance(instance, nullptr);
    }

    VkInstance instance;
    VkPhysicalDevice physicalDevice;
    VkDevice device;
    std::unique_ptr<VulkanMemoryManager> memoryManager;
};

BENCHMARK_DEFINE_F(MemoryManagerFixture, BufferAllocation)(benchmark::State& state) {
    const VkDeviceSize bufferSize = 1024 * 1024; // 1MB

    for (auto _ : state) {
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = bufferSize;
        bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VkBuffer buffer;
        if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create buffer");
        }

        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = 0; // TODO: Find the right memory type

        VkDeviceMemory memory;
        if (vkAllocateMemory(device, &allocInfo, nullptr, &memory) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate memory");
        }

        vkBindBufferMemory(device, buffer, memory, 0);

        vkDestroyBuffer(device, buffer, nullptr);
        vkFreeMemory(device, memory, nullptr);
    }
}

BENCHMARK_DEFINE_F(MemoryManagerFixture, MemoryMapping)(benchmark::State& state) {
    const VkDeviceSize bufferSize = 1024 * 1024; // 1MB

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = bufferSize;
    bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer buffer;
    if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create buffer");
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = 0; // TODO: Find the right memory type

    VkDeviceMemory memory;
    if (vkAllocateMemory(device, &allocInfo, nullptr, &memory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate memory");
    }

    vkBindBufferMemory(device, buffer, memory, 0);

    for (auto _ : state) {
        void* data;
        if (vkMapMemory(device, memory, 0, bufferSize, 0, &data) != VK_SUCCESS) {
            throw std::runtime_error("Failed to map memory");
        }

        // Write some data
        memset(data, 0, bufferSize);

        vkUnmapMemory(device, memory);
    }

    vkDestroyBuffer(device, buffer, nullptr);
    vkFreeMemory(device, memory, nullptr);
}

BENCHMARK_DEFINE_F(MemoryManagerFixture, MemoryCopy)(benchmark::State& state) {
    const VkDeviceSize bufferSize = 1024 * 1024; // 1MB

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = bufferSize;
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer srcBuffer, dstBuffer;
    if (vkCreateBuffer(device, &bufferInfo, nullptr, &srcBuffer) != VK_SUCCESS ||
        vkCreateBuffer(device, &bufferInfo, nullptr, &dstBuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create buffers");
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, srcBuffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = 0; // TODO: Find the right memory type

    VkDeviceMemory srcMemory, dstMemory;
    if (vkAllocateMemory(device, &allocInfo, nullptr, &srcMemory) != VK_SUCCESS ||
        vkAllocateMemory(device, &allocInfo, nullptr, &dstMemory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate memory");
    }

    vkBindBufferMemory(device, srcBuffer, srcMemory, 0);
    vkBindBufferMemory(device, dstBuffer, dstMemory, 0);

    for (auto _ : state) {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = VK_NULL_HANDLE; // TODO: Create command pool
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer commandBuffer;
        if (vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate command buffer");
        }

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        vkBeginCommandBuffer(commandBuffer, &beginInfo);

        VkBufferCopy copyRegion{};
        copyRegion.size = bufferSize;
        vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

        vkEndCommandBuffer(commandBuffer);

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;

        vkQueueSubmit(VK_NULL_HANDLE, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(VK_NULL_HANDLE);

        vkFreeCommandBuffers(device, VK_NULL_HANDLE, 1, &commandBuffer);
    }

    vkDestroyBuffer(device, srcBuffer, nullptr);
    vkDestroyBuffer(device, dstBuffer, nullptr);
    vkFreeMemory(device, srcMemory, nullptr);
    vkFreeMemory(device, dstMemory, nullptr);
}

BENCHMARK_REGISTER_F(MemoryManagerFixture, BufferAllocation)
    ->Unit(benchmark::kMicrosecond)
    ->Iterations(1000);

BENCHMARK_REGISTER_F(MemoryManagerFixture, MemoryMapping)
    ->Unit(benchmark::kMicrosecond)
    ->Iterations(1000);

BENCHMARK_REGISTER_F(MemoryManagerFixture, MemoryCopy)
    ->Unit(benchmark::kMicrosecond)
    ->Iterations(1000); 
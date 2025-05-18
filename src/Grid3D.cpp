#include "Grid3D.h"
#include "VulkanEngine.h"
#include <stdexcept>
#include <algorithm>

Grid3D::Grid3D(uint32_t width, uint32_t height, uint32_t depth)
    : width(width), height(height), depth(depth),
      population(0), generation(0),
      stateBuffer(VK_NULL_HANDLE), nextStateBuffer(VK_NULL_HANDLE),
      stateMemory(VK_NULL_HANDLE), nextStateMemory(VK_NULL_HANDLE),
      computeCommandBuffer(VK_NULL_HANDLE),
      descriptorSetLayout(VK_NULL_HANDLE),
      descriptorPool(VK_NULL_HANDLE),
      descriptorSet(VK_NULL_HANDLE),
      pipelineLayout(VK_NULL_HANDLE),
      computePipeline(VK_NULL_HANDLE),
      populationPipeline(VK_NULL_HANDLE),
      populationBuffer(VK_NULL_HANDLE),
      populationMemory(VK_NULL_HANDLE) {
}

Grid3D::~Grid3D() {
    destroyComputeResources();
    destroyBuffers();
}

void Grid3D::initialize() {
    createBuffers();
    allocateMemory();
    createComputeResources();
    clear();
}

void Grid3D::createBuffers() {
    auto& memoryPool = VulkanEngine::getInstance()->getMemoryPool();
    
    // Allocate state buffers from memory pool
    auto stateAllocation = memoryPool.allocateBuffer(
        getTotalCells() * sizeof(uint32_t),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );
    stateBuffer = stateAllocation.buffer;
    stateMemory = stateAllocation.memory;

    auto nextStateAllocation = memoryPool.allocateBuffer(
        getTotalCells() * sizeof(uint32_t),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );
    nextStateBuffer = nextStateAllocation.buffer;
    nextStateMemory = nextStateAllocation.memory;
}

void Grid3D::allocateMemory() {
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(VulkanEngine::getDevice(), stateBuffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = VulkanEngine::findMemoryType(
        memRequirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );

    VkResult result = vkAllocateMemory(VulkanEngine::getDevice(), &allocInfo, nullptr, &stateMemory);
    if (result != VK_SUCCESS) {
        vkDestroyBuffer(VulkanEngine::getDevice(), stateBuffer, nullptr);
        vkDestroyBuffer(VulkanEngine::getDevice(), nextStateBuffer, nullptr);
        throw std::runtime_error("Failed to allocate grid memory!");
    }

    result = vkBindBufferMemory(VulkanEngine::getDevice(), stateBuffer, stateMemory, 0);
    if (result != VK_SUCCESS) {
        vkDestroyBuffer(VulkanEngine::getDevice(), stateBuffer, nullptr);
        vkDestroyBuffer(VulkanEngine::getDevice(), nextStateBuffer, nullptr);
        vkFreeMemory(VulkanEngine::getDevice(), stateMemory, nullptr);
        vkFreeMemory(VulkanEngine::getDevice(), nextStateMemory, nullptr);
        throw std::runtime_error("Failed to bind grid buffer memory!");
    }

    result = vkBindBufferMemory(VulkanEngine::getDevice(), nextStateBuffer, stateMemory, 0);
    if (result != VK_SUCCESS) {
        vkDestroyBuffer(VulkanEngine::getDevice(), stateBuffer, nullptr);
        vkDestroyBuffer(VulkanEngine::getDevice(), nextStateBuffer, nullptr);
        vkFreeMemory(VulkanEngine::getDevice(), stateMemory, nullptr);
        vkFreeMemory(VulkanEngine::getDevice(), nextStateMemory, nullptr);
        throw std::runtime_error("Failed to bind grid buffer memory!");
    }
}

void Grid3D::createComputeResources() {
    // Create descriptor set layout
    VkDescriptorSetLayoutBinding stateBinding{};
    stateBinding.binding = 0;
    stateBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    stateBinding.descriptorCount = 1;
    stateBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutBinding nextStateBinding{};
    nextStateBinding.binding = 1;
    nextStateBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    nextStateBinding.descriptorCount = 1;
    nextStateBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    std::array<VkDescriptorSetLayoutBinding, 2> bindings = {stateBinding, nextStateBinding};
    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    VkResult result = vkCreateDescriptorSetLayout(VulkanEngine::getDevice(), &layoutInfo, nullptr, &descriptorSetLayout);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create descriptor set layout!");
    }

    // Create descriptor pool
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSize.descriptorCount = 2;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = 1;

    result = vkCreateDescriptorPool(VulkanEngine::getDevice(), &poolInfo, nullptr, &descriptorPool);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create descriptor pool!");
    }

    // Allocate descriptor set
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &descriptorSetLayout;

    result = vkAllocateDescriptorSets(VulkanEngine::getDevice(), &allocInfo, &descriptorSet);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate descriptor set!");
    }

    // Update descriptor set
    VkDescriptorBufferInfo stateBufferInfo{};
    stateBufferInfo.buffer = stateBuffer;
    stateBufferInfo.offset = 0;
    stateBufferInfo.range = getTotalCells() * sizeof(uint32_t);

    VkDescriptorBufferInfo nextStateBufferInfo{};
    nextStateBufferInfo.buffer = nextStateBuffer;
    nextStateBufferInfo.offset = 0;
    nextStateBufferInfo.range = getTotalCells() * sizeof(uint32_t);

    std::array<VkWriteDescriptorSet, 2> descriptorWrites{};
    descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[0].dstSet = descriptorSet;
    descriptorWrites[0].dstBinding = 0;
    descriptorWrites[0].dstArrayElement = 0;
    descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites[0].descriptorCount = 1;
    descriptorWrites[0].pBufferInfo = &stateBufferInfo;

    descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[1].dstSet = descriptorSet;
    descriptorWrites[1].dstBinding = 1;
    descriptorWrites[1].dstArrayElement = 0;
    descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites[1].descriptorCount = 1;
    descriptorWrites[1].pBufferInfo = &nextStateBufferInfo;

    vkUpdateDescriptorSets(VulkanEngine::getDevice(), static_cast<uint32_t>(descriptorWrites.size()),
                          descriptorWrites.data(), 0, nullptr);

    // Create compute pipeline
    computePipeline = VulkanEngine::createComputePipeline("shaders/game_of_life_3d.comp");

    // Create population reduction pipeline
    populationPipeline = VulkanEngine::createComputePipeline("shaders/population_reduction.comp");

    // Create population result buffer
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = sizeof(uint32_t);
    bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    result = vkCreateBuffer(VulkanEngine::getDevice(), &bufferInfo, nullptr, &populationBuffer);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create population buffer!");
    }

    vkGetBufferMemoryRequirements(VulkanEngine::getDevice(), populationBuffer, &memRequirements);

    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = VulkanEngine::findMemoryType(
        memRequirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );

    result = vkAllocateMemory(VulkanEngine::getDevice(), &allocInfo, nullptr, &populationMemory);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate population memory!");
    }

    result = vkBindBufferMemory(VulkanEngine::getDevice(), populationBuffer, populationMemory, 0);
    if (result != VK_SUCCESS) {
        vkDestroyBuffer(VulkanEngine::getDevice(), populationBuffer, nullptr);
        vkFreeMemory(VulkanEngine::getDevice(), populationMemory, nullptr);
        throw std::runtime_error("Failed to bind population buffer memory!");
    }

    // Update descriptor set for population reduction
    VkDescriptorBufferInfo populationBufferInfo{};
    populationBufferInfo.buffer = populationBuffer;
    populationBufferInfo.offset = 0;
    populationBufferInfo.range = sizeof(uint32_t);

    VkWriteDescriptorSet populationWrite{};
    populationWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    populationWrite.dstSet = descriptorSet;
    populationWrite.dstBinding = 2;
    populationWrite.dstArrayElement = 0;
    populationWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    populationWrite.descriptorCount = 1;
    populationWrite.pBufferInfo = &populationBufferInfo;

    vkUpdateDescriptorSets(VulkanEngine::getDevice(), 1, &populationWrite, 0, nullptr);
}

void Grid3D::destroyComputeResources() {
    if (computePipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(VulkanEngine::getDevice(), computePipeline, nullptr);
    }
    if (pipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(VulkanEngine::getDevice(), pipelineLayout, nullptr);
    }
    if (descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(VulkanEngine::getDevice(), descriptorPool, nullptr);
    }
    if (descriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(VulkanEngine::getDevice(), descriptorSetLayout, nullptr);
    }
    if (populationBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(VulkanEngine::getDevice(), populationBuffer, nullptr);
    }
    if (populationMemory != VK_NULL_HANDLE) {
        vkFreeMemory(VulkanEngine::getDevice(), populationMemory, nullptr);
    }
    if (populationPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(VulkanEngine::getDevice(), populationPipeline, nullptr);
    }
}

void Grid3D::destroyBuffers() {
    auto& memoryPool = VulkanEngine::getInstance()->getMemoryPool();
    
    if (stateBuffer != VK_NULL_HANDLE) {
        memoryPool.freeBuffer({stateBuffer, stateMemory, getTotalCells() * sizeof(uint32_t),
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, false});
    }
    if (nextStateBuffer != VK_NULL_HANDLE) {
        memoryPool.freeBuffer({nextStateBuffer, nextStateMemory, getTotalCells() * sizeof(uint32_t),
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, false});
    }
}

void Grid3D::update() {
    static uint32_t currentFrame = 0;
    auto& memoryPool = VulkanEngine::getInstance()->getMemoryPool();
    
    // Wait for previous frame's compute operations to complete
    vkWaitForFences(VulkanEngine::getDevice(), 1, &VulkanEngine::getInstance()->getComputeFence(currentFrame), VK_TRUE, UINT64_MAX);
    vkResetFences(VulkanEngine::getDevice(), 1, &VulkanEngine::getInstance()->getComputeFence(currentFrame));

    // Record compute command buffer
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    if (vkBeginCommandBuffer(computeCommandBuffer, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("Failed to begin recording compute command buffer!");
    }

    // Add memory barrier to ensure previous writes are complete
    VkMemoryBarrier memoryBarrier{};
    memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    memoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    memoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(computeCommandBuffer,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);

    // Bind compute pipeline
    vkCmdBindPipeline(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);

    // Bind descriptor set
    vkCmdBindDescriptorSets(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                           pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

    // Push constants for grid dimensions
    uint32_t pushConstants[3] = {width, height, depth};
    vkCmdPushConstants(computeCommandBuffer, pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT,
                      0, sizeof(pushConstants), pushConstants);

    // Dispatch compute shader
    uint32_t groupCountX = (width + 7) / 8;
    uint32_t groupCountY = (height + 7) / 8;
    uint32_t groupCountZ = (depth + 7) / 8;
    vkCmdDispatch(computeCommandBuffer, groupCountX, groupCountY, groupCountZ);

    // Add memory barrier to ensure compute shader writes are complete
    memoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    memoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(computeCommandBuffer,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);

    // Reset population buffer to zero
    vkCmdFillBuffer(computeCommandBuffer, populationBuffer, 0, sizeof(uint32_t), 0);

    // Bind population reduction pipeline
    vkCmdBindPipeline(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, populationPipeline);

    // Dispatch population reduction shader
    uint32_t groupCount = (getTotalCells() + 255) / 256;
    vkCmdDispatch(computeCommandBuffer, groupCount, 1, 1);

    if (vkEndCommandBuffer(computeCommandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to record compute command buffer!");
    }

    // Submit compute command buffer with synchronization
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &computeCommandBuffer;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &VulkanEngine::getInstance()->getComputeSemaphore(currentFrame);

    if (vkQueueSubmit(VulkanEngine::getComputeQueue(), 1, &submitInfo, 
        VulkanEngine::getInstance()->getComputeFence(currentFrame)) != VK_SUCCESS) {
        throw std::runtime_error("Failed to submit compute command buffer!");
    }

    // Read population count using staging buffer
    auto stagingBuffer = memoryPool.getStagingBuffer(sizeof(uint32_t));
    
    // Copy from population buffer to staging buffer
    VkCommandBuffer commandBuffer = VulkanEngine::beginSingleTimeCommands();

    VkBufferCopy copyRegion{};
    copyRegion.size = sizeof(uint32_t);
    vkCmdCopyBuffer(commandBuffer, populationBuffer, stagingBuffer.buffer, 1, &copyRegion);

    VulkanEngine::endSingleTimeCommands(commandBuffer);

    // Read population count
    void* data;
    vkMapMemory(VulkanEngine::getDevice(), stagingBuffer.memory, 0, sizeof(uint32_t), 0, &data);
    memcpy(&population, data, sizeof(uint32_t));
    vkUnmapMemory(VulkanEngine::getDevice(), stagingBuffer.memory);

    // Return staging buffer to pool
    memoryPool.returnStagingBuffer(stagingBuffer);

    // Swap buffers
    std::swap(stateBuffer, nextStateBuffer);
    std::swap(stateMemory, nextStateMemory);

    // Update statistics
    generation++;
    currentFrame = (currentFrame + 1) % 2;
}

void Grid3D::setCell(uint32_t x, uint32_t y, uint32_t z, bool state) {
    if (!isValidPosition(x, y, z)) return;
    
    auto& memoryPool = VulkanEngine::getInstance()->getMemoryPool();
    auto stagingBuffer = memoryPool.getStagingBuffer(sizeof(uint32_t));
    
    // Copy value to staging buffer
    void* data;
    vkMapMemory(VulkanEngine::getDevice(), stagingBuffer.memory, 0, sizeof(uint32_t), 0, &data);
    uint32_t value = state ? 1 : 0;
    memcpy(data, &value, sizeof(uint32_t));
    vkUnmapMemory(VulkanEngine::getDevice(), stagingBuffer.memory);

    // Copy from staging buffer to state buffer
    VkCommandBuffer commandBuffer = VulkanEngine::beginSingleTimeCommands();

    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = getIndex(x, y, z) * sizeof(uint32_t);
    copyRegion.size = sizeof(uint32_t);
    vkCmdCopyBuffer(commandBuffer, stagingBuffer.buffer, stateBuffer, 1, &copyRegion);

    VulkanEngine::endSingleTimeCommands(commandBuffer);

    // Return staging buffer to pool
    memoryPool.returnStagingBuffer(stagingBuffer);
}

bool Grid3D::getCell(uint32_t x, uint32_t y, uint32_t z) const {
    if (!isValidPosition(x, y, z)) return false;
    
    auto& memoryPool = VulkanEngine::getInstance()->getMemoryPool();
    auto stagingBuffer = memoryPool.getStagingBuffer(sizeof(uint32_t));

    // Copy from state buffer to staging buffer
    VkCommandBuffer commandBuffer = VulkanEngine::beginSingleTimeCommands();

    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = getIndex(x, y, z) * sizeof(uint32_t);
    copyRegion.dstOffset = 0;
    copyRegion.size = sizeof(uint32_t);
    vkCmdCopyBuffer(commandBuffer, stateBuffer, stagingBuffer.buffer, 1, &copyRegion);

    VulkanEngine::endSingleTimeCommands(commandBuffer);

    // Read value from staging buffer
    void* data;
    vkMapMemory(VulkanEngine::getDevice(), stagingBuffer.memory, 0, sizeof(uint32_t), 0, &data);
    uint32_t value;
    memcpy(&value, data, sizeof(uint32_t));
    vkUnmapMemory(VulkanEngine::getDevice(), stagingBuffer.memory);

    // Return staging buffer to pool
    memoryPool.returnStagingBuffer(stagingBuffer);

    return value == 1;
}

void Grid3D::clear() {
    auto& memoryPool = VulkanEngine::getInstance()->getMemoryPool();
    auto stagingBuffer = memoryPool.getStagingBuffer(getTotalCells() * sizeof(uint32_t));

    // Fill staging buffer with zeros
    void* data;
    vkMapMemory(VulkanEngine::getDevice(), stagingBuffer.memory, 0, getTotalCells() * sizeof(uint32_t), 0, &data);
    memset(data, 0, getTotalCells() * sizeof(uint32_t));
    vkUnmapMemory(VulkanEngine::getDevice(), stagingBuffer.memory);

    // Copy from staging buffer to state buffer
    VkCommandBuffer commandBuffer = VulkanEngine::beginSingleTimeCommands();

    VkBufferCopy copyRegion{};
    copyRegion.size = getTotalCells() * sizeof(uint32_t);
    vkCmdCopyBuffer(commandBuffer, stagingBuffer.buffer, stateBuffer, 1, &copyRegion);

    VulkanEngine::endSingleTimeCommands(commandBuffer);

    // Return staging buffer to pool
    memoryPool.returnStagingBuffer(stagingBuffer);

    population = 0;
    generation = 0;
}

void Grid3D::resize(uint32_t newWidth, uint32_t newHeight, uint32_t newDepth) {
    destroyComputeResources();
    destroyBuffers();
    
    width = newWidth;
    height = newHeight;
    depth = newDepth;
    
    initialize();
}

uint32_t Grid3D::getIndex(uint32_t x, uint32_t y, uint32_t z) const {
    return z * width * height + y * width + x;
}

bool Grid3D::isValidPosition(uint32_t x, uint32_t y, uint32_t z) const {
    return x < width && y < height && z < depth;
}

uint32_t Grid3D::countNeighbors(uint32_t x, uint32_t y, uint32_t z) const {
    uint32_t count = 0;
    
    // Check all 26 neighbors in 3D space
    for (int dx = -1; dx <= 1; dx++) {
        for (int dy = -1; dy <= 1; dy++) {
            for (int dz = -1; dz <= 1; dz++) {
                if (dx == 0 && dy == 0 && dz == 0) continue;
                
                int nx = x + dx;
                int ny = y + dy;
                int nz = z + dz;
                
                if (isValidPosition(nx, ny, nz) && getCell(nx, ny, nz)) {
                    count++;
                }
            }
        }
    }
    
    return count;
} 
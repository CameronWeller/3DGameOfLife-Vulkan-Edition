#include <gtest/gtest.h>
#include "../src/VulkanEngine.h"
#include <memory>
#include <vector>

// This test suite focuses on finding memory leaks
class MemoryLeakTest : public ::testing::Test {
protected:
    void SetUp() override {
        engine = std::make_unique<VulkanEngine>();
        engine->initWindow(800, 600, "Memory Leak Test Window");
        engine->initVulkan();
    }

    void TearDown() override {
        engine->cleanup();
    }

    std::unique_ptr<VulkanEngine> engine;
};

// Test repeated shader module creation and destruction
TEST_F(MemoryLeakTest, RepeatedShaderModuleCreationTest) {
    const std::vector<char> dummyCode(1024, 0);  // 1KB of zeros

    for (int i = 0; i < 100; i++) {
        VkShaderModule shaderModule = engine->createShaderModule(dummyCode);
        EXPECT_NE(shaderModule, VK_NULL_HANDLE);
        vkDestroyShaderModule(engine->getDevice(), shaderModule, nullptr);
    }
}

// Test buffer creation without cleanup
TEST_F(MemoryLeakTest, BufferLeakTest) {
    std::vector<VkBuffer> buffers;
    std::vector<VkDeviceMemory> memories;
    
    // Create multiple buffers without immediate cleanup
    for (int i = 0; i < 10; i++) {
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = 1024 * (i + 1);  // Increasing sizes
        bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VkBuffer buffer;
        ASSERT_EQ(vkCreateBuffer(engine->getDevice(), &bufferInfo, nullptr, &buffer), VK_SUCCESS);
        buffers.push_back(buffer);

        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(engine->getDevice(), buffer, &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = engine->findMemoryType(
            memRequirements.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        );

        VkDeviceMemory bufferMemory;
        ASSERT_EQ(vkAllocateMemory(engine->getDevice(), &allocInfo, nullptr, &bufferMemory), VK_SUCCESS);
        memories.push_back(bufferMemory);

        ASSERT_EQ(vkBindBufferMemory(engine->getDevice(), buffer, bufferMemory, 0), VK_SUCCESS);
    }
    
    // Clean up all resources
    for (size_t i = 0; i < buffers.size(); i++) {
        vkDestroyBuffer(engine->getDevice(), buffers[i], nullptr);
        vkFreeMemory(engine->getDevice(), memories[i], nullptr);
    }
}

// Test command buffer allocation without cleanup
TEST_F(MemoryLeakTest, CommandBufferLeakTest) {
    // Create a command pool
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = engine->findQueueFamilies(engine->getPhysicalDevice()).graphicsFamily.value();

    VkCommandPool commandPool;
    ASSERT_EQ(vkCreateCommandPool(engine->getDevice(), &poolInfo, nullptr, &commandPool), VK_SUCCESS);

    // Allocate multiple command buffers
    std::vector<VkCommandBuffer> commandBuffers(50);
    
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());

    ASSERT_EQ(vkAllocateCommandBuffers(engine->getDevice(), &allocInfo, commandBuffers.data()), VK_SUCCESS);
    
    // Reset the command buffers multiple times
    for (const auto& commandBuffer : commandBuffers) {
        vkResetCommandBuffer(commandBuffer, 0);
    }
    
    // Clean up
    vkFreeCommandBuffers(engine->getDevice(), commandPool, static_cast<uint32_t>(commandBuffers.size()), commandBuffers.data());
    vkDestroyCommandPool(engine->getDevice(), commandPool, nullptr);
}

// Test descriptor set leak
TEST_F(MemoryLeakTest, DescriptorSetLeakTest) {
    // Create descriptor set layout
    VkDescriptorSetLayoutBinding layoutBinding{};
    layoutBinding.binding = 0;
    layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    layoutBinding.descriptorCount = 1;
    layoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    
    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &layoutBinding;
    
    VkDescriptorSetLayout setLayout;
    ASSERT_EQ(vkCreateDescriptorSetLayout(engine->getDevice(), &layoutInfo, nullptr, &setLayout), VK_SUCCESS);
    
    // Create descriptor pool
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSize.descriptorCount = 100;
    
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = 100;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    
    VkDescriptorPool descriptorPool;
    ASSERT_EQ(vkCreateDescriptorPool(engine->getDevice(), &poolInfo, nullptr, &descriptorPool), VK_SUCCESS);
    
    // Allocate many descriptor sets
    std::vector<VkDescriptorSet> descriptorSets(100);
    std::vector<VkDescriptorSetLayout> layouts(100, setLayout);
    
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool;
    allocInfo.descriptorSetCount = static_cast<uint32_t>(layouts.size());
    allocInfo.pSetLayouts = layouts.data();
    
    ASSERT_EQ(vkAllocateDescriptorSets(engine->getDevice(), &allocInfo, descriptorSets.data()), VK_SUCCESS);
    
    // Free half of the descriptor sets
    std::vector<VkDescriptorSet> halfSets(descriptorSets.begin(), descriptorSets.begin() + 50);
    vkFreeDescriptorSets(engine->getDevice(), descriptorPool, static_cast<uint32_t>(halfSets.size()), halfSets.data());
    
    // Clean up
    vkDestroyDescriptorPool(engine->getDevice(), descriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(engine->getDevice(), setLayout, nullptr);
}

// Test pipeline leak
TEST_F(MemoryLeakTest, PipelineLeakTest) {
    // Create descriptor set layout
    VkDescriptorSetLayoutBinding layoutBinding{};
    layoutBinding.binding = 0;
    layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    layoutBinding.descriptorCount = 1;
    layoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    
    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &layoutBinding;
    
    VkDescriptorSetLayout setLayout;
    ASSERT_EQ(vkCreateDescriptorSetLayout(engine->getDevice(), &layoutInfo, nullptr, &setLayout), VK_SUCCESS);
    
    // Create pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &setLayout;
    
    VkPipelineLayout pipelineLayout;
    ASSERT_EQ(vkCreatePipelineLayout(engine->getDevice(), &pipelineLayoutInfo, nullptr, &pipelineLayout), VK_SUCCESS);
    
    // Create multiple compute pipelines
    std::vector<VkPipeline> pipelines(10);
    
    // No actual pipeline creation since we don't have shader modules
    // In real tests, you'd create multiple pipelines and check for leaks
    
    // Clean up
    // for (const auto& pipeline : pipelines) {
    //     if (pipeline != VK_NULL_HANDLE) {
    //         vkDestroyPipeline(engine->getDevice(), pipeline, nullptr);
    //     }
    // }
    
    vkDestroyPipelineLayout(engine->getDevice(), pipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(engine->getDevice(), setLayout, nullptr);
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
} 
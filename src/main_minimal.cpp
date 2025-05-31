// Minimal Vulkan Application - Uses only confirmed working components
#include <iostream>
#include <stdexcept>
#include <memory>
#include <chrono>
#include <thread>
#include <GLFW/glfw3.h>  // Add GLFW header for extension functions
#include <algorithm>
#include <array>

// GLM for 3D math
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// Only include headers we know exist
#include "WindowManager.h"
#include "VulkanContext.h"
#include "DeviceManager.h"
// #include "VulkanMemoryManager.h"  // Temporarily disabled due to VMA linking issues
#include "Logger.h"
#include "Camera.h"

// Add Vulkan rendering components
// #include "vulkan/resources/VulkanSwapChain.h"  // Disabled - incomplete implementation
#include "vulkan/resources/ShaderManager.h"

using namespace VulkanHIP;

class MinimalVulkanApp {
public:
    MinimalVulkanApp() {
        std::cout << "Starting Minimal Vulkan Application" << std::endl;
    }

    ~MinimalVulkanApp() {
        cleanup();
    }

    void run() {
        try {
            initWindow();
            initVulkan();
            initRendering();
            mainLoop();
        }
        catch (const std::exception& e) {
            std::cerr << "Application error: " << e.what() << std::endl;
            throw;
        }
    }

private:
    // Use references to singletons instead of unique_ptr
    WindowManager* windowManager = nullptr;
    VulkanContext* vulkanContext = nullptr;
    // std::unique_ptr<VulkanMemoryManager> memoryManager;  // Temporarily disabled
    
    // Camera for 3D rendering
    std::unique_ptr<Camera> camera;
    
    // Rendering components - simplified inline swapchain
    std::unique_ptr<ShaderManager> shaderManager;
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    std::vector<VkImage> swapchainImages;
    std::vector<VkImageView> swapchainImageViews;
    std::vector<VkFramebuffer> framebuffers;
    VkFormat swapchainImageFormat;
    VkExtent2D swapchainExtent;
    VkRenderPass renderPass = VK_NULL_HANDLE;
    VkPipeline graphicsPipeline = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> commandBuffers;
    
    // 3D rendering support
    struct UniformBufferObject {
        glm::mat4 model;
        glm::mat4 view;
        glm::mat4 proj;
    };
    
    struct Vertex {
        glm::vec3 pos;
        glm::vec3 color;
    };
    
    VkBuffer vertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory vertexBufferMemory = VK_NULL_HANDLE;
    std::vector<VkBuffer> uniformBuffers;
    std::vector<VkDeviceMemory> uniformBuffersMemory;
    std::vector<void*> uniformBuffersMapped;
    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> descriptorSets;
    
    // Frame synchronization
    static const int MAX_FRAMES_IN_FLIGHT = 2;
    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;
    size_t currentFrame = 0;
    
    // Timing
    std::chrono::steady_clock::time_point startTime = std::chrono::steady_clock::now();
    
    // Cube geometry data
    const std::vector<Vertex> cubeVertices = {
        // Front face (red)
        {{-0.5f, -0.5f,  0.5f}, {1.0f, 0.0f, 0.0f}},
        {{ 0.5f, -0.5f,  0.5f}, {1.0f, 0.0f, 0.0f}},
        {{ 0.5f,  0.5f,  0.5f}, {1.0f, 0.0f, 0.0f}},
        {{-0.5f,  0.5f,  0.5f}, {1.0f, 0.0f, 0.0f}},
        
        // Back face (green)
        {{-0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
        {{ 0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
        {{ 0.5f,  0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
        {{-0.5f,  0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}}
    };
    
    const std::vector<uint16_t> cubeIndices = {
        0, 1, 2, 2, 3, 0,   // Front face
        4, 5, 6, 6, 7, 4,   // Back face
        7, 6, 2, 2, 3, 7,   // Top face
        0, 1, 5, 5, 4, 0,   // Bottom face
        0, 4, 7, 7, 3, 0,   // Left face
        1, 5, 6, 6, 2, 1    // Right face
    };
    
    void initWindow() {
        std::cout << "Initializing window..." << std::endl;
        
        // Get singleton instance
        windowManager = &WindowManager::getInstance();
        
        // Initialize window
        WindowManager::WindowConfig config{};
        config.width = 1280;
        config.height = 720;
        config.title = "Vulkan HIP Engine - Minimal Build";
        
        windowManager->init(config);
        
        std::cout << "Window created: " << config.width << "x" << config.height << std::endl;
        
        // Initialize camera
        camera = std::make_unique<Camera>(windowManager->getWindow(), 45.0f, 0.1f, 100.0f);
        camera->setPosition(glm::vec3(2.0f, 2.0f, 2.0f));
        camera->setMode(CameraMode::Fly);
        
        std::cout << "Camera initialized" << std::endl;
    }
    
    void initVulkan() {
        std::cout << "Initializing Vulkan..." << std::endl;
        
        // Get singleton instance
        vulkanContext = &VulkanContext::getInstance();
        
        // Get required GLFW extensions for surface support
        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
        
        std::vector<const char*> extensions;
        for (uint32_t i = 0; i < glfwExtensionCount; i++) {
            extensions.push_back(glfwExtensions[i]);
            std::cout << "Adding required extension: " << glfwExtensions[i] << std::endl;
        }
        
        vulkanContext->init(extensions);
        
        // Skip memory manager for now
        // memoryManager = std::make_unique<VulkanMemoryManager>(
        //     vulkanContext->getDevice(),
        //     vulkanContext->getPhysicalDevice()
        // );
        
        std::cout << "Vulkan initialization complete" << std::endl;
    }
    
    void initRendering() {
        std::cout << "Initializing rendering..." << std::endl;
        
        // Create swapchain manually
        createSwapchain();
        createImageViews();
        createRenderPass();
        
        // Create descriptor set layout
        createDescriptorSetLayout();
        
        // Create shader manager
        shaderManager = std::make_unique<ShaderManager>(vulkanContext);
        
        // Create graphics pipeline
        createGraphicsPipeline();
        
        // Create framebuffers
        createFramebuffers();
        
        // Create vertex buffer
        createVertexBuffer();
        
        // Create uniform buffers
        createUniformBuffers();
        
        // Create descriptor pool and sets
        createDescriptorPool();
        createDescriptorSets();
        
        // Create command buffers
        createCommandBuffers();
        
        // Initialize frame synchronization
        imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
        
        createSyncObjects();
        
        std::cout << "Rendering initialization complete" << std::endl;
    }
    
    void createSwapchain() {
        // Query swapchain support
        SwapChainSupportDetails swapChainSupport = vulkanContext->querySwapChainSupport(vulkanContext->getPhysicalDevice());
        
        VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
        VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
        VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);
        
        uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
        if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
            imageCount = swapChainSupport.capabilities.maxImageCount;
        }
        
        VkSwapchainCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = vulkanContext->getSurface();
        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageExtent = extent;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        
        QueueFamilyIndices indices = vulkanContext->getQueueFamilyIndices();
        uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(), indices.presentFamily.value()};
        
        if (indices.graphicsFamily != indices.presentFamily) {
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = 2;
            createInfo.pQueueFamilyIndices = queueFamilyIndices;
        } else {
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        }
        
        createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        createInfo.presentMode = presentMode;
        createInfo.clipped = VK_TRUE;
        createInfo.oldSwapchain = VK_NULL_HANDLE;
        
        if (vkCreateSwapchainKHR(vulkanContext->getDevice(), &createInfo, nullptr, &swapchain) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create swap chain!");
        }
        
        // Get swapchain images
        vkGetSwapchainImagesKHR(vulkanContext->getDevice(), swapchain, &imageCount, nullptr);
        swapchainImages.resize(imageCount);
        vkGetSwapchainImagesKHR(vulkanContext->getDevice(), swapchain, &imageCount, swapchainImages.data());
        
        swapchainImageFormat = surfaceFormat.format;
        swapchainExtent = extent;
        
        std::cout << "Swapchain created with " << imageCount << " images" << std::endl;
    }
    
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
        for (const auto& availableFormat : availableFormats) {
            if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && 
                availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                return availableFormat;
            }
        }
        return availableFormats[0];
    }
    
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
        for (const auto& availablePresentMode : availablePresentModes) {
            if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
                return availablePresentMode;
            }
        }
        return VK_PRESENT_MODE_FIFO_KHR;
    }
    
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) {
        if (capabilities.currentExtent.width != UINT32_MAX) {
            return capabilities.currentExtent;
        } else {
            int width, height;
            glfwGetFramebufferSize(windowManager->getWindow(), &width, &height);
            
            VkExtent2D actualExtent = {
                static_cast<uint32_t>(width),
                static_cast<uint32_t>(height)
            };
            
            actualExtent.width = std::clamp(actualExtent.width,
                capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
            actualExtent.height = std::clamp(actualExtent.height,
                capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
            
            return actualExtent;
        }
    }
    
    void createImageViews() {
        swapchainImageViews.resize(swapchainImages.size());
        
        for (size_t i = 0; i < swapchainImages.size(); i++) {
            VkImageViewCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            createInfo.image = swapchainImages[i];
            createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            createInfo.format = swapchainImageFormat;
            createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            createInfo.subresourceRange.baseMipLevel = 0;
            createInfo.subresourceRange.levelCount = 1;
            createInfo.subresourceRange.baseArrayLayer = 0;
            createInfo.subresourceRange.layerCount = 1;
            
            if (vkCreateImageView(vulkanContext->getDevice(), &createInfo, nullptr, &swapchainImageViews[i]) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create image views!");
            }
        }
        
        std::cout << "Created " << swapchainImageViews.size() << " image views" << std::endl;
    }
    
    void createRenderPass() {
        VkAttachmentDescription colorAttachment{};
        colorAttachment.format = swapchainImageFormat;
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        
        VkAttachmentReference colorAttachmentRef{};
        colorAttachmentRef.attachment = 0;
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        
        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorAttachmentRef;
        
        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        
        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = 1;
        renderPassInfo.pAttachments = &colorAttachment;
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies = &dependency;
        
        if (vkCreateRenderPass(vulkanContext->getDevice(), &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create render pass!");
        }
        
        std::cout << "Render pass created successfully" << std::endl;
    }
    
    void createFramebuffers() {
        framebuffers.resize(swapchainImageViews.size());
        
        for (size_t i = 0; i < swapchainImageViews.size(); i++) {
            VkImageView attachments[] = {
                swapchainImageViews[i]
            };
            
            VkFramebufferCreateInfo framebufferInfo{};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = renderPass;
            framebufferInfo.attachmentCount = 1;
            framebufferInfo.pAttachments = attachments;
            framebufferInfo.width = swapchainExtent.width;
            framebufferInfo.height = swapchainExtent.height;
            framebufferInfo.layers = 1;
            
            if (vkCreateFramebuffer(vulkanContext->getDevice(), &framebufferInfo, nullptr, &framebuffers[i]) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create framebuffer!");
            }
        }
        
        std::cout << "Created " << framebuffers.size() << " framebuffers" << std::endl;
    }
    
    void createGraphicsPipeline() {
        // Create shader stages
        VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
        VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
        
        shaderManager->createShaderStages(
            "shaders/minimal.vert.spv",
            "shaders/minimal.frag.spv",
            vertShaderStageInfo,
            fragShaderStageInfo
        );
        
        VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};
        
        // Vertex input (now with actual vertex data)
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(Vertex);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        
        std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions{};
        
        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[0].offset = offsetof(Vertex, pos);
        
        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[1].offset = offsetof(Vertex, color);
        
        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = 1;
        vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
        vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
        vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();
        
        // Input assembly
        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssembly.primitiveRestartEnable = VK_FALSE;
        
        // Viewport and scissor
        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (float) swapchainExtent.width;
        viewport.height = (float) swapchainExtent.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        
        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = swapchainExtent;
        
        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.pViewports = &viewport;
        viewportState.scissorCount = 1;
        viewportState.pScissors = &scissor;
        
        // Rasterizer
        VkPipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
        rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
        rasterizer.depthBiasEnable = VK_FALSE;
        
        // Multisampling (disabled)
        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        
        // Color blending
        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | 
                                             VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_FALSE;
        
        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable = VK_FALSE;
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;
        
        // Pipeline layout
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;
        pipelineLayoutInfo.pushConstantRangeCount = 0;
        
        if (vkCreatePipelineLayout(vulkanContext->getDevice(), &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create pipeline layout!");
        }
        
        // Create graphics pipeline
        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = shaderStages;
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.layout = pipelineLayout;
        pipelineInfo.renderPass = renderPass;
        pipelineInfo.subpass = 0;
        
        if (vkCreateGraphicsPipelines(vulkanContext->getDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create graphics pipeline!");
        }
        
        std::cout << "Graphics pipeline created successfully" << std::endl;
    }
    
    void createCommandBuffers() {
        size_t imageCount = swapchainImages.size();
        commandBuffers.resize(imageCount);
        
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = vulkanContext->getGraphicsCommandPool();
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());
        
        if (vkAllocateCommandBuffers(vulkanContext->getDevice(), &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate command buffers!");
        }
        
        // Record command buffers
        for (size_t i = 0; i < commandBuffers.size(); i++) {
            recordCommandBuffer(commandBuffers[i], i);
        }
        
        std::cout << "Command buffers created and recorded" << std::endl;
    }
    
    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) {
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        
        if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
            throw std::runtime_error("Failed to begin recording command buffer!");
        }
        
        // Begin render pass
        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = renderPass;
        renderPassInfo.framebuffer = framebuffers[imageIndex];
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = swapchainExtent;
        
        VkClearValue clearColor = {{{0.1f, 0.2f, 0.4f, 1.0f}}}; // Dark blue background
        renderPassInfo.clearValueCount = 1;
        renderPassInfo.pClearValues = &clearColor;
        
        vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
        
        // Bind graphics pipeline
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
        
        // Bind vertex buffer
        VkBuffer vertexBuffers[] = {vertexBuffer};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
        
        // Bind descriptor set (will be updated for each frame in renderFrame)
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets[imageIndex], 0, nullptr);
        
        // Draw cube (36 vertices for 12 triangles)
        vkCmdDraw(commandBuffer, static_cast<uint32_t>(cubeIndices.size()), 1, 0, 0);
        
        // End render pass
        vkCmdEndRenderPass(commandBuffer);
        
        if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to record command buffer!");
        }
    }
    
    void createSyncObjects() {
        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        
        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            if (vkCreateSemaphore(vulkanContext->getDevice(), &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
                vkCreateSemaphore(vulkanContext->getDevice(), &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS ||
                vkCreateFence(vulkanContext->getDevice(), &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create synchronization objects for a frame!");
            }
        }
        
        std::cout << "Synchronization objects created for " << MAX_FRAMES_IN_FLIGHT << " frames" << std::endl;
    }
    
    void mainLoop() {
        std::cout << "Entering main loop..." << std::endl;
        
        auto lastTime = std::chrono::steady_clock::now();
        
        while (!windowManager->shouldClose()) {
            windowManager->pollEvents();
            
            // Calculate delta time
            auto currentTime = std::chrono::steady_clock::now();
            float deltaTime = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - lastTime).count();
            lastTime = currentTime;
            
            // Update camera
            camera->update(deltaTime);
            
            // Render frame
            renderFrame();
        }
        
        // Wait for device to finish before cleanup
        vkDeviceWaitIdle(vulkanContext->getDevice());
    }
    
    void renderFrame() {
        // Wait for the previous frame to finish
        vkWaitForFences(vulkanContext->getDevice(), 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);
        vkResetFences(vulkanContext->getDevice(), 1, &inFlightFences[currentFrame]);
        
        // Acquire next image
        uint32_t imageIndex;
        VkResult result = vkAcquireNextImageKHR(vulkanContext->getDevice(), swapchain, 
                                               UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);
        
        if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            std::cerr << "Failed to acquire swap chain image! Error: " << result << std::endl;
            return;
        }
        
        // Update uniform buffer
        updateUniformBuffer(currentFrame);
        
        // Submit command buffer
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        
        VkSemaphore waitSemaphores[] = {imageAvailableSemaphores[currentFrame]};
        VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffers[imageIndex];
        
        VkSemaphore signalSemaphores[] = {renderFinishedSemaphores[currentFrame]};
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;
        
        if (vkQueueSubmit(vulkanContext->getGraphicsQueue(), 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to submit draw command buffer!");
        }
        
        // Present
        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores;
        
        VkSwapchainKHR swapChains[] = {swapchain};
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapChains;
        presentInfo.pImageIndices = &imageIndex;
        
        result = vkQueuePresentKHR(vulkanContext->getPresentQueue(), &presentInfo);
        
        if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            std::cerr << "Failed to present swap chain image! Error: " << result << std::endl;
        }
        
        // Update current frame
        currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    }
    
    void cleanup() {
        std::cout << "Cleaning up resources..." << std::endl;
        
        // Wait for device to finish
        if (vulkanContext) {
            vkDeviceWaitIdle(vulkanContext->getDevice());
        }
        
        // Cleanup rendering resources in reverse order
        if (graphicsPipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(vulkanContext->getDevice(), graphicsPipeline, nullptr);
            graphicsPipeline = VK_NULL_HANDLE;
        }
        
        if (pipelineLayout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(vulkanContext->getDevice(), pipelineLayout, nullptr);
            pipelineLayout = VK_NULL_HANDLE;
        }
        
        // Command buffers are automatically freed when command pool is destroyed
        commandBuffers.clear();
        
        // Cleanup synchronization objects
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            vkDestroySemaphore(vulkanContext->getDevice(), imageAvailableSemaphores[i], nullptr);
            vkDestroySemaphore(vulkanContext->getDevice(), renderFinishedSemaphores[i], nullptr);
            vkDestroyFence(vulkanContext->getDevice(), inFlightFences[i], nullptr);
        }
        
        // Cleanup 3D rendering resources
        if (descriptorPool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(vulkanContext->getDevice(), descriptorPool, nullptr);
            descriptorPool = VK_NULL_HANDLE;
        }
        
        if (descriptorSetLayout != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(vulkanContext->getDevice(), descriptorSetLayout, nullptr);
            descriptorSetLayout = VK_NULL_HANDLE;
        }
        
        // Cleanup uniform buffers
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            if (uniformBuffers[i] != VK_NULL_HANDLE) {
                vkDestroyBuffer(vulkanContext->getDevice(), uniformBuffers[i], nullptr);
                vkFreeMemory(vulkanContext->getDevice(), uniformBuffersMemory[i], nullptr);
            }
        }
        
        // Cleanup vertex buffer
        if (vertexBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(vulkanContext->getDevice(), vertexBuffer, nullptr);
            vkFreeMemory(vulkanContext->getDevice(), vertexBufferMemory, nullptr);
        }
        
        // Cleanup framebuffers
        for (auto framebuffer : framebuffers) {
            vkDestroyFramebuffer(vulkanContext->getDevice(), framebuffer, nullptr);
        }
        framebuffers.clear();
        
        if (renderPass != VK_NULL_HANDLE) {
            vkDestroyRenderPass(vulkanContext->getDevice(), renderPass, nullptr);
            renderPass = VK_NULL_HANDLE;
        }
        
        // Cleanup image views
        for (auto imageView : swapchainImageViews) {
            vkDestroyImageView(vulkanContext->getDevice(), imageView, nullptr);
        }
        swapchainImageViews.clear();
        
        // Cleanup swapchain
        if (swapchain != VK_NULL_HANDLE) {
            vkDestroySwapchainKHR(vulkanContext->getDevice(), swapchain, nullptr);
            swapchain = VK_NULL_HANDLE;
        }
        
        // Cleanup shader manager
        if (shaderManager) {
            shaderManager->cleanup();
            shaderManager.reset();
        }
        
        // Cleanup core Vulkan components
        // memoryManager.reset();  // Temporarily disabled
        
        if (vulkanContext) {
            vulkanContext->cleanup();
        }
        
        if (windowManager) {
            windowManager->cleanup();
        }
        
        std::cout << "Cleanup complete" << std::endl;
    }

    void createDescriptorSetLayout() {
        VkDescriptorSetLayoutBinding uboLayoutBinding{};
        uboLayoutBinding.binding = 0;
        uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uboLayoutBinding.descriptorCount = 1;
        uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        uboLayoutBinding.pImmutableSamplers = nullptr;
        
        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = 1;
        layoutInfo.pBindings = &uboLayoutBinding;
        
        if (vkCreateDescriptorSetLayout(vulkanContext->getDevice(), &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create descriptor set layout!");
        }
        
        std::cout << "Descriptor set layout created" << std::endl;
    }
    
    void createVertexBuffer() {
        VkDeviceSize bufferSize = sizeof(cubeVertices[0]) * cubeVertices.size();
        
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = bufferSize;
        bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        
        if (vkCreateBuffer(vulkanContext->getDevice(), &bufferInfo, nullptr, &vertexBuffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create vertex buffer!");
        }
        
        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(vulkanContext->getDevice(), vertexBuffer, &memRequirements);
        
        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = VulkanContext::findMemoryType(vulkanContext->getPhysicalDevice(), 
            memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        
        if (vkAllocateMemory(vulkanContext->getDevice(), &allocInfo, nullptr, &vertexBufferMemory) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate vertex buffer memory!");
        }
        
        vkBindBufferMemory(vulkanContext->getDevice(), vertexBuffer, vertexBufferMemory, 0);
        
        void* data;
        vkMapMemory(vulkanContext->getDevice(), vertexBufferMemory, 0, bufferSize, 0, &data);
        memcpy(data, cubeVertices.data(), (size_t) bufferSize);
        vkUnmapMemory(vulkanContext->getDevice(), vertexBufferMemory);
        
        std::cout << "Vertex buffer created with " << cubeVertices.size() << " vertices" << std::endl;
    }
    
    void createUniformBuffers() {
        VkDeviceSize bufferSize = sizeof(UniformBufferObject);
        
        uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
        uniformBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);
        uniformBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);
        
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            VkBufferCreateInfo bufferInfo{};
            bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            bufferInfo.size = bufferSize;
            bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
            bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            
            if (vkCreateBuffer(vulkanContext->getDevice(), &bufferInfo, nullptr, &uniformBuffers[i]) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create uniform buffer!");
            }
            
            VkMemoryRequirements memRequirements;
            vkGetBufferMemoryRequirements(vulkanContext->getDevice(), uniformBuffers[i], &memRequirements);
            
            VkMemoryAllocateInfo allocInfo{};
            allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            allocInfo.allocationSize = memRequirements.size;
            allocInfo.memoryTypeIndex = VulkanContext::findMemoryType(vulkanContext->getPhysicalDevice(),
                memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
            
            if (vkAllocateMemory(vulkanContext->getDevice(), &allocInfo, nullptr, &uniformBuffersMemory[i]) != VK_SUCCESS) {
                throw std::runtime_error("Failed to allocate uniform buffer memory!");
            }
            
            vkBindBufferMemory(vulkanContext->getDevice(), uniformBuffers[i], uniformBuffersMemory[i], 0);
            vkMapMemory(vulkanContext->getDevice(), uniformBuffersMemory[i], 0, bufferSize, 0, &uniformBuffersMapped[i]);
        }
        
        std::cout << "Uniform buffers created for " << MAX_FRAMES_IN_FLIGHT << " frames" << std::endl;
    }
    
    void createDescriptorPool() {
        VkDescriptorPoolSize poolSize{};
        poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSize.descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
        
        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes = &poolSize;
        poolInfo.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
        
        if (vkCreateDescriptorPool(vulkanContext->getDevice(), &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create descriptor pool!");
        }
        
        std::cout << "Descriptor pool created" << std::endl;
    }
    
    void createDescriptorSets() {
        std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, descriptorSetLayout);
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = descriptorPool;
        allocInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
        allocInfo.pSetLayouts = layouts.data();
        
        descriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
        if (vkAllocateDescriptorSets(vulkanContext->getDevice(), &allocInfo, descriptorSets.data()) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate descriptor sets!");
        }
        
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            VkDescriptorBufferInfo bufferInfo{};
            bufferInfo.buffer = uniformBuffers[i];
            bufferInfo.offset = 0;
            bufferInfo.range = sizeof(UniformBufferObject);
            
            VkWriteDescriptorSet descriptorWrite{};
            descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrite.dstSet = descriptorSets[i];
            descriptorWrite.dstBinding = 0;
            descriptorWrite.dstArrayElement = 0;
            descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            descriptorWrite.descriptorCount = 1;
            descriptorWrite.pBufferInfo = &bufferInfo;
            
            vkUpdateDescriptorSets(vulkanContext->getDevice(), 1, &descriptorWrite, 0, nullptr);
        }
        
        std::cout << "Descriptor sets created and updated" << std::endl;
    }
    
    void updateUniformBuffer(uint32_t currentImage) {
        auto currentTime = std::chrono::steady_clock::now();
        float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();
        
        UniformBufferObject ubo{};
        
        // Model matrix (rotate cube)
        ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        
        // View matrix from camera
        ubo.view = camera->getViewMatrix();
        
        // Projection matrix from camera
        ubo.proj = camera->getProjectionMatrix();
        ubo.proj[1][1] *= -1; // GLM was designed for OpenGL, flip Y for Vulkan
        
        memcpy(uniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
    }
};

int main() {
    try {
        // Initialize logger if available
        std::cout << "Starting Vulkan HIP Engine - Minimal Build" << std::endl;
        
        // Run application
        MinimalVulkanApp app;
        app.run();
        
        std::cout << "Application exited successfully" << std::endl;
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
} 
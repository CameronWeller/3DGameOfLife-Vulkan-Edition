#include "render/Renderer.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <stdexcept>

#include "imgui.h"
#include "imgui_impl_vulkan.h"

#include "gpu/Barriers.h"
#include "gpu/Descriptors.h"
#include "gpu/GpuContext.h"
#include "gpu/ShaderModule.h"
#include "util/Png.h"
#include "world/BlockInstances.h"

namespace gol3d {
namespace {

struct CubeVertex {
    glm::vec3 position; // corner of a unit cube centered on the origin
    glm::vec3 normal;
};

// Frame descriptor set bindings; see life3d_frame.glsl and the vertex shaders.
enum FrameBinding : uint32_t {
    FrameUniformsBinding = 0,
    BlockInstancesBinding = 1,
    BoxesBinding = 2,
    ChunkOriginsBinding = 3,
    FrameBindingCount,
};

constexpr uint64_t ACQUIRE_TIMEOUT_NS = 4'000'000'000;
constexpr VkShaderStageFlags DRAW_STAGES =
    VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

} // namespace

// ------------------------------------------------------------------- setup

void Renderer::initSwapchain(const GpuContext& gpu, BufferAllocator& allocator,
                             GLFWwindow* window) {
    gpu_ = &gpu;
    allocator_ = &allocator;
    swapchain_.create(gpu, allocator, window);
}

void Renderer::initPipelines(const std::filesystem::path& shaderDir, const GpuBuffer& instances,
                             const GpuBuffer& indirectDraw, const GpuBuffer& origins) {
    shaderDir_ = shaderDir;
    indirectDraw_ = &indirectDraw;
    createGeometry();
    createFrameBuffers();
    createLayouts();
    // Blocks and outlines are solid geometry: depth-tested and depth-writing.
    blockPipeline_ = createPipeline({.vertexShader = "life3d_blocks.vert.spv",
                                     .fragmentShader = "life3d_world.frag.spv",
                                     .depthTest = true,
                                     .depthWrite = true});
    boxPipeline_ = createPipeline({.vertexShader = "life3d_boxes.vert.spv",
                                   .fragmentShader = "life3d_world.frag.spv",
                                   .cubeVertices = true,
                                   .depthTest = true,
                                   .depthWrite = true});
    // The fullscreen passes: the sky behind everything, the ground grid hidden
    // by blocks in front of it, and the HUD over everything.
    skyPipeline_ = createPipeline(
        {.vertexShader = "life3d_screen.vert.spv", .fragmentShader = "life3d_screen.frag.spv"});
    gridPipeline_ = createPipeline({.vertexShader = "life3d_screen.vert.spv",
                                    .fragmentShader = "life3d_screen.frag.spv",
                                    .depthTest = true,
                                    .alphaBlend = true});
    hudPipeline_ = createPipeline({.vertexShader = "life3d_screen.vert.spv",
                                   .fragmentShader = "life3d_screen.frag.spv",
                                   .alphaBlend = true});
    createDescriptorSets();
    writeDescriptors(instances, origins);
    createCommandBuffers();
    createSyncObjects();
}

void Renderer::destroy() {
    if (!gpu_) return;
    VkDevice device = gpu_->device;
    allocator_->destroy(captureBuffer_);
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroySemaphore(device, imageAvailable_[i], nullptr);
        vkDestroyFence(device, inFlight_[i], nullptr);
        allocator_->destroy(uniformBuffers_[i]);
        allocator_->destroy(boxBuffers_[i]);
    }
    for (VkPipeline pipeline :
         {blockPipeline_, boxPipeline_, skyPipeline_, gridPipeline_, hudPipeline_}) {
        vkDestroyPipeline(device, pipeline, nullptr);
    }
    vkDestroyPipelineLayout(device, pipelineLayout_, nullptr);
    vkDestroyDescriptorPool(device, descriptorPool_, nullptr);
    vkDestroyDescriptorSetLayout(device, setLayout_, nullptr);
    allocator_->destroy(cubeVertices_);
    allocator_->destroy(blockIndices_);
    swapchain_.destroy();
    gpu_ = nullptr;
}

void Renderer::createGeometry() {
    // A unit cube as 12 triangles with per-face normals, for box outlines.
    std::vector<CubeVertex> vertices;
    const glm::vec3 faceNormals[6] = {glm::vec3(1, 0, 0), glm::vec3(-1, 0, 0),
                                      glm::vec3(0, 1, 0), glm::vec3(0, -1, 0),
                                      glm::vec3(0, 0, 1), glm::vec3(0, 0, -1)};
    for (const glm::vec3& normal : faceNormals) {
        glm::vec3 u = std::abs(normal.y) > 0.5f ? glm::vec3(1, 0, 0) : glm::vec3(0, 1, 0);
        glm::vec3 w = glm::cross(normal, u);
        glm::vec3 center = normal * 0.5f;
        const glm::vec3 corners[4] = {center - 0.5f * u - 0.5f * w, center + 0.5f * u - 0.5f * w,
                                      center + 0.5f * u + 0.5f * w, center - 0.5f * u + 0.5f * w};
        for (int corner : {0, 1, 2, 0, 2, 3}) {
            vertices.push_back({corners[corner], normal});
        }
    }
    cubeVertexCount_ = static_cast<uint32_t>(vertices.size());
    VkDeviceSize vertexBytes = sizeof(CubeVertex) * vertices.size();
    allocator_->create(cubeVertices_, vertexBytes, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                       {memory::DEVICE_HOST, memory::HOST});
    std::memcpy(cubeVertices_.mapped, vertices.data(), vertexBytes);

    // Blocks: each of the three camera-facing quads is two triangles, corners
    // 0-1-2 and 0-2-3 (life3d_blocks.vert generates the corner positions).
    std::array<uint16_t, BLOCK_INDEX_COUNT> indices{};
    const int quadCorners[6] = {0, 1, 2, 0, 2, 3};
    for (uint16_t face = 0; face < 3; ++face) {
        for (int i = 0; i < 6; ++i) {
            indices[face * 6 + i] = static_cast<uint16_t>(face * 4 + quadCorners[i]);
        }
    }
    allocator_->create(blockIndices_, sizeof(indices), VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                       {memory::DEVICE_HOST, memory::HOST});
    std::memcpy(blockIndices_.mapped, indices.data(), sizeof(indices));
}

void Renderer::createFrameBuffers() {
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        allocator_->create(uniformBuffers_[i], sizeof(FrameUniforms),
                           VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, {memory::DEVICE_HOST, memory::HOST});
        allocator_->create(boxBuffers_[i], static_cast<VkDeviceSize>(MAX_BOXES) * sizeof(Box),
                           usage::STORAGE, {memory::DEVICE_HOST, memory::HOST});
    }
}

void Renderer::createLayouts() {
    std::array<VkDescriptorSetLayoutBinding, FrameBindingCount> bindings{};
    bindings[FrameUniformsBinding] = {FrameUniformsBinding, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
                                      DRAW_STAGES, nullptr};
    bindings[BlockInstancesBinding] = {BlockInstancesBinding, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1,
                                       VK_SHADER_STAGE_VERTEX_BIT, nullptr};
    bindings[BoxesBinding] = {BoxesBinding, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1,
                              VK_SHADER_STAGE_VERTEX_BIT, nullptr};
    bindings[ChunkOriginsBinding] = {ChunkOriginsBinding, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1,
                                     VK_SHADER_STAGE_VERTEX_BIT, nullptr};
    VkDescriptorSetLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();
    if (vkCreateDescriptorSetLayout(gpu_->device, &layoutInfo, nullptr, &setLayout_) !=
        VK_SUCCESS) {
        throw std::runtime_error("Could not create descriptor set layout.");
    }

    // One push constant: the screen shaders' mode.
    VkPushConstantRange pushRange{DRAW_STAGES, 0, sizeof(uint32_t)};
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &setLayout_;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushRange;
    if (vkCreatePipelineLayout(gpu_->device, &pipelineLayoutInfo, nullptr, &pipelineLayout_) !=
        VK_SUCCESS) {
        throw std::runtime_error("Could not create pipeline layout.");
    }
}

VkPipelineRenderingCreateInfo Renderer::renderingInfo() const {
    VkPipelineRenderingCreateInfo info{VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
    info.colorAttachmentCount = 1;
    info.pColorAttachmentFormats = &swapchain_.colorFormat(); // lives as long as the swapchain
    info.depthAttachmentFormat = swapchain_.depthFormat();
    return info;
}

VkPipeline Renderer::createPipeline(const PipelineSpec& spec) {
    VkDevice device = gpu_->device;
    VkShaderModule vertexModule = loadShaderModule(device, shaderDir_ / spec.vertexShader);
    VkShaderModule fragmentModule = loadShaderModule(device, shaderDir_ / spec.fragmentShader);
    std::array<VkPipelineShaderStageCreateInfo, 2> stages{};
    stages[0] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                 nullptr,
                 0,
                 VK_SHADER_STAGE_VERTEX_BIT,
                 vertexModule,
                 "main",
                 nullptr};
    stages[1] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                 nullptr,
                 0,
                 VK_SHADER_STAGE_FRAGMENT_BIT,
                 fragmentModule,
                 "main",
                 nullptr};

    VkVertexInputBindingDescription binding{0, sizeof(CubeVertex), VK_VERTEX_INPUT_RATE_VERTEX};
    std::array<VkVertexInputAttributeDescription, 2> attributes{{
        {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(CubeVertex, position)},
        {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(CubeVertex, normal)},
    }};
    VkPipelineVertexInputStateCreateInfo vertexInput{
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    if (spec.cubeVertices) {
        vertexInput.vertexBindingDescriptionCount = 1;
        vertexInput.pVertexBindingDescriptions = &binding;
        vertexInput.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributes.size());
        vertexInput.pVertexAttributeDescriptions = attributes.data();
    }

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo viewportState{
        VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer{
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_NONE; // blocks only emit camera-facing faces
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    VkPipelineMultisampleStateCreateInfo multisampling{
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthStencil{
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    depthStencil.depthTestEnable = spec.depthTest;
    depthStencil.depthWriteEnable = spec.depthWrite;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

    VkPipelineColorBlendAttachmentState blendAttachment{};
    blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                     VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    if (spec.alphaBlend) {
        // Standard "over" blending: color = src * srcAlpha + dst * (1 - srcAlpha).
        blendAttachment.blendEnable = VK_TRUE;
        blendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        blendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        blendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        blendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        blendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        blendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
    }
    VkPipelineColorBlendStateCreateInfo colorBlending{
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &blendAttachment;

    // Viewport and scissor are set per frame, so a resize needs no new pipelines.
    std::array<VkDynamicState, 2> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT,
                                                   VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicState{
        VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    VkPipelineRenderingCreateInfo rendering = renderingInfo();
    VkGraphicsPipelineCreateInfo pipelineInfo{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
                                              &rendering};
    pipelineInfo.stageCount = static_cast<uint32_t>(stages.size());
    pipelineInfo.pStages = stages.data();
    pipelineInfo.pVertexInputState = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = pipelineLayout_;
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkResult result =
        vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline);
    vkDestroyShaderModule(device, vertexModule, nullptr);
    vkDestroyShaderModule(device, fragmentModule, nullptr);
    if (result != VK_SUCCESS) throw std::runtime_error("Could not create graphics pipeline.");
    return pipeline;
}

void Renderer::createDescriptorSets() {
    std::array<VkDescriptorPoolSize, 2> poolSizes{{
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, MAX_FRAMES_IN_FLIGHT},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, MAX_FRAMES_IN_FLIGHT * 3},
    }};
    VkDescriptorPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = MAX_FRAMES_IN_FLIGHT;
    if (vkCreateDescriptorPool(gpu_->device, &poolInfo, nullptr, &descriptorPool_) != VK_SUCCESS) {
        throw std::runtime_error("Could not create descriptor pool.");
    }
    for (VkDescriptorSet& set : descriptorSets_) {
        VkDescriptorSetAllocateInfo allocInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        allocInfo.descriptorPool = descriptorPool_;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &setLayout_;
        if (vkAllocateDescriptorSets(gpu_->device, &allocInfo, &set) != VK_SUCCESS) {
            throw std::runtime_error("Could not allocate descriptor set.");
        }
    }
}

void Renderer::writeDescriptors(const GpuBuffer& instances, const GpuBuffer& origins) {
    VkDevice device = gpu_->device;
    for (int frame = 0; frame < MAX_FRAMES_IN_FLIGHT; ++frame) {
        VkDescriptorSet set = descriptorSets_[frame];
        writeBufferDescriptor(device, set, FrameUniformsBinding, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                              uniformBuffers_[frame].buffer);
        writeBufferDescriptor(device, set, BlockInstancesBinding, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                              instances.buffer);
        writeBufferDescriptor(device, set, BoxesBinding, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                              boxBuffers_[frame].buffer);
        writeBufferDescriptor(device, set, ChunkOriginsBinding, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                              origins.buffer);
    }
}

void Renderer::createCommandBuffers() {
    VkCommandBufferAllocateInfo allocInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    allocInfo.commandPool = gpu_->commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = MAX_FRAMES_IN_FLIGHT;
    if (vkAllocateCommandBuffers(gpu_->device, &allocInfo, commandBuffers_.data()) != VK_SUCCESS) {
        throw std::runtime_error("Could not allocate command buffers.");
    }
}

void Renderer::createSyncObjects() {
    VkSemaphoreCreateInfo semaphoreInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; // the first wait on each slot returns at once
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateSemaphore(gpu_->device, &semaphoreInfo, nullptr, &imageAvailable_[i]) !=
                VK_SUCCESS ||
            vkCreateFence(gpu_->device, &fenceInfo, nullptr, &inFlight_[i]) != VK_SUCCESS) {
            throw std::runtime_error("Could not create synchronization objects.");
        }
    }
}

// ------------------------------------------------------------------ frames

void Renderer::waitForPreviousFrame() const {
    size_t previous = (currentFrame_ + MAX_FRAMES_IN_FLIGHT - 1) % MAX_FRAMES_IN_FLIGHT;
    gpu_->waitForFence(inFlight_[previous], "a frame");
}

void Renderer::requestScreenshot(const std::string& path) {
    if (!swapchain_.captureSupported()) {
        std::cerr << "Screenshots are not supported by this swapchain." << std::endl;
        return;
    }
    pendingScreenshot_ = path;
}

bool Renderer::beginFrame() {
    gpu_->waitForFence(inFlight_[currentFrame_], "a frame");
    VkResult result =
        vkAcquireNextImageKHR(gpu_->device, swapchain_.handle(), ACQUIRE_TIMEOUT_NS,
                              imageAvailable_[currentFrame_], VK_NULL_HANDLE, &imageIndex_);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        swapchain_.recreate();
        resized_ = false;
        return false;
    }
    if (result == VK_TIMEOUT || result == VK_NOT_READY) return false; // try again next loop
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("Could not acquire swap chain image.");
    }
    vkResetFences(gpu_->device, 1, &inFlight_[currentFrame_]);
    return true;
}

void Renderer::endFrame(const FrameUniforms& uniforms, const std::vector<Box>& boxes,
                        bool drawImGui) {
    std::memcpy(uniformBuffers_[currentFrame_].mapped, &uniforms, sizeof(uniforms));
    const uint32_t boxCount = static_cast<uint32_t>(std::min<size_t>(boxes.size(), MAX_BOXES));
    std::memcpy(boxBuffers_[currentFrame_].mapped, boxes.data(), boxCount * sizeof(Box));

    std::string screenshot;
    screenshot.swap(pendingScreenshot_);
    if (!screenshot.empty()) prepareCaptureBuffer();

    VkCommandBuffer cmd = commandBuffers_[currentFrame_];
    vkResetCommandBuffer(cmd, 0);
    recordCommands(cmd, boxCount, drawImGui, !screenshot.empty());

    // Draw once the image is available; signal the image's semaphore for presenting.
    VkSemaphoreSubmitInfo waitInfo{VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO};
    waitInfo.semaphore = imageAvailable_[currentFrame_];
    waitInfo.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSemaphoreSubmitInfo signalInfo{VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO};
    signalInfo.semaphore = swapchain_.renderFinished(imageIndex_);
    signalInfo.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    VkCommandBufferSubmitInfo commandInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO};
    commandInfo.commandBuffer = cmd;
    VkSubmitInfo2 submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO_2};
    submitInfo.waitSemaphoreInfoCount = 1;
    submitInfo.pWaitSemaphoreInfos = &waitInfo;
    submitInfo.commandBufferInfoCount = 1;
    submitInfo.pCommandBufferInfos = &commandInfo;
    submitInfo.signalSemaphoreInfoCount = 1;
    submitInfo.pSignalSemaphoreInfos = &signalInfo;
    if (vkQueueSubmit2(gpu_->queue, 1, &submitInfo, inFlight_[currentFrame_]) != VK_SUCCESS) {
        throw std::runtime_error("Could not submit draw command buffer.");
    }

    VkSemaphore renderFinished = swapchain_.renderFinished(imageIndex_);
    VkSwapchainKHR swapchain = swapchain_.handle();
    VkPresentInfoKHR presentInfo{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &renderFinished;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &swapchain;
    presentInfo.pImageIndices = &imageIndex_;
    VkResult result = vkQueuePresentKHR(gpu_->queue, &presentInfo);

    if (!screenshot.empty()) saveCapture(screenshot, currentFrame_);

    currentFrame_ = (currentFrame_ + 1) % MAX_FRAMES_IN_FLIGHT;
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || resized_) {
        swapchain_.recreate();
        resized_ = false;
    } else if (result != VK_SUCCESS) {
        throw std::runtime_error("Could not present swap chain image.");
    }
}

void Renderer::recordCommands(VkCommandBuffer cmd, uint32_t boxCount, bool drawImGui,
                              bool capture) {
    VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (vkBeginCommandBuffer(cmd, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("Could not begin recording command buffer.");
    }
    const VkImage image = swapchain_.image(imageIndex_);
    const VkExtent2D extent = swapchain_.extent();

    // The sky covers every pixel, so the image's old contents can be discarded
    // (layout UNDEFINED); the depth buffer is cleared.
    imageBarrier(
        cmd, image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        0, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);
    const VkPipelineStageFlags2 depthStages =
        VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
    imageBarrier(cmd, swapchain_.depthImage(), VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                 VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, depthStages,
                 VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, depthStages,
                 VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                     VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);

    VkRenderingAttachmentInfo colorAttachment{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
    colorAttachment.imageView = swapchain_.imageView(imageIndex_);
    colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    VkRenderingAttachmentInfo depthAttachment{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
    depthAttachment.imageView = swapchain_.depthImageView();
    depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.clearValue.depthStencil = {1.0f, 0};
    VkRenderingInfo rendering{VK_STRUCTURE_TYPE_RENDERING_INFO};
    rendering.renderArea.extent = extent;
    rendering.layerCount = 1;
    rendering.colorAttachmentCount = 1;
    rendering.pColorAttachments = &colorAttachment;
    rendering.pDepthAttachment = &depthAttachment;
    vkCmdBeginRendering(cmd, &rendering);

    VkViewport viewport{
        0.0f, 0.0f, static_cast<float>(extent.width), static_cast<float>(extent.height),
        0.0f, 1.0f};
    VkRect2D scissor{{0, 0}, extent};
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    vkCmdSetScissor(cmd, 0, 1, &scissor);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout_, 0, 1,
                            &descriptorSets_[currentFrame_], 0, nullptr);
    auto setScreenMode = [&](ScreenMode mode) {
        uint32_t value = static_cast<uint32_t>(mode);
        vkCmdPushConstants(cmd, pipelineLayout_, DRAW_STAGES, 0, sizeof(value), &value);
    };
    constexpr uint32_t FULLSCREEN_TRIANGLE = 3; // vertices

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, skyPipeline_);
    setScreenMode(ScreenMode::Sky);
    vkCmdDraw(cmd, FULLSCREEN_TRIANGLE, 1, 0, 0);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, blockPipeline_);
    vkCmdBindIndexBuffer(cmd, blockIndices_.buffer, 0, VK_INDEX_TYPE_UINT16);
    vkCmdDrawIndexedIndirect(cmd, indirectDraw_->buffer, 0, 1,
                             sizeof(VkDrawIndexedIndirectCommand));

    if (boxCount > 0) {
        constexpr uint32_t EDGES_PER_BOX = 12; // each edge is one instance of the cube, stretched
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, boxPipeline_);
        VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(cmd, 0, 1, &cubeVertices_.buffer, &offset);
        vkCmdDraw(cmd, cubeVertexCount_, boxCount * EDGES_PER_BOX, 0, 0);
    }

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, gridPipeline_);
    setScreenMode(ScreenMode::Grid);
    vkCmdDraw(cmd, FULLSCREEN_TRIANGLE, 1, 0, 0);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, hudPipeline_);
    setScreenMode(ScreenMode::Hud);
    vkCmdDraw(cmd, FULLSCREEN_TRIANGLE, 1, 0, 0);
    if (drawImGui) {
        ImDrawData* drawData = ImGui::GetDrawData();
        if (drawData && drawData->CmdLists.Size > 0) ImGui_ImplVulkan_RenderDrawData(drawData, cmd);
    }
    vkCmdEndRendering(cmd);

    if (capture) {
        // Copy the finished image into the capture buffer, then present it.
        imageBarrier(cmd, image, VK_IMAGE_ASPECT_COLOR_BIT,
                     VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                     VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                     VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_2_COPY_BIT,
                     VK_ACCESS_2_TRANSFER_READ_BIT);
        VkBufferImageCopy region{};
        region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        region.imageExtent = {extent.width, extent.height, 1};
        vkCmdCopyImageToBuffer(cmd, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                               captureBuffer_.buffer, 1, &region);
        memoryBarrier(cmd, VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
                      VK_PIPELINE_STAGE_2_HOST_BIT, VK_ACCESS_2_HOST_READ_BIT);
        imageBarrier(cmd, image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                     VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_PIPELINE_STAGE_2_COPY_BIT, 0,
                     VK_PIPELINE_STAGE_2_NONE, 0);
    } else {
        imageBarrier(cmd, image, VK_IMAGE_ASPECT_COLOR_BIT,
                     VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                     VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                     VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_2_NONE, 0);
    }

    if (vkEndCommandBuffer(cmd) != VK_SUCCESS) {
        throw std::runtime_error("Could not record command buffer.");
    }
}

// ------------------------------------------------------------- screenshots

void Renderer::prepareCaptureBuffer() {
    const VkExtent2D extent = swapchain_.extent();
    allocator_->destroy(captureBuffer_);
    allocator_->create(captureBuffer_, static_cast<VkDeviceSize>(extent.width) * extent.height * 4,
                       VK_BUFFER_USAGE_TRANSFER_DST_BIT, {memory::HOST_CACHED, memory::HOST});
}

void Renderer::saveCapture(const std::string& path, size_t frame) {
    gpu_->waitForFence(inFlight_[frame], "the screenshot frame");
    const VkExtent2D extent = swapchain_.extent();
    const size_t pixelCount = static_cast<size_t>(extent.width) * extent.height;
    const uint8_t* pixels = captureBuffer_.as<const uint8_t>();
    const VkFormat format = swapchain_.colorFormat();
    const bool bgr = format == VK_FORMAT_B8G8R8A8_SRGB || format == VK_FORMAT_B8G8R8A8_UNORM;
    std::vector<uint8_t> rgb(pixelCount * 3);
    for (size_t i = 0; i < pixelCount; ++i) {
        rgb[i * 3 + 0] = pixels[i * 4 + (bgr ? 2 : 0)];
        rgb[i * 3 + 1] = pixels[i * 4 + 1];
        rgb[i * 3 + 2] = pixels[i * 4 + (bgr ? 0 : 2)];
    }
    allocator_->destroy(captureBuffer_); // a full-screen buffer is too big to keep around
    if (writePng(path, extent.width, extent.height, rgb)) {
        std::cout << "Saved screenshot " << path << std::endl;
    } else {
        std::cerr << "Could not write screenshot " << path << std::endl;
    }
}

} // namespace gol3d

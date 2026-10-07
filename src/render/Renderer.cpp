// The Renderer: pipelines and buffers for the frame's draws (setup), then per
// frame acquiring a swapchain image, recording the draws, submitting and
// presenting (frames), plus copying a frame out as a PNG (screenshots).

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

// The cube vertex buffer's layout: binding 0, attributes at locations 0 and 1
// (inPosition and inNormal in life3d_boxes.vert).
constexpr VkVertexInputBindingDescription CUBE_VERTEX_BINDING{0, sizeof(CubeVertex),
                                                              VK_VERTEX_INPUT_RATE_VERTEX};
constexpr std::array<VkVertexInputAttributeDescription, 2> CUBE_VERTEX_ATTRIBUTES{{
    {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(CubeVertex, position)},
    {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(CubeVertex, normal)},
}};

// Frame descriptor set bindings; see life3d_frame.glsl and the vertex shaders.
enum FrameBinding : uint32_t {
    FrameUniformsBinding = 0,
    BlockInstancesBinding = 1,
    BoxesBinding = 2,
    ChunkOriginsBinding = 3,
    FrameBindingCount,
};
// Bindings 1-3 are storage buffers; binding 0 is the uniform buffer.
constexpr uint32_t STORAGE_BINDINGS_PER_SET = 3;

constexpr uint64_t ACQUIRE_TIMEOUT_NS = 4'000'000'000; // 4 seconds
constexpr VkShaderStageFlags DRAW_STAGES =
    VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

// A quad's corners 0-1-2-3 (counterclockwise) as two triangles, 0-1-2 and 0-2-3.
constexpr std::array<int, 6> QUAD_TRIANGLE_CORNERS = {0, 1, 2, 0, 2, 3};
constexpr int CORNERS_PER_QUAD = 4;
constexpr int BLOCK_FACES = 3; // life3d_blocks.vert draws only the camera-facing three
static_assert(BLOCK_FACES * QUAD_TRIANGLE_CORNERS.size() == BLOCK_INDEX_COUNT);

constexpr uint32_t FULLSCREEN_TRIANGLE_VERTICES = 3; // see life3d_screen.vert
// Each edge of a box is one instance of the cube, stretched; must match
// EDGES_PER_BOX in life3d_boxes.vert.
constexpr uint32_t EDGES_PER_BOX = 12;

// Swapchain images are 8-bit RGBA or BGRA; screenshots are 8-bit RGB.
constexpr size_t CAPTURE_BYTES_PER_PIXEL = 4;
constexpr size_t PNG_BYTES_PER_PIXEL = 3;

VkPipelineShaderStageCreateInfo shaderStage(VkShaderStageFlagBits stage, VkShaderModule module) {
    VkPipelineShaderStageCreateInfo info{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    info.stage = stage;
    info.module = module;
    info.pName = "main";
    return info;
}

// Vertex input for pipelines that read the cube vertex buffer; the others
// build their vertices from gl_VertexIndex and buffers alone.
VkPipelineVertexInputStateCreateInfo vertexInputState(bool cubeVertices) {
    VkPipelineVertexInputStateCreateInfo vertexInput{
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    if (cubeVertices) {
        vertexInput.vertexBindingDescriptionCount = 1;
        vertexInput.pVertexBindingDescriptions = &CUBE_VERTEX_BINDING;
        vertexInput.vertexAttributeDescriptionCount =
            static_cast<uint32_t>(CUBE_VERTEX_ATTRIBUTES.size());
        vertexInput.pVertexAttributeDescriptions = CUBE_VERTEX_ATTRIBUTES.data();
    }
    return vertexInput;
}

VkPipelineRasterizationStateCreateInfo rasterizationState() {
    VkPipelineRasterizationStateCreateInfo rasterizer{
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_NONE; // blocks only emit camera-facing faces
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    return rasterizer;
}

VkPipelineDepthStencilStateCreateInfo depthStencilState(bool depthTest, bool depthWrite) {
    VkPipelineDepthStencilStateCreateInfo depthStencil{
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    depthStencil.depthTestEnable = depthTest;
    depthStencil.depthWriteEnable = depthWrite;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    return depthStencil;
}

VkPipelineColorBlendAttachmentState colorBlendAttachment(bool alphaBlend) {
    VkPipelineColorBlendAttachmentState blendAttachment{};
    blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                     VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    if (alphaBlend) {
        // Standard "over" blending: color = src * srcAlpha + dst * (1 - srcAlpha).
        blendAttachment.blendEnable = VK_TRUE;
        blendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        blendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        blendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        blendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        blendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        blendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
    }
    return blendAttachment;
}

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
    createCubeVertices();
    createBlockIndices();
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

// A unit cube as 12 triangles with per-face normals, for box outlines.
void Renderer::createCubeVertices() {
    std::vector<CubeVertex> vertices;
    const glm::vec3 faceNormals[6] = {glm::vec3(1, 0, 0), glm::vec3(-1, 0, 0),
                                      glm::vec3(0, 1, 0), glm::vec3(0, -1, 0),
                                      glm::vec3(0, 0, 1), glm::vec3(0, 0, -1)};
    for (const glm::vec3& normal : faceNormals) {
        // Two axes in the face's plane; any perpendicular pair will do.
        glm::vec3 tangent = std::abs(normal.y) > 0.5f ? glm::vec3(1, 0, 0) : glm::vec3(0, 1, 0);
        glm::vec3 bitangent = glm::cross(normal, tangent);
        glm::vec3 center = normal * 0.5f;
        const glm::vec3 corners[CORNERS_PER_QUAD] = {
            center - 0.5f * tangent - 0.5f * bitangent, center + 0.5f * tangent - 0.5f * bitangent,
            center + 0.5f * tangent + 0.5f * bitangent, center - 0.5f * tangent + 0.5f * bitangent};
        for (int corner : QUAD_TRIANGLE_CORNERS) {
            vertices.push_back({corners[corner], normal});
        }
    }
    cubeVertexCount_ = static_cast<uint32_t>(vertices.size());
    VkDeviceSize vertexBytes = sizeof(CubeVertex) * vertices.size();
    allocator_->create(cubeVertices_, vertexBytes, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                       {memory::DEVICE_HOST, memory::HOST});
    std::memcpy(cubeVertices_.mapped, vertices.data(), vertexBytes);
}

// The index pattern of one block, shared by every block instance: each of the
// three camera-facing quads is two triangles. life3d_blocks.vert generates the
// corner positions from the vertex index (face * 4 + corner).
void Renderer::createBlockIndices() {
    std::array<uint16_t, BLOCK_INDEX_COUNT> indices{};
    const size_t indicesPerQuad = QUAD_TRIANGLE_CORNERS.size();
    for (uint16_t face = 0; face < BLOCK_FACES; ++face) {
        for (size_t i = 0; i < indicesPerQuad; ++i) {
            indices[face * indicesPerQuad + i] =
                static_cast<uint16_t>(face * CORNERS_PER_QUAD + QUAD_TRIANGLE_CORNERS[i]);
        }
    }
    allocator_->create(blockIndices_, sizeof(indices), VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                       {memory::DEVICE_HOST, memory::HOST});
    std::memcpy(blockIndices_.mapped, indices.data(), sizeof(indices));
}

// The per-frame-slot buffers the CPU writes every frame. They stay mapped
// (host-visible, preferably device-local) so writing them is a memcpy.
void Renderer::createFrameBuffers() {
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        allocator_->create(uniformBuffers_[i], sizeof(FrameUniforms),
                           VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, {memory::DEVICE_HOST, memory::HOST});
        allocator_->create(boxBuffers_[i], static_cast<VkDeviceSize>(MAX_BOXES) * sizeof(Box),
                           usage::STORAGE, {memory::DEVICE_HOST, memory::HOST});
    }
}

// The descriptor set layout (which buffers the shaders see at which binding)
// and the pipeline layout (that set plus the push constant), shared by all of
// the renderer's pipelines.
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

    // One push constant: the screen shaders' mode (`Draw` in life3d_frame.glsl).
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

// A graphics pipeline bakes the shaders and all fixed-function state into one
// object, so switching between draws is a single bind.
VkPipeline Renderer::createPipeline(const PipelineSpec& spec) {
    VkDevice device = gpu_->device;
    VkShaderModule vertexModule = loadShaderModule(device, shaderDir_ / spec.vertexShader);
    VkShaderModule fragmentModule = loadShaderModule(device, shaderDir_ / spec.fragmentShader);
    std::array<VkPipelineShaderStageCreateInfo, 2> stages{
        shaderStage(VK_SHADER_STAGE_VERTEX_BIT, vertexModule),
        shaderStage(VK_SHADER_STAGE_FRAGMENT_BIT, fragmentModule)};

    VkPipelineVertexInputStateCreateInfo vertexInput = vertexInputState(spec.cubeVertices);
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkPipelineViewportStateCreateInfo viewportState{
        VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;
    VkPipelineRasterizationStateCreateInfo rasterizer = rasterizationState();
    VkPipelineMultisampleStateCreateInfo multisampling{
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    VkPipelineDepthStencilStateCreateInfo depthStencil =
        depthStencilState(spec.depthTest, spec.depthWrite);
    VkPipelineColorBlendAttachmentState blendAttachment = colorBlendAttachment(spec.alphaBlend);
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

    // Dynamic rendering: instead of a render pass, the pipeline names the
    // attachment formats it draws into, chained in through pNext.
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
    // The pipeline keeps its own compiled copy of the shaders.
    vkDestroyShaderModule(device, vertexModule, nullptr);
    vkDestroyShaderModule(device, fragmentModule, nullptr);
    if (result != VK_SUCCESS) throw std::runtime_error("Could not create graphics pipeline.");
    return pipeline;
}

// One descriptor set per frame slot, since each slot has its own uniform and
// box buffers.
void Renderer::createDescriptorSets() {
    std::array<VkDescriptorPoolSize, 2> poolSizes{{
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, MAX_FRAMES_IN_FLIGHT},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, MAX_FRAMES_IN_FLIGHT * STORAGE_BINDINGS_PER_SET},
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
    // The slot before currentFrame_, wrapping around (adding the slot count
    // first keeps the unsigned subtraction from going below zero).
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
    // The slot's previous frame must be done before its buffers are reused.
    gpu_->waitForFence(inFlight_[currentFrame_], "a frame");
    // Acquiring can return an image index while the presentation engine is
    // still reading that image; imageAvailable_ is signaled once it is done.
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
    // Reset only once a frame will surely be submitted, or the next wait on
    // this fence would never return.
    vkResetFences(gpu_->device, 1, &inFlight_[currentFrame_]);
    return true;
}

void Renderer::endFrame(const FrameUniforms& uniforms, const std::vector<Box>& boxes,
                        bool drawImGui) {
    std::memcpy(uniformBuffers_[currentFrame_].mapped, &uniforms, sizeof(uniforms));
    const uint32_t boxCount = static_cast<uint32_t>(std::min<size_t>(boxes.size(), MAX_BOXES));
    std::memcpy(boxBuffers_[currentFrame_].mapped, boxes.data(), boxCount * sizeof(Box));

    // Take the pending request, so a screenshot is served exactly once.
    std::string screenshot;
    screenshot.swap(pendingScreenshot_);
    if (!screenshot.empty()) prepareCaptureBuffer();

    VkCommandBuffer cmd = commandBuffers_[currentFrame_];
    vkResetCommandBuffer(cmd, 0);
    recordCommands(cmd, boxCount, drawImGui, !screenshot.empty());
    submit(cmd);
    VkResult result = present();

    if (!screenshot.empty()) saveCapture(screenshot, currentFrame_);

    currentFrame_ = (currentFrame_ + 1) % MAX_FRAMES_IN_FLIGHT;
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || resized_) {
        swapchain_.recreate();
        resized_ = false;
    } else if (result != VK_SUCCESS) {
        throw std::runtime_error("Could not present swap chain image.");
    }
}

// Submits the frame's commands. They wait for the image to be available (only
// from the color output stage on, so vertex work can start earlier), signal
// the image's renderFinished semaphore for present(), and the slot's fence
// for the CPU.
void Renderer::submit(VkCommandBuffer cmd) {
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
}

// Queues the image for display once its renderFinished semaphore is signaled.
VkResult Renderer::present() {
    VkSemaphore renderFinished = swapchain_.renderFinished(imageIndex_);
    VkSwapchainKHR swapchain = swapchain_.handle();
    VkPresentInfoKHR presentInfo{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &renderFinished;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &swapchain;
    presentInfo.pImageIndices = &imageIndex_;
    return vkQueuePresentKHR(gpu_->queue, &presentInfo);
}

void Renderer::recordCommands(VkCommandBuffer cmd, uint32_t boxCount, bool drawImGui,
                              bool capture) {
    VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (vkBeginCommandBuffer(cmd, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("Could not begin recording command buffer.");
    }
    beginRendering(cmd);
    recordDraws(cmd, boxCount, drawImGui);
    vkCmdEndRendering(cmd);
    finishImage(cmd, capture);
    if (vkEndCommandBuffer(cmd) != VK_SUCCESS) {
        throw std::runtime_error("Could not record command buffer.");
    }
}

// Prepares the swapchain image and the depth buffer as attachments and starts
// rendering into them.
//
// Images have a layout (how their memory is arranged), and a barrier changes
// it. Both barriers come from UNDEFINED, which lets the driver discard the old
// contents: the sky covers every pixel, and depth is cleared.
//   - Color: the source stage is COLOR_ATTACHMENT_OUTPUT, the stage submit()
//     makes wait for imageAvailable_, so the layout change happens only after
//     the presentation engine has released the image.
//   - Depth: one depth buffer serves both frames in flight, so the previous
//     frame's depth tests and writes must finish before this frame clears it.
void Renderer::beginRendering(VkCommandBuffer cmd) {
    const VkImage image = swapchain_.image(imageIndex_);
    const VkExtent2D extent = swapchain_.extent();
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

    // Load and store ops say what happens to an attachment at the start and
    // end of rendering: color starts undefined (the sky covers it) and keeps
    // what is drawn for presenting; depth is cleared to the far plane (1) and
    // thrown away afterwards.
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
}

// The frame's draws, back to front in the order listed in Renderer.h.
void Renderer::recordDraws(VkCommandBuffer cmd, uint32_t boxCount, bool drawImGui) {
    const VkExtent2D extent = swapchain_.extent();
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

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, skyPipeline_);
    setScreenMode(ScreenMode::Sky);
    vkCmdDraw(cmd, FULLSCREEN_TRIANGLE_VERTICES, 1, 0, 0);

    // The block count lives on the GPU: the build pass wrote it into the
    // indirect command, so the CPU never needs to read it back.
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, blockPipeline_);
    vkCmdBindIndexBuffer(cmd, blockIndices_.buffer, 0, VK_INDEX_TYPE_UINT16);
    vkCmdDrawIndexedIndirect(cmd, indirectDraw_->buffer, 0, 1,
                             sizeof(VkDrawIndexedIndirectCommand));

    if (boxCount > 0) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, boxPipeline_);
        VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(cmd, 0, 1, &cubeVertices_.buffer, &offset);
        vkCmdDraw(cmd, cubeVertexCount_, boxCount * EDGES_PER_BOX, 0, 0);
    }

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, gridPipeline_);
    setScreenMode(ScreenMode::Grid);
    vkCmdDraw(cmd, FULLSCREEN_TRIANGLE_VERTICES, 1, 0, 0);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, hudPipeline_);
    setScreenMode(ScreenMode::Hud);
    vkCmdDraw(cmd, FULLSCREEN_TRIANGLE_VERTICES, 1, 0, 0);
    if (drawImGui) {
        ImDrawData* drawData = ImGui::GetDrawData();
        if (drawData && drawData->CmdLists.Size > 0) ImGui_ImplVulkan_RenderDrawData(drawData, cmd);
    }
}

// Moves the finished image to the layout the presentation engine reads,
// copying it into the capture buffer first when a screenshot was requested.
// The barriers into PRESENT_SRC need no destination stage: present() waits on
// renderFinished, which is signaled after all commands and makes their writes
// visible.
void Renderer::finishImage(VkCommandBuffer cmd, bool capture) {
    const VkImage image = swapchain_.image(imageIndex_);
    if (capture) {
        const VkExtent2D extent = swapchain_.extent();
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
        // The CPU reads the buffer after the fence (saveCapture).
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
}

// ------------------------------------------------------------- screenshots

void Renderer::prepareCaptureBuffer() {
    const VkExtent2D extent = swapchain_.extent();
    allocator_->destroy(captureBuffer_);
    allocator_->create(captureBuffer_,
                       static_cast<VkDeviceSize>(extent.width) * extent.height *
                           CAPTURE_BYTES_PER_PIXEL,
                       VK_BUFFER_USAGE_TRANSFER_DST_BIT, {memory::HOST_CACHED, memory::HOST});
}

// Waits for the frame in slot `frame`, then writes its captured image as a PNG.
void Renderer::saveCapture(const std::string& path, size_t frame) {
    gpu_->waitForFence(inFlight_[frame], "the screenshot frame");
    const VkExtent2D extent = swapchain_.extent();
    const size_t pixelCount = static_cast<size_t>(extent.width) * extent.height;
    const uint8_t* pixels = captureBuffer_.as<const uint8_t>();
    const VkFormat format = swapchain_.colorFormat();
    // BGRA images store blue first: swap red and blue while dropping alpha.
    const bool bgr = format == VK_FORMAT_B8G8R8A8_SRGB || format == VK_FORMAT_B8G8R8A8_UNORM;
    const size_t redOffset = bgr ? 2 : 0;
    const size_t blueOffset = bgr ? 0 : 2;
    std::vector<uint8_t> rgb(pixelCount * PNG_BYTES_PER_PIXEL);
    for (size_t i = 0; i < pixelCount; ++i) {
        const uint8_t* pixel = pixels + i * CAPTURE_BYTES_PER_PIXEL;
        rgb[i * PNG_BYTES_PER_PIXEL + 0] = pixel[redOffset];
        rgb[i * PNG_BYTES_PER_PIXEL + 1] = pixel[1];
        rgb[i * PNG_BYTES_PER_PIXEL + 2] = pixel[blueOffset];
    }
    allocator_->destroy(captureBuffer_); // a full-screen buffer is too big to keep around
    if (writePng(path, extent.width, extent.height, rgb)) {
        std::cout << "Saved screenshot " << path << std::endl;
    } else {
        std::cerr << "Could not write screenshot " << path << std::endl;
    }
}

} // namespace gol3d

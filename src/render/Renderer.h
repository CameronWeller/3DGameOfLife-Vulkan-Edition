#pragma once

// Draws a frame. Everything on screen comes from four kinds of draw, in order:
//
//   1. sky   - a fullscreen triangle (shaders/life3d_screen.frag, mode 0)
//   2. blocks - the block list built by the simulation's build pass, one
//               indirect instanced draw (life3d_blocks.vert)
//   3. boxes - outlines: the targeted block, the placement preview, chunk
//               borders, tutorial marks (life3d_boxes.vert)
//   4. grid and HUD - the y = 0 grid and the crosshair and hotbar, fullscreen
//               (life3d_screen.frag, modes 1 and 2), then Dear ImGui's menus
//
// The renderer knows nothing about the game: each frame the game fills a
// FrameUniforms block and a list of boxes between beginFrame() and endFrame().
//
// Two frames may be in flight: the CPU records frame N + 1 while the GPU still
// draws frame N. Everything the CPU writes per frame (uniforms, boxes, the
// command buffer) therefore exists twice, one per frame slot, and a fence per
// slot tells the CPU when the GPU is done with it.
//
// Drawing uses Vulkan 1.3 dynamic rendering: vkCmdBeginRendering names the
// attachments directly, so there are no VkRenderPass or VkFramebuffer
// objects; pipelines only declare the attachment formats (renderingInfo()).

#include <array>
#include <filesystem>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <volk.h>

#include "gpu/GpuBuffer.h"
#include "gpu/Swapchain.h"

struct GLFWwindow;

namespace gol3d {

class GpuContext;

// The per-frame uniform block; must match `Frame` in shaders/life3d_frame.glsl
// (std140 layout: only mat4 and 16-byte vectors, so the C++ layout matches
// without padding).
struct FrameUniforms {
    glm::mat4 viewProjection;
    glm::mat4 inverseViewProjection;
    glm::vec4 camera;   // xyz eye position, w seconds since start
    glm::vec4 viewport; // xy framebuffer size, z 1 = HUD visible, w blocks to draw
    glm::ivec4 hotbar;  // x selected slot (-1 = empty hand), y slot count, z material
    glm::vec4 fog;      // x fog start, y fog end (blocks)
    glm::vec4 anim;     // x birth/death animation progress (1 = done), y 1 = ambient occlusion
    glm::vec4 sun;      // xyz direction toward the sun
};

// An outline box, as life3d_boxes.vert reads it (`Boxes`, two vec4 per box).
struct Box {
    glm::vec4 min; // w = edge thickness
    glm::vec4 max; // w = color id (BoxColor)
};

// Outline colors; the table is BOX_COLORS in life3d_boxes.vert.
enum class BoxColor {
    Target = 0, // the block under the crosshair
    ChunkBorder = 1,
    PlaceLife = 2,    // placement preview, by material
    TutorialMark = 3, // 3-6: tutorial marks (see tutorial::Mark)
    PlaceStone = 7,
    PlaceEmber = 8,
};

class Renderer {
public:
    static constexpr int MAX_FRAMES_IN_FLIGHT = 2;
    static constexpr uint32_t MAX_BOXES = 4096;

    void initSwapchain(const GpuContext& gpu, BufferAllocator& allocator, GLFWwindow* window);
    // `instances` and `indirectDraw` are the block list; `origins` the chunk origins.
    void initPipelines(const std::filesystem::path& shaderDir, const GpuBuffer& instances,
                       const GpuBuffer& indirectDraw, const GpuBuffer& origins);
    void destroy();

    // Points the frame descriptor sets at new buffers (after the world grew).
    void writeDescriptors(const GpuBuffer& instances, const GpuBuffer& origins);

    // Waits for this frame slot to be free and acquires a swapchain image.
    // False when there is nothing to draw into this time (the swapchain is being
    // rebuilt or the compositor is not ready); skip the frame then.
    bool beginFrame();
    // Uploads the frame's data, records and submits the draws, and presents.
    void endFrame(const FrameUniforms& uniforms, const std::vector<Box>& boxes, bool drawImGui);

    // Waits until the GPU has finished the previous frame, so the governor does
    // not count its drawing as simulation time.
    void waitForPreviousFrame() const;
    // The window was resized; rebuild the swapchain at the next opportunity.
    void onResize() { resized_ = true; }
    // Saves the next frame as a PNG.
    void requestScreenshot(const std::string& path);

    const Swapchain& swapchain() const { return swapchain_; }
    // Attachment formats for pipelines that draw into the frame (ImGui's too).
    VkPipelineRenderingCreateInfo renderingInfo() const;

private:
    // How the shared screen shaders draw (the `mode` push constant); must
    // match MODE_* in shaders/life3d_screen.frag.
    enum class ScreenMode : uint32_t { Sky = 0, Grid = 1, Hud = 2 };

    // What differs between the renderer's pipelines; everything else is shared.
    struct PipelineSpec {
        const char* vertexShader = nullptr;
        const char* fragmentShader = nullptr;
        bool cubeVertices = false; // reads the cube vertex buffer
        bool depthTest = false;
        bool depthWrite = false;
        bool alphaBlend = false;
    };

    // Setup, in the order initPipelines() calls them.
    void createCubeVertices();
    void createBlockIndices();
    void createFrameBuffers();
    void createLayouts();
    VkPipeline createPipeline(const PipelineSpec& spec);
    void createDescriptorSets();
    void createCommandBuffers();
    void createSyncObjects();

    // One frame: endFrame() records (recordCommands, in three steps), submits
    // and presents.
    void recordCommands(VkCommandBuffer cmd, uint32_t boxCount, bool drawImGui, bool capture);
    void beginRendering(VkCommandBuffer cmd);
    void recordDraws(VkCommandBuffer cmd, uint32_t boxCount, bool drawImGui);
    void finishImage(VkCommandBuffer cmd, bool capture);
    void submit(VkCommandBuffer cmd);
    VkResult present();

    void prepareCaptureBuffer();
    void saveCapture(const std::string& path, size_t frame);

    const GpuContext* gpu_ = nullptr;
    BufferAllocator* allocator_ = nullptr;
    std::filesystem::path shaderDir_;
    Swapchain swapchain_;
    bool resized_ = false;

    // Pipelines; all share one layout and one descriptor set per frame.
    VkDescriptorSetLayout setLayout_ = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE;
    VkPipeline blockPipeline_ = VK_NULL_HANDLE;
    VkPipeline boxPipeline_ = VK_NULL_HANDLE;
    VkPipeline skyPipeline_ = VK_NULL_HANDLE;
    VkPipeline gridPipeline_ = VK_NULL_HANDLE;
    VkPipeline hudPipeline_ = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool_ = VK_NULL_HANDLE;
    std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> descriptorSets_{};

    // Geometry and per-frame data.
    GpuBuffer cubeVertices_; // a unit cube, for box outlines
    uint32_t cubeVertexCount_ = 0;
    GpuBuffer blockIndices_; // the index pattern of one block's three faces
    const GpuBuffer* indirectDraw_ = nullptr;
    std::array<GpuBuffer, MAX_FRAMES_IN_FLIGHT> uniformBuffers_;
    std::array<GpuBuffer, MAX_FRAMES_IN_FLIGHT> boxBuffers_;

    // Frame pacing, per frame slot. imageAvailable_ is signaled when the
    // acquired swapchain image may be drawn into; inFlight_ when the GPU has
    // finished the slot's commands. (The semaphore the present waits on is
    // per swapchain image; see Swapchain::renderFinished.)
    std::array<VkCommandBuffer, MAX_FRAMES_IN_FLIGHT> commandBuffers_{};
    std::array<VkSemaphore, MAX_FRAMES_IN_FLIGHT> imageAvailable_{};
    std::array<VkFence, MAX_FRAMES_IN_FLIGHT> inFlight_{};
    size_t currentFrame_ = 0; // the frame slot in use, 0..MAX_FRAMES_IN_FLIGHT - 1
    uint32_t imageIndex_ = 0; // the image acquired by beginFrame()

    // Screenshots: the path requested for the next frame, and a host-visible
    // buffer the frame's image is copied into (only while a capture runs).
    std::string pendingScreenshot_;
    GpuBuffer captureBuffer_;
};

} // namespace gol3d

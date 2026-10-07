#pragma once

// The Vulkan instance, GPU and queue the game runs on. gol3d needs Vulkan 1.3
// (dynamic rendering, synchronization2) and compute subgroup operations
// (basic, arithmetic, ballot), which every desktop driver from the last several
// years provides, as does MoltenVK on macOS. One queue family does graphics,
// compute and presentation.
//
// Validation layers are off unless GOL3D_VALIDATION=1 is set. GOL3D_GPU=<text>
// picks the first GPU whose name contains <text> (for example "llvmpipe").

#include <cstdint>
#include <vector>

#include <volk.h>

struct GLFWwindow;

namespace gol3d {

// What the window surface accepts, as the swapchain needs to know it: image
// count and size limits, pixel formats and presentation modes.
struct SurfaceSupport {
    VkSurfaceCapabilitiesKHR capabilities{};
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

// The Vulkan objects everything else is built on. In Vulkan terms:
//   - the instance is the game's connection to the Vulkan library;
//   - the surface is the window as Vulkan sees it, something to present to;
//   - the physical device is one GPU, used to ask what it supports;
//   - the device is the game's open session on that GPU; every buffer, image
//     and pipeline belongs to it;
//   - a queue family is a group of hardware queues with the same abilities
//     (graphics, compute, transfer, presenting). The game submits all of its
//     work to one queue of one family, so no resource ever has to be handed
//     from one queue to another;
//   - the command pool hands out the command buffers that work is recorded into.
// The fields are public because nearly every GPU call needs one of them.
class GpuContext {
public:
    // Opens Vulkan for the window (its extensions come from GLFW). Throws with a
    // message for the player when no suitable GPU exists.
    void init(GLFWwindow* window);
    // Destroys what init() created. Everything made from the device (buffers,
    // pipelines, the swapchain) must be destroyed before this.
    void destroy();

    // What the window surface supports right now; it changes with the window size.
    SurfaceSupport querySurface() const;

    // Waits for the GPU to signal `fence`. A fence is a flag the GPU raises when
    // a submission has finished, which the CPU can wait on. Throws when it takes
    // over 4 seconds (the GPU is treated as lost) or the device is lost; `what`
    // names the work in the message.
    void waitForFence(VkFence fence, const char* what) const;

    VkInstance instance = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    uint32_t queueFamily = 0;
    VkQueue queue = VK_NULL_HANDLE;
    // Its command buffers can be reset one by one, so each user allocates its
    // own once and re-records it every time.
    VkCommandPool commandPool = VK_NULL_HANDLE;
    VkPhysicalDeviceProperties properties{};             // GPU name, type and limits
    VkPhysicalDeviceMemoryProperties memoryProperties{}; // its memory types and heaps

private:
    void createInstance();
    void choosePhysicalDevice();
    void createDevice();

    // Prints validation-layer messages; exists only with GOL3D_VALIDATION=1.
    VkDebugUtilsMessengerEXT messenger_ = VK_NULL_HANDLE;
};

} // namespace gol3d

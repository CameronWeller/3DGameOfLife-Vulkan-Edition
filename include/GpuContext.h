#pragma once

// The Vulkan instance, GPU and queue the game runs on. gol3d needs Vulkan 1.3
// (dynamic rendering, synchronization2) and compute subgroup operations
// (basic, arithmetic, ballot), which every desktop driver from the last several
// years provides, as does MoltenVK on macOS. One queue family does graphics,
// compute and presentation.
//
// Validation layers are off unless GOL3D_VALIDATION=1 is set. GOL3D_GPU=<text>
// picks the first GPU whose name contains <text> (for example "llvmpipe").

#include <string>
#include <vector>

struct GLFWwindow;

namespace gol3d {

struct SurfaceSupport {
    VkSurfaceCapabilitiesKHR capabilities{};
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

class GpuContext {
public:
    // Opens Vulkan for the window (its extensions come from GLFW). Throws with a
    // message for the player when no suitable GPU exists.
    void init(GLFWwindow* window);
    void destroy();

    SurfaceSupport querySurface() const;

    VkInstance instance = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    uint32_t queueFamily = 0;
    VkQueue queue = VK_NULL_HANDLE;
    VkCommandPool commandPool = VK_NULL_HANDLE;
    VkPhysicalDeviceProperties properties{};
    VkPhysicalDeviceMemoryProperties memoryProperties{};
    uint32_t subgroupSize = 0;

private:
    VkDebugUtilsMessengerEXT messenger = VK_NULL_HANDLE;
};

} // namespace gol3d

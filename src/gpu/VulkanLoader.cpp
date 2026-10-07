// Loads the Vulkan library through volk, which fills in the global vk*
// function pointers that the rest of the code calls.

#include "gpu/VulkanLoader.h"

#include <stdexcept>

#include <volk.h>

#include <GLFW/glfw3.h>

#if defined(__APPLE__)
#include <dlfcn.h>
#endif

namespace gol3d {
namespace {

#if defined(__APPLE__)
// Loads the MoltenVK shipped inside the app bundle (or next to a build-tree
// executable). Returns false when there is none. The library stays loaded for
// the life of the process, so its handle is never closed.
bool loadBundledMoltenVk(const std::filesystem::path& exeDir) {
    const std::filesystem::path candidates[] = {
        exeDir / ".." / "Frameworks" / "libMoltenVK.dylib",
        exeDir / "libMoltenVK.dylib",
    };
    for (const std::filesystem::path& candidate : candidates) {
        void* library = dlopen(candidate.c_str(), RTLD_NOW | RTLD_LOCAL);
        if (!library) continue;
        auto getInstanceProcAddr =
            reinterpret_cast<PFN_vkGetInstanceProcAddr>(dlsym(library, "vkGetInstanceProcAddr"));
        if (getInstanceProcAddr) {
            volkInitializeCustom(getInstanceProcAddr);
            return true;
        }
    }
    return false;
}
#endif

} // namespace

void initVulkanLoader([[maybe_unused]] const std::filesystem::path& exeDir) {
    bool loaded = false;
#if defined(__APPLE__)
    loaded = loadBundledMoltenVk(exeDir);
#endif
    if (!loaded && volkInitialize() != VK_SUCCESS) {
        throw std::runtime_error(
            "Vulkan is not available. Install a GPU driver with Vulkan support "
            "(on Linux also the Vulkan loader, e.g. libvulkan1 or vulkan-icd-loader).");
    }
    // GLFW 3.4 and later can be told which loader to use, so that it creates
    // window surfaces through the same one (older versions find their own).
#if GLFW_VERSION_MAJOR > 3 || (GLFW_VERSION_MAJOR == 3 && GLFW_VERSION_MINOR >= 4)
    glfwInitVulkanLoader(vkGetInstanceProcAddr);
#endif
}

} // namespace gol3d

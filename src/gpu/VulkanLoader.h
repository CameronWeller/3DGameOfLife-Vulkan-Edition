#pragma once

#include <filesystem>

namespace gol3d {

// Opens the system's Vulkan library at runtime (through volk) instead of linking
// it, so one binary starts on any machine with a Vulkan driver and can say what
// is missing when there is none. On macOS the app bundle's MoltenVK is tried
// first. Throws with an install hint when Vulkan is unavailable.
void initVulkanLoader(const std::filesystem::path& exeDir);

} // namespace gol3d

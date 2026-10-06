#pragma once

#include <filesystem>

#include <volk.h>

namespace gol3d {

// Loads a compiled SPIR-V shader (*.spv). The caller destroys the module once
// its pipelines exist. Throws when the file is missing or invalid.
VkShaderModule loadShaderModule(VkDevice device, const std::filesystem::path& path);

} // namespace gol3d

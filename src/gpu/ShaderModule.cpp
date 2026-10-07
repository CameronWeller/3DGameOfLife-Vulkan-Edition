// Reads a .spv file and hands it to the driver as a shader module.

#include "gpu/ShaderModule.h"

#include <fstream>
#include <stdexcept>
#include <vector>

namespace gol3d {

VkShaderModule loadShaderModule(VkDevice device, const std::filesystem::path& path) {
    // Opened at the end (ios::ate), so tellg() gives the file size.
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) throw std::runtime_error("Missing shader " + path.string());
    std::vector<char> code(static_cast<size_t>(file.tellg()));
    file.seekg(0);
    file.read(code.data(), static_cast<std::streamsize>(code.size()));

    VkShaderModuleCreateInfo createInfo{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    createInfo.codeSize = code.size();
    // SPIR-V is a stream of 32-bit words. A vector's heap storage is aligned
    // for any fundamental type, so viewing its bytes as words is safe.
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());
    VkShaderModule module = VK_NULL_HANDLE;
    if (vkCreateShaderModule(device, &createInfo, nullptr, &module) != VK_SUCCESS) {
        throw std::runtime_error("Could not load shader " + path.string());
    }
    return module;
}

} // namespace gol3d

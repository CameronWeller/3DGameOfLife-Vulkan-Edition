#include "vulkan/resources/ShaderManager.h"
#include "VulkanContext.h"
#include "VulkanError.h"
#include <fstream>
#include <stdexcept>

namespace VulkanHIP {

ShaderManager::ShaderManager(VulkanContext* context)
    : context_(context) {
}

ShaderManager::~ShaderManager() {
    cleanup();
}

VkShaderModule ShaderManager::createShaderModule(const std::string& filename) {
    // Check if shader module already exists
    auto it = shaderModules_.find(filename);
    if (it != shaderModules_.end()) {
        return it->second;
    }
    
    // Read shader file
    auto code = readFile(filename);
    
    // Create shader module
    auto shaderModule = createShaderModuleFromCode(code);
    
    // Cache shader module
    shaderModules_[filename] = shaderModule;
    
    return shaderModule;
}

void ShaderManager::createShaderStages(const std::string& vertPath, const std::string& fragPath,
                                     VkPipelineShaderStageCreateInfo& vertStageInfo,
                                     VkPipelineShaderStageCreateInfo& fragStageInfo) {
    auto vertShaderModule = createShaderModule(vertPath);
    auto fragShaderModule = createShaderModule(fragPath);
    
    vertStageInfo = {};
    vertStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertStageInfo.module = vertShaderModule;
    vertStageInfo.pName = "main";
    
    fragStageInfo = {};
    fragStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragStageInfo.module = fragShaderModule;
    fragStageInfo.pName = "main";
}

VkPipelineShaderStageCreateInfo ShaderManager::createComputeStage(const std::string& compPath) {
    auto compShaderModule = createShaderModule(compPath);
    
    VkPipelineShaderStageCreateInfo stageInfo{};
    stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stageInfo.module = compShaderModule;
    stageInfo.pName = "main";
    
    return stageInfo;
}

void ShaderManager::cleanup() {
    VkDevice device = context_->getDevice();
    
    for (const auto& [filename, shaderModule] : shaderModules_) {
        vkDestroyShaderModule(device, shaderModule, nullptr);
    }
    
    shaderModules_.clear();
}

std::vector<char> ShaderManager::readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open shader file: " + filename);
    }
    
    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);
    
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();
    
    return buffer;
}

VkShaderModule ShaderManager::createShaderModuleFromCode(const std::vector<char>& code) {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());
    
    VkShaderModule shaderModule;
    VK_CHECK(vkCreateShaderModule(context_->getDevice(), &createInfo, nullptr, &shaderModule));
    
    return shaderModule;
}

} // namespace VulkanHIP 
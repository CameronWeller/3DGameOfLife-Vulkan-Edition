#pragma once

#include <vulkan/vulkan.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

namespace VulkanHIP {

class VulkanContext;

/**
 * @brief Manages shader module creation and caching
 */
class ShaderManager {
public:
    explicit ShaderManager(VulkanContext* context);
    ~ShaderManager();
    
    // Delete copy constructor and assignment operator
    ShaderManager(const ShaderManager&) = delete;
    ShaderManager& operator=(const ShaderManager&) = delete;
    
    /**
     * @brief Create a shader module from SPIR-V file
     * @param filename Path to the SPIR-V shader file
     * @return VkShaderModule handle
     */
    VkShaderModule createShaderModule(const std::string& filename);
    
    /**
     * @brief Create shader stages for graphics pipeline
     * @param vertPath Path to vertex shader
     * @param fragPath Path to fragment shader
     * @param vertStageInfo Output vertex shader stage info
     * @param fragStageInfo Output fragment shader stage info
     */
    void createShaderStages(const std::string& vertPath, const std::string& fragPath,
                           VkPipelineShaderStageCreateInfo& vertStageInfo,
                           VkPipelineShaderStageCreateInfo& fragStageInfo);
    
    /**
     * @brief Create compute shader stage
     * @param compPath Path to compute shader
     * @return Shader stage create info
     */
    VkPipelineShaderStageCreateInfo createComputeStage(const std::string& compPath);
    
    /**
     * @brief Clean up shader resources
     */
    void cleanup();
    
private:
    VulkanContext* context_;
    std::unordered_map<std::string, VkShaderModule> shaderModules_;
    
    std::vector<char> readFile(const std::string& filename);
    VkShaderModule createShaderModuleFromCode(const std::vector<char>& code);
};

} // namespace VulkanHIP 
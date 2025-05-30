#include "VulkanPerformanceTestBase.hpp"
#include <benchmark/benchmark.h>
#include <fstream>
#include <vector>

class ShaderPerformanceTest : public VulkanPerformanceTestBase {
protected:
    void SetUp() override {
        VulkanPerformanceTestBase::SetUp();
        // Get queue for shader operations
        vkGetDeviceQueue(device, 0, 0, &queue);
    }

    void TearDown() override {
        VulkanPerformanceTestBase::TearDown();
    }

    // Helper function to read shader file
    std::vector<char> readShaderFile(const std::string& filename) {
        std::ifstream file(filename, std::ios::ate | std::ios::binary);
        if (!file.is_open()) {
            throw std::runtime_error("Failed to open shader file");
        }

        size_t fileSize = (size_t)file.tellg();
        std::vector<char> buffer(fileSize);
        file.seekg(0);
        file.read(buffer.data(), fileSize);
        file.close();

        return buffer;
    }

    // Helper function to create shader module
    VkShaderModule createShaderModule(const std::vector<char>& code) {
        VkShaderModuleCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = code.size();
        createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

        VkShaderModule shaderModule;
        if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create shader module");
        }

        return shaderModule;
    }
};

// Test shader module creation performance
TEST_F(ShaderPerformanceTest, ShaderModuleCreation) {
    // Create a simple vertex shader
    const char* vertexShaderSource = R"(
        #version 450
        layout(location = 0) in vec3 inPosition;
        layout(location = 1) in vec3 inColor;
        layout(location = 0) out vec3 fragColor;
        void main() {
            gl_Position = vec4(inPosition, 1.0);
            fragColor = inColor;
        }
    )";

    std::vector<char> shaderCode(vertexShaderSource, vertexShaderSource + strlen(vertexShaderSource));
    
    measurePipelineCreation("Shader Module Creation", [&]() {
        VkShaderModule shaderModule = createShaderModule(shaderCode);
        vkDestroyShaderModule(device, shaderModule, nullptr);
    });
}

// Test shader compilation performance
TEST_F(ShaderPerformanceTest, ShaderCompilation) {
    // Create a simple fragment shader
    const char* fragmentShaderSource = R"(
        #version 450
        layout(location = 0) in vec3 fragColor;
        layout(location = 0) out vec4 outColor;
        void main() {
            outColor = vec4(fragColor, 1.0);
        }
    )";

    std::vector<char> shaderCode(fragmentShaderSource, fragmentShaderSource + strlen(fragmentShaderSource));
    
    measurePipelineCreation("Shader Compilation", [&]() {
        VkShaderModule shaderModule = createShaderModule(shaderCode);
        vkDestroyShaderModule(device, shaderModule, nullptr);
    });
}

// Benchmark shader module creation with different shader sizes
static void BM_ShaderModuleCreation(benchmark::State& state) {
    ShaderPerformanceTest test;
    test.SetUp();

    // Create a shader with varying complexity
    std::string shaderSource = "#version 450\n";
    for (int i = 0; i < state.range(0); i++) {
        shaderSource += "layout(location = " + std::to_string(i) + ") in vec3 inPos" + std::to_string(i) + ";\n";
    }
    shaderSource += "void main() { gl_Position = vec4(0.0); }";

    std::vector<char> shaderCode(shaderSource.begin(), shaderSource.end());

    for (auto _ : state) {
        VkShaderModule shaderModule = test.createShaderModule(shaderCode);
        vkDestroyShaderModule(test.device, shaderModule, nullptr);
    }

    test.TearDown();
}
BENCHMARK(BM_ShaderModuleCreation)->Range(1, 100); // Test with 1 to 100 input variables

// Benchmark shader pipeline creation
static void BM_ShaderPipelineCreation(benchmark::State& state) {
    ShaderPerformanceTest test;
    test.SetUp();

    // Create vertex shader
    const char* vertexShaderSource = R"(
        #version 450
        layout(location = 0) in vec3 inPosition;
        layout(location = 1) in vec3 inColor;
        layout(location = 0) out vec3 fragColor;
        void main() {
            gl_Position = vec4(inPosition, 1.0);
            fragColor = inColor;
        }
    )";

    // Create fragment shader
    const char* fragmentShaderSource = R"(
        #version 450
        layout(location = 0) in vec3 fragColor;
        layout(location = 0) out vec4 outColor;
        void main() {
            outColor = vec4(fragColor, 1.0);
        }
    )";

    std::vector<char> vertexShaderCode(vertexShaderSource, vertexShaderSource + strlen(vertexShaderSource));
    std::vector<char> fragmentShaderCode(fragmentShaderSource, fragmentShaderSource + strlen(fragmentShaderSource));

    for (auto _ : state) {
        VkShaderModule vertexShader = test.createShaderModule(vertexShaderCode);
        VkShaderModule fragmentShader = test.createShaderModule(fragmentShaderCode);

        // Create pipeline layout
        VkPipelineLayout pipelineLayout;
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        vkCreatePipelineLayout(test.device, &pipelineLayoutInfo, nullptr, &pipelineLayout);

        // Create pipeline
        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        // TODO: Add pipeline configuration

        VkPipeline pipeline;
        vkCreateGraphicsPipelines(test.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline);

        // Cleanup
        vkDestroyPipeline(test.device, pipeline, nullptr);
        vkDestroyPipelineLayout(test.device, pipelineLayout, nullptr);
        vkDestroyShaderModule(test.device, vertexShader, nullptr);
        vkDestroyShaderModule(test.device, fragmentShader, nullptr);
    }

    test.TearDown();
}
BENCHMARK(BM_ShaderPipelineCreation);

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    benchmark::Initialize(&argc, argv);
    benchmark::RunSpecifiedBenchmarks();
    return RUN_ALL_TESTS();
} 
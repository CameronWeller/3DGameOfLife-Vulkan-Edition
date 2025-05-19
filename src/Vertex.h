#pragma once

#include <vulkan/vulkan.h>
#include <array>
#include <glm/glm.hpp>

/**
 * @brief Vertex structure with position, color, and texture coordinates
 */
struct Vertex {
    /** @brief Position of the vertex in 3D space */
    glm::vec3 pos;
    
    /** @brief Color of the vertex (RGB) */
    glm::vec3 color;
    
    /** @brief Texture coordinates (UV) */
    glm::vec2 texCoord;

    /**
     * @brief Get the binding description for vertex input
     * @return VkVertexInputBindingDescription
     */
    static VkVertexInputBindingDescription getBindingDescription() {
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(Vertex);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        
        return bindingDescription;
    }
    
    /**
     * @brief Get the attribute descriptions for vertex input
     * @return std::array<VkVertexInputAttributeDescription, 3>
     */
    static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions() {
        std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions{};
        
        // Position
        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[0].offset = offsetof(Vertex, pos);
        
        // Color
        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[1].offset = offsetof(Vertex, color);
        
        // Texture coordinate
        attributeDescriptions[2].binding = 0;
        attributeDescriptions[2].location = 2;
        attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[2].offset = offsetof(Vertex, texCoord);
        
        return attributeDescriptions;
    }
    
    /**
     * @brief Equality operator for vertices
     */
    bool operator==(const Vertex& other) const {
        return pos == other.pos && color == other.color && texCoord == other.texCoord;
    }
}; 
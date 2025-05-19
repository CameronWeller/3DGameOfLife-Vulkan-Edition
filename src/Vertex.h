#pragma once

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <array>

/**
 * @brief Represents a vertex with position, color, and texture coordinates
 * 
 * This struct defines the vertex format used in the rendering pipeline.
 * It provides static methods to generate Vulkan vertex input descriptions.
 */
struct Vertex {
    /** @brief Position of the vertex in 3D space */
    glm::vec3 pos;
    
    /** @brief Color of the vertex (RGB) */
    glm::vec3 color;
    
    /** @brief Texture coordinates (UV) */
    glm::vec2 texCoord;

    /**
     * @brief Get the binding description for this vertex format
     * 
     * This describes how to interpret vertex buffer data in the shader
     * 
     * @return The vertex binding description
     */
    static VkVertexInputBindingDescription getBindingDescription();

    /**
     * @brief Get the attribute descriptions for this vertex format
     * 
     * This describes the individual attributes within the vertex structure
     * 
     * @return Array of attribute descriptions (position, color, UV)
     */
    static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions();
    
    /**
     * @brief Equality operator for vertices
     */
    bool operator==(const Vertex& other) const {
        return pos == other.pos && color == other.color && texCoord == other.texCoord;
    }
}; 
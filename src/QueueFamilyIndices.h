#pragma once

#include <vulkan/vulkan.h>
#include <optional>
#include <vector>
#include <set>

/**
 * @brief Indices for queue families required by the application
 */
struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;
    std::optional<uint32_t> computeFamily;

    /**
     * @brief Check if all required graphics and present queues were found
     * @return True if graphics and present queues are available
     */
    bool isCompleteGraphicsPresent() const {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }

    /**
     * @brief Check if all required queues (graphics, present, compute) were found
     * @return True if all required queues are available
     */
    bool isFullyComplete() const {
        return graphicsFamily.has_value() && presentFamily.has_value() && computeFamily.has_value();
    }

    /**
     * @brief Check if a compute queue family was found
     * @return True if compute queue family is available
     */
    bool hasCompute() const {
        return computeFamily.has_value();
    }

    /**
     * @brief Get a list of unique queue family indices.
     * Useful for creating device queues without duplicates.
     * @return A vector of unique queue family indices.
     */
    std::set<uint32_t> getUniqueFamilies() const {
        std::set<uint32_t> families;
        if (graphicsFamily.has_value()) {
            families.insert(graphicsFamily.value());
        }
        if (presentFamily.has_value()) {
            families.insert(presentFamily.value());
        }
        if (computeFamily.has_value()) {
            families.insert(computeFamily.value());
        }
        return families;
    }
}; 
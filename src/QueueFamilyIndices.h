#pragma once

#include <optional>
#include <vulkan/vulkan.h>

struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;
    std::optional<uint32_t> computeFamily;  // For HIP compute operations

    bool isComplete() const {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }

    bool hasCompute() const {
        return computeFamily.has_value();
    }

    // Get unique queue families
    std::vector<uint32_t> getUniqueFamilies() const {
        std::vector<uint32_t> families;
        if (graphicsFamily.has_value()) {
            families.push_back(graphicsFamily.value());
        }
        if (presentFamily.has_value() && 
            (!graphicsFamily.has_value() || presentFamily.value() != graphicsFamily.value())) {
            families.push_back(presentFamily.value());
        }
        if (computeFamily.has_value() && 
            (!graphicsFamily.has_value() || computeFamily.value() != graphicsFamily.value()) &&
            (!presentFamily.has_value() || computeFamily.value() != presentFamily.value())) {
            families.push_back(computeFamily.value());
        }
        return families;
    }
}; 
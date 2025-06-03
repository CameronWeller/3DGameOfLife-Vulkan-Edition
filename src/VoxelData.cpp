#include "VoxelData.h"
#include <fstream>
#include <algorithm>
#include <cmath>

void VoxelData::addVoxel(const Voxel& voxel) {
    // Check if voxel already exists at this position
    auto it = std::find_if(voxels_.begin(), voxels_.end(),
        [&voxel](const Voxel& v) {
            return v.position == voxel.position;
        });
    
    if (it != voxels_.end()) {
        *it = voxel; // Update existing voxel
    } else {
        voxels_.push_back(voxel); // Add new voxel
    }
}

void VoxelData::removeVoxel(const glm::vec3& position) {
    voxels_.erase(
        std::remove_if(voxels_.begin(), voxels_.end(),
            [&position](const Voxel& v) {
                return v.position == position;
            }),
        voxels_.end()
    );
}

void VoxelData::clear() {
    voxels_.clear();
}

// Grid-based access methods (int versions)
void VoxelData::setVoxel(int x, int y, int z, bool active) {
    setVoxel(glm::ivec3(x, y, z), active);
}

void VoxelData::setVoxel(const glm::ivec3& pos, bool active) {
    glm::vec3 position(pos.x, pos.y, pos.z);
    
    // Find existing voxel at this position
    auto it = std::find_if(voxels_.begin(), voxels_.end(),
        [&position](const Voxel& v) {
            return v.position == position;
        });
    
    if (active) {
        if (it != voxels_.end()) {
            it->active = true;
        } else {
            // Create new voxel
            Voxel voxel;
            voxel.position = position;
            voxel.active = true;
            voxel.color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f); // Default white
            voxel.type = 0;
            voxels_.push_back(voxel);
        }
    } else {
        if (it != voxels_.end()) {
            it->active = false;
        }
    }
}

bool VoxelData::getVoxel(int x, int y, int z) const {
    return getVoxel(glm::ivec3(x, y, z));
}

bool VoxelData::getVoxel(const glm::ivec3& pos) const {
    glm::vec3 position(pos.x, pos.y, pos.z);
    
    auto it = std::find_if(voxels_.begin(), voxels_.end(),
        [&position](const Voxel& v) {
            return v.position == position;
        });
    
    return (it != voxels_.end()) && it->active;
}

// SaveManager compatibility methods (uint32_t versions)
bool VoxelData::getVoxel(uint32_t x, uint32_t y, uint32_t z) const {
    glm::vec3 pos(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z));
    auto it = std::find_if(voxels_.begin(), voxels_.end(),
        [&pos](const Voxel& v) {
            return v.position == pos && v.active;
        });
    return it != voxels_.end();
}

void VoxelData::setVoxel(uint32_t x, uint32_t y, uint32_t z, bool active) {
    glm::vec3 pos(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z));
    auto it = std::find_if(voxels_.begin(), voxels_.end(),
        [&pos](const Voxel& v) {
            return v.position == pos;
        });
    
    if (it != voxels_.end()) {
        it->active = active;
        if (!active) {
            voxels_.erase(it);
        }
    } else if (active) {
        Voxel newVoxel;
        newVoxel.position = pos;
        newVoxel.active = true;
        newVoxel.color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f); // Default white
        newVoxel.type = 0;
        voxels_.push_back(newVoxel);
    }
}

std::vector<Voxel> VoxelData::getActiveVoxels() const {
    std::vector<Voxel> activeVoxels;
    std::copy_if(voxels_.begin(), voxels_.end(), std::back_inserter(activeVoxels),
        [](const Voxel& v) { return v.active; });
    return activeVoxels;
}

glm::vec3 VoxelData::getCenter() const {
    if (voxels_.empty()) {
        return glm::vec3(0.0f);
    }
    
    glm::vec3 center(0.0f);
    for (const auto& voxel : voxels_) {
        center += voxel.position;
    }
    center /= static_cast<float>(voxels_.size());
    return center;
}

float VoxelData::getBoundingRadius() const {
    if (voxels_.empty()) {
        return 0.0f;
    }
    
    glm::vec3 center = getCenter();
    float maxDistance = 0.0f;
    
    for (const auto& voxel : voxels_) {
        float distance = glm::length(voxel.position - center);
        maxDistance = std::max(maxDistance, distance);
    }
    
    return maxDistance;
}

nlohmann::json VoxelData::toJson() const {
    nlohmann::json json;
    json["version"] = "1.0";
    json["voxelCount"] = voxels_.size();
    json["dimensions"] = {dimensions.x, dimensions.y, dimensions.z};
    
    nlohmann::json voxelsArray = nlohmann::json::array();
    for (const auto& voxel : voxels_) {
        voxelsArray.push_back(voxel.toJson());
    }
    json["voxels"] = voxelsArray;
    
    return json;
}

std::unique_ptr<VoxelData> VoxelData::fromJson(const nlohmann::json& json) {
    auto voxelData = std::make_unique<VoxelData>();
    
    if (!json.contains("version") || !json.contains("voxels")) {
        return nullptr;
    }
    
    // Load dimensions if available
    if (json.contains("dimensions")) {
        auto dims = json["dimensions"];
        voxelData->dimensions = glm::ivec3(dims[0], dims[1], dims[2]);
    }
    
    const auto& voxelsArray = json["voxels"];
    for (const auto& voxelJson : voxelsArray) {
        voxelData->addVoxel(Voxel::fromJson(voxelJson));
    }
    
    return voxelData;
}

bool VoxelData::saveToFile(const std::string& filename) const {
    try {
        std::ofstream file(filename);
        if (!file.is_open()) {
            return false;
        }
        
        file << toJson().dump(4);
        return true;
    }
    catch (const std::exception&) {
        return false;
    }
}

std::unique_ptr<VoxelData> VoxelData::loadFromFile(const std::string& filename) {
    try {
        std::ifstream file(filename);
        if (!file.is_open()) {
            return nullptr;
        }
        
        nlohmann::json json;
        file >> json;
        
        return fromJson(json);
    }
    catch (const std::exception&) {
        return nullptr;
    }
} 
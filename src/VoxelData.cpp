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

glm::vec3 VoxelData::getCenter() const {
    if (voxels_.empty()) {
        return glm::vec3(0.0f);
    }
    
    glm::vec3 sum(0.0f);
    for (const auto& voxel : voxels_) {
        sum += voxel.position;
    }
    return sum / static_cast<float>(voxels_.size());
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
#include "VoxelData.h"
#include <fstream>
#include <algorithm>

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
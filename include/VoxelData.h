#pragma once

#include <vector>
#include <glm/glm.hpp>
#include <nlohmann/json.hpp>
#include <string>
#include <memory>

struct Voxel {
    glm::vec3 position;
    glm::vec4 color;
    uint32_t type;
    bool active;

    // Serialization helpers
    nlohmann::json toJson() const {
        return {
            {"position", {position.x, position.y, position.z}},
            {"color", {color.r, color.g, color.b, color.a}},
            {"type", type},
            {"active", active}
        };
    }

    static Voxel fromJson(const nlohmann::json& json) {
        Voxel voxel;
        auto pos = json["position"];
        voxel.position = glm::vec3(pos[0], pos[1], pos[2]);
        auto col = json["color"];
        voxel.color = glm::vec4(col[0], col[1], col[2], col[3]);
        voxel.type = json["type"];
        voxel.active = json["active"];
        return voxel;
    }
};

class VoxelData {
public:
    VoxelData() = default;
    VoxelData(const glm::ivec3& dims) : dimensions(dims) {}
    ~VoxelData() = default;

    // Voxel management
    void addVoxel(const Voxel& voxel);
    void removeVoxel(const glm::vec3& position);
    void clear();
    size_t getVoxelCount() const { return voxels_.size(); }
    const std::vector<Voxel>& getVoxels() const { return voxels_; }
    
    // Grid-based access methods
    void setVoxel(int x, int y, int z, bool active);
    void setVoxel(const glm::ivec3& pos, bool active);
    bool getVoxel(int x, int y, int z) const;
    bool getVoxel(const glm::ivec3& pos) const;
    
    // Grid properties
    glm::ivec3 dimensions{64, 64, 64};
    void setDimensions(const glm::ivec3& dims) { dimensions = dims; }
    glm::ivec3 getDimensions() const { return dimensions; }
    
    // Active voxel iteration
    std::vector<Voxel> getActiveVoxels() const;
    
    // Spatial properties
    glm::vec3 getCenter() const;
    float getBoundingRadius() const;

    // Serialization
    nlohmann::json toJson() const;
    static std::unique_ptr<VoxelData> fromJson(const nlohmann::json& json);

    // File operations
    bool saveToFile(const std::string& filename) const;
    static std::unique_ptr<VoxelData> loadFromFile(const std::string& filename);

private:
    std::vector<Voxel> voxels_;
}; 
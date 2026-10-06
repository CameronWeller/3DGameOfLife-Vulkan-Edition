#pragma once

// The .life3d save format. All values are little-endian, as written by the x86
// and ARM machines the game ships for:
//
//   char[4]   magic "L3D2"
//   uint32    rule index (into lifeRules())
//   uint64    generation
//   float[3]  eye position
//   float     yaw, then float pitch (degrees)
//   uint64    live cell count N, then N x int32[3] (x, y, z)
//   uint64    block count M,     then M x int32[4] (x, y, z, CellKind)
//
// "L3D1" files are the same without the block section; they still load.

#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>

namespace gol3d {

// A static block as stored: its cell, then its CellKind value. Kinds are kept
// as raw numbers because a file may hold kinds this version does not know.
struct SavedBlock {
    glm::ivec3 cell;
    int32_t kind;
    bool operator==(const SavedBlock&) const = default;
};
static_assert(sizeof(SavedBlock) == 16, "four int32 per block in the file");

struct SavedWorld {
    uint32_t ruleIndex = 0;
    uint64_t generation = 0;
    glm::vec3 eye{0.0f};
    float yaw = 0.0f;
    float pitch = 0.0f;
    std::vector<glm::ivec3> cells; // live cells
    std::vector<SavedBlock> blocks;
};

enum class SaveFileStatus {
    Ok,
    NotASave, // missing file, wrong magic, unknown rule or impossible counts
    Truncated,
};

bool writeSaveFile(const std::string& path, const SavedWorld& world);

// `ruleCount` is the number of rules a valid file may refer to. Counts are never
// trusted further than the file's size, so a corrupt file cannot make the game
// allocate gigabytes.
SaveFileStatus readSaveFile(const std::string& path, size_t ruleCount, SavedWorld& out);

} // namespace gol3d

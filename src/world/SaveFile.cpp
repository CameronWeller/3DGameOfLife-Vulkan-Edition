#include "world/SaveFile.h"

#include <fstream>
#include <utility>

namespace gol3d {
namespace {

constexpr char MAGIC_V1[] = "L3D1"; // cells only
constexpr char MAGIC_V2[] = "L3D2"; // cells and blocks

// Fixed-size values are stored as their bytes in memory (little-endian).
template <typename T>
void writeValue(std::ofstream& out, const T& value) {
    out.write(reinterpret_cast<const char*>(&value), sizeof(value));
}

template <typename T>
void readValue(std::ifstream& in, T& value) {
    in.read(reinterpret_cast<char*>(&value), sizeof(value));
}

template <typename T>
void writeArray(std::ofstream& out, const std::vector<T>& values) {
    writeValue(out, static_cast<uint64_t>(values.size()));
    out.write(reinterpret_cast<const char*>(values.data()),
              static_cast<std::streamsize>(values.size() * sizeof(T)));
}

} // namespace

bool writeSaveFile(const std::string& path, const SavedWorld& world) {
    std::ofstream out(path, std::ios::binary);
    out.write(MAGIC_V2, 4);
    writeValue(out, world.ruleIndex);
    writeValue(out, world.generation);
    writeValue(out, world.eye);
    writeValue(out, world.yaw);
    writeValue(out, world.pitch);
    writeArray(out, world.cells);
    writeArray(out, world.blocks);
    return static_cast<bool>(out);
}

SaveFileStatus readSaveFile(const std::string& path, size_t ruleCount, SavedWorld& out) {
    std::ifstream in(path, std::ios::binary);
    char magic[4] = {};
    in.read(magic, 4);
    const std::string format(magic, 4);
    SavedWorld world;
    uint64_t cellCount = 0;
    readValue(in, world.ruleIndex);
    readValue(in, world.generation);
    readValue(in, world.eye);
    readValue(in, world.yaw);
    readValue(in, world.pitch);
    readValue(in, cellCount);

    // How many bytes follow the header, to check the counts against.
    const std::streamoff headerEnd = in.tellg();
    in.seekg(0, std::ios::end);
    const uint64_t bytesLeft = in ? static_cast<uint64_t>(in.tellg() - headerEnd) : 0;
    in.seekg(headerEnd);

    const bool knownFormat = format == MAGIC_V1 || format == MAGIC_V2;
    if (!in || !knownFormat || world.ruleIndex >= ruleCount ||
        cellCount > bytesLeft / sizeof(glm::ivec3)) {
        return SaveFileStatus::NotASave;
    }
    world.cells.resize(cellCount);
    in.read(reinterpret_cast<char*>(world.cells.data()),
            static_cast<std::streamsize>(cellCount * sizeof(glm::ivec3)));

    if (format == MAGIC_V2) {
        uint64_t blockCount = 0;
        readValue(in, blockCount);
        if (blockCount > bytesLeft / sizeof(SavedBlock)) {
            in.setstate(std::ios::failbit);
        } else {
            world.blocks.resize(blockCount);
            in.read(reinterpret_cast<char*>(world.blocks.data()),
                    static_cast<std::streamsize>(blockCount * sizeof(SavedBlock)));
        }
    }
    if (!in) return SaveFileStatus::Truncated;
    out = std::move(world);
    return SaveFileStatus::Ok;
}

} // namespace gol3d

// Reading and writing .life3d files; the format is described in SaveFile.h.

#include "world/SaveFile.h"

#include <fstream>
#include <utility>

namespace gol3d {
namespace {

constexpr char MAGIC_V1[] = "L3D1"; // cells only
constexpr char MAGIC_V2[] = "L3D2"; // cells and blocks
constexpr size_t MAGIC_SIZE = 4;    // the magic is stored without its terminating '\0'

// Fixed-size values are stored as their bytes in memory (little-endian).
template <typename T>
void writeValue(std::ofstream& out, const T& value) {
    out.write(reinterpret_cast<const char*>(&value), sizeof(value));
}

template <typename T>
void readValue(std::ifstream& in, T& value) {
    in.read(reinterpret_cast<char*>(&value), sizeof(value));
}

// A uint64 count, then the elements' bytes.
template <typename T>
void writeArray(std::ofstream& out, const std::vector<T>& values) {
    writeValue(out, static_cast<uint64_t>(values.size()));
    out.write(reinterpret_cast<const char*>(values.data()),
              static_cast<std::streamsize>(values.size() * sizeof(T)));
}

// Reads `count` elements' bytes (the count itself has already been read).
template <typename T>
void readElements(std::ifstream& in, uint64_t count, std::vector<T>& values) {
    values.resize(count);
    in.read(reinterpret_cast<char*>(values.data()),
            static_cast<std::streamsize>(count * sizeof(T)));
}

// Bytes from the read position to the end of the file, or 0 if the stream has
// failed. Leaves the read position where it was.
uint64_t bytesRemaining(std::ifstream& in) {
    const std::streamoff position = in.tellg();
    in.seekg(0, std::ios::end);
    const uint64_t remaining = in ? static_cast<uint64_t>(in.tellg() - position) : 0;
    in.seekg(position);
    return remaining;
}

} // namespace

bool writeSaveFile(const std::string& path, const SavedWorld& world) {
    std::ofstream out(path, std::ios::binary);
    out.write(MAGIC_V2, MAGIC_SIZE);
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
    char magic[MAGIC_SIZE] = {};
    in.read(magic, MAGIC_SIZE);
    const std::string format(magic, MAGIC_SIZE);
    SavedWorld world;
    uint64_t cellCount = 0;
    readValue(in, world.ruleIndex);
    readValue(in, world.generation);
    readValue(in, world.eye);
    readValue(in, world.yaw);
    readValue(in, world.pitch);
    readValue(in, cellCount);

    // Every count must fit in the bytes after the header before anything is
    // allocated for it.
    const uint64_t bytesLeft = bytesRemaining(in);
    const bool knownFormat = format == MAGIC_V1 || format == MAGIC_V2;
    if (!in || !knownFormat || world.ruleIndex >= ruleCount ||
        cellCount > bytesLeft / sizeof(glm::ivec3)) {
        return SaveFileStatus::NotASave;
    }
    readElements(in, cellCount, world.cells);

    if (format == MAGIC_V2) {
        uint64_t blockCount = 0;
        readValue(in, blockCount);
        if (blockCount > bytesLeft / sizeof(SavedBlock)) {
            in.setstate(std::ios::failbit); // reported as Truncated below
        } else {
            readElements(in, blockCount, world.blocks);
        }
    }
    if (!in) return SaveFileStatus::Truncated;
    out = std::move(world);
    return SaveFileStatus::Ok;
}

} // namespace gol3d

// Checks for src/world/SaveFile.h: round trips, the older L3D1 format, and that
// damaged files are rejected instead of loaded.

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>

#include "TestSupport.h"
#include "world/SaveFile.h"

using namespace gol3d;
using testing::expect;

namespace {

// The file layout from SaveFile.h, in bytes.
constexpr size_t HEADER_BYTES = 4 + 4 + 8 + 12 + 4 + 4; // magic, rule, generation, eye, yaw, pitch
constexpr size_t COUNT_BYTES = 8;                       // a uint64 cell or block count
constexpr size_t CELL_BYTES = 3 * 4;                    // int32 x, y, z
constexpr size_t BLOCK_BYTES = 4 * 4;                   // int32 x, y, z, kind
constexpr size_t MAGIC_VERSION_INDEX = 3;               // the '2' of "L3D2"

// How many rules a file may refer to (the game has this many).
constexpr size_t RULE_COUNT = 8;

std::filesystem::path tempFile(const std::string& name) {
    static const std::string RUN_ID = std::to_string(std::random_device{}());
    return std::filesystem::temp_directory_path() / ("gol3d-test-" + RUN_ID + "-" + name);
}

// A world with a value in every field: a generation past 32 bits, negative and
// fractional floats, cells far from the origin in both directions, and one
// block of each static kind (1 Stone, 2 Ember).
SavedWorld sampleWorld() {
    SavedWorld world;
    world.ruleIndex = 3;
    world.generation = 123456789012ull;
    world.eye = glm::vec3(1.5f, -2.25f, 1000.0f);
    world.yaw = 45.0f;
    world.pitch = -12.5f;
    world.cells = {{0, 0, 0}, {-1, 5, 7}, {1048000, -1048000, 3}};
    world.blocks = {{{2, 0, 2}, 1}, {{3, 0, 2}, 2}};
    return world;
}

std::string readBytes(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), {});
}

void writeBytes(const std::filesystem::path& path, const std::string& bytes) {
    std::ofstream out(path, std::ios::binary);
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

void checkRoundTrip() {
    const std::filesystem::path path = tempFile("roundtrip.life3d");
    const SavedWorld original = sampleWorld();
    expect(writeSaveFile(path.string(), original), "a save can be written");
    SavedWorld loaded;
    expect(readSaveFile(path.string(), RULE_COUNT, loaded) == SaveFileStatus::Ok, "and read back");
    expect(loaded.ruleIndex == original.ruleIndex && loaded.generation == original.generation,
           "rule and generation survive");
    expect(loaded.eye == original.eye && loaded.yaw == original.yaw &&
               loaded.pitch == original.pitch,
           "the camera survives");
    expect(loaded.cells == original.cells && loaded.blocks == original.blocks,
           "cells and blocks survive");
    const size_t expectedSize = HEADER_BYTES + COUNT_BYTES + original.cells.size() * CELL_BYTES +
                                COUNT_BYTES + original.blocks.size() * BLOCK_BYTES;
    expect(std::filesystem::file_size(path) == expectedSize, "the file has the documented layout");
    std::filesystem::remove(path);
}

void checkOldFormat() {
    // An L3D1 file is an L3D2 file without the block section.
    const std::filesystem::path path = tempFile("old.life3d");
    SavedWorld world = sampleWorld();
    world.blocks.clear();
    writeSaveFile(path.string(), world);
    std::string bytes = readBytes(path);
    bytes[MAGIC_VERSION_INDEX] = '1';
    bytes.resize(bytes.size() - COUNT_BYTES); // drop the (empty) block count
    writeBytes(path, bytes);
    SavedWorld loaded;
    expect(readSaveFile(path.string(), RULE_COUNT, loaded) == SaveFileStatus::Ok,
           "L3D1 saves still load");
    expect(loaded.cells == world.cells && loaded.blocks.empty(), "with their cells and no blocks");
    std::filesystem::remove(path);
}

void checkDamagedFiles() {
    SavedWorld loaded;
    expect(readSaveFile(tempFile("missing.life3d").string(), RULE_COUNT, loaded) ==
               SaveFileStatus::NotASave,
           "a missing file is not a save");

    const std::filesystem::path path = tempFile("damaged.life3d");
    writeSaveFile(path.string(), sampleWorld());
    const std::string good = readBytes(path);

    writeBytes(path, "PNG?" + good.substr(4));
    expect(readSaveFile(path.string(), RULE_COUNT, loaded) == SaveFileStatus::NotASave,
           "a wrong magic is rejected");

    // The sample world uses rule index 3, which a game with only 2 rules lacks.
    writeBytes(path, good);
    expect(readSaveFile(path.string(), 2, loaded) == SaveFileStatus::NotASave,
           "an unknown rule is rejected");

    // Cut off partway through the last block.
    writeBytes(path, good.substr(0, good.size() - 5));
    expect(readSaveFile(path.string(), RULE_COUNT, loaded) == SaveFileStatus::Truncated,
           "a cut-off file is truncated");

    // A cell count far beyond the file's size must be refused, not allocated.
    std::string huge = good;
    const size_t cellCountOffset = HEADER_BYTES;
    for (size_t i = 0; i < COUNT_BYTES; ++i) {
        huge[cellCountOffset + i] = static_cast<char>(0xFF);
    }
    writeBytes(path, huge);
    expect(readSaveFile(path.string(), RULE_COUNT, loaded) == SaveFileStatus::NotASave,
           "an impossible cell count is rejected");
    std::filesystem::remove(path);
}

} // namespace

int main() {
    checkRoundTrip();
    checkOldFormat();
    checkDamagedFiles();
    return testing::finish("save file");
}

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

constexpr size_t RULE_COUNT = 8;

std::filesystem::path tempFile(const std::string& name) {
    static const std::string RUN_ID = std::to_string(std::random_device{}());
    return std::filesystem::temp_directory_path() / ("gol3d-test-" + RUN_ID + "-" + name);
}

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
    // Header 4 + 4 + 8 + 12 + 4 + 4, then 8 + 3 * 12 for cells, 8 + 2 * 16 for blocks.
    expect(std::filesystem::file_size(path) == 36 + 44 + 40, "the file has the documented layout");
    std::filesystem::remove(path);
}

void checkOldFormat() {
    // An L3D1 file is an L3D2 file without the block section.
    const std::filesystem::path path = tempFile("old.life3d");
    SavedWorld world = sampleWorld();
    world.blocks.clear();
    writeSaveFile(path.string(), world);
    std::string bytes = readBytes(path);
    bytes[3] = '1';
    bytes.resize(bytes.size() - 8); // drop the (empty) block count
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

    writeBytes(path, good);
    expect(readSaveFile(path.string(), 2, loaded) == SaveFileStatus::NotASave,
           "an unknown rule is rejected");

    writeBytes(path, good.substr(0, good.size() - 5));
    expect(readSaveFile(path.string(), RULE_COUNT, loaded) == SaveFileStatus::Truncated,
           "a cut-off file is truncated");

    // A cell count far beyond the file's size must be refused, not allocated.
    std::string huge = good;
    const size_t countOffset = 36;
    for (int i = 0; i < 8; ++i) {
        huge[countOffset + i] = static_cast<char>(0xFF);
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

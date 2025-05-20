#include <gtest/gtest.h>
#include "../src/SaveManager.h"
#include "../src/Logger.h"
#include <filesystem>
#include <fstream>

using namespace VulkanHIP;

class UtilityTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create temporary test directory
        testDir = std::filesystem::temp_directory_path() / "VulkanEngineTest";
        std::filesystem::create_directories(testDir);
    }

    void TearDown() override {
        // Clean up test directory
        std::filesystem::remove_all(testDir);
    }

    std::filesystem::path testDir;
};

// SaveManager Tests
TEST_F(UtilityTest, SaveManagerInitialization) {
    SaveManager saveManager;
    EXPECT_TRUE(std::filesystem::exists(saveManager.getSaveDirectory()));
}

TEST_F(UtilityTest, SaveManagerSaveLoad) {
    SaveManager saveManager;
    saveManager.setSaveDirectory(testDir);

    // Create test data
    VoxelData testData;
    testData.size = 100;
    testData.data = std::vector<uint8_t>(100, 0x42);

    // Test saving
    std::string filename = "test.save";
    EXPECT_TRUE(saveManager.saveCurrentState(filename, testData));
    EXPECT_TRUE(std::filesystem::exists(testDir / filename));

    // Test loading
    VoxelData loadedData;
    EXPECT_TRUE(saveManager.loadSaveFile(filename, loadedData));
    EXPECT_EQ(loadedData.size, testData.size);
    EXPECT_EQ(loadedData.data, testData.data);
}

TEST_F(UtilityTest, SaveManagerDelete) {
    SaveManager saveManager;
    saveManager.setSaveDirectory(testDir);

    // Create and save test data
    VoxelData testData;
    testData.size = 100;
    testData.data = std::vector<uint8_t>(100, 0x42);
    std::string filename = "test.save";
    saveManager.saveCurrentState(filename, testData);

    // Test deletion
    EXPECT_TRUE(saveManager.deleteSaveFile(filename));
    EXPECT_FALSE(std::filesystem::exists(testDir / filename));
}

// Logger Tests
TEST_F(UtilityTest, LoggerInitialization) {
    Logger logger;
    EXPECT_TRUE(std::filesystem::exists(logger.getLogDirectory()));
}

TEST_F(UtilityTest, LoggerWrite) {
    Logger logger;
    logger.setLogDirectory(testDir);

    // Test logging
    logger.log(LogLevel::Info, "Test log message");
    logger.log(LogLevel::Warning, "Test warning message");
    logger.log(LogLevel::Error, "Test error message");

    // Verify log file exists and contains messages
    std::filesystem::path logFile = testDir / "log.txt";
    EXPECT_TRUE(std::filesystem::exists(logFile));
    EXPECT_GT(std::filesystem::file_size(logFile), 0);
}

TEST_F(UtilityTest, LoggerCleanup) {
    Logger logger;
    logger.setLogDirectory(testDir);

    // Create some test logs
    for (int i = 0; i < 10; ++i) {
        logger.log(LogLevel::Info, "Test message " + std::to_string(i));
    }

    // Test cleanup
    logger.cleanupOldLogs(5);
    auto logCount = std::distance(
        std::filesystem::directory_iterator(testDir),
        std::filesystem::directory_iterator()
    );
    EXPECT_LE(logCount, 5);
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
} 
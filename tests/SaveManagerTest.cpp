#include <gtest/gtest.h>
#include "SaveManager.h"
#include <fstream>
#include <cstdio>
#include <string>

class SaveManagerTest : public ::testing::Test {
protected:
    std::string testFile = "test_save_file.sav";
    void TearDown() override {
        std::remove(testFile.c_str());
    }
};

TEST_F(SaveManagerTest, SaveAndLoadData) {
    SaveManager manager;
    std::string data = "GameState:12345";
    // Save data
    bool saveResult = manager.saveToFile(testFile, data);
    EXPECT_TRUE(saveResult);
    // Load data
    std::string loaded;
    bool loadResult = manager.loadFromFile(testFile, loaded);
    EXPECT_TRUE(loadResult);
    EXPECT_EQ(data, loaded);
}

TEST_F(SaveManagerTest, FileNotFound) {
    SaveManager manager;
    std::string loaded;
    bool loadResult = manager.loadFromFile("nonexistent_file.sav", loaded);
    EXPECT_FALSE(loadResult);
}

TEST_F(SaveManagerTest, OverwriteFile) {
    SaveManager manager;
    std::string data1 = "FirstState";
    std::string data2 = "SecondState";
    // Save first
    EXPECT_TRUE(manager.saveToFile(testFile, data1));
    // Overwrite
    EXPECT_TRUE(manager.saveToFile(testFile, data2));
    // Load
    std::string loaded;
    EXPECT_TRUE(manager.loadFromFile(testFile, loaded));
    EXPECT_EQ(data2, loaded);
} 
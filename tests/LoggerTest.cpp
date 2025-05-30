#include <gtest/gtest.h>
#include "Logger.h"
#include <fstream>
#include <string>
#include <filesystem>

class LoggerTest : public ::testing::Test {
protected:
    std::string testLogFile = "test_log.txt";
    void TearDown() override {
        std::filesystem::remove(testLogFile);
    }
};

TEST_F(LoggerTest, BasicLogging) {
    Logger logger(testLogFile);
    logger.setLogLevel(LogLevel::DEBUG);
    
    logger.debug("Debug message");
    logger.info("Info message");
    logger.warning("Warning message");
    logger.error("Error message");
    
    std::ifstream logFile(testLogFile);
    ASSERT_TRUE(logFile.is_open());
    
    std::string line;
    std::getline(logFile, line);
    EXPECT_TRUE(line.find("Debug message") != std::string::npos);
    std::getline(logFile, line);
    EXPECT_TRUE(line.find("Info message") != std::string::npos);
    std::getline(logFile, line);
    EXPECT_TRUE(line.find("Warning message") != std::string::npos);
    std::getline(logFile, line);
    EXPECT_TRUE(line.find("Error message") != std::string::npos);
}

TEST_F(LoggerTest, LogLevelFiltering) {
    Logger logger(testLogFile);
    logger.setLogLevel(LogLevel::WARNING);
    
    logger.debug("Debug message");
    logger.info("Info message");
    logger.warning("Warning message");
    logger.error("Error message");
    
    std::ifstream logFile(testLogFile);
    ASSERT_TRUE(logFile.is_open());
    
    std::string line;
    std::getline(logFile, line);
    EXPECT_TRUE(line.find("Warning message") != std::string::npos);
    std::getline(logFile, line);
    EXPECT_TRUE(line.find("Error message") != std::string::npos);
    
    // Debug and Info messages should not be present
    std::string content((std::istreambuf_iterator<char>(logFile)),
                        std::istreambuf_iterator<char>());
    EXPECT_TRUE(content.find("Debug message") == std::string::npos);
    EXPECT_TRUE(content.find("Info message") == std::string::npos);
}

TEST_F(LoggerTest, FileCreation) {
    Logger logger(testLogFile);
    logger.info("Test message");
    
    EXPECT_TRUE(std::filesystem::exists(testLogFile));
    EXPECT_GT(std::filesystem::file_size(testLogFile), 0);
} 
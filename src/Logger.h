#pragma once

#include <string>
#include <fstream>
#include <mutex>
#include <memory>
#include <chrono>
#include <iomanip>
#include <sstream>

enum class LogLevel {
    VERBOSE,
    INFO,
    WARNING,
    ERROR
};

class Logger {
public:
    static Logger& getInstance();

    void init(const std::string& logFilePath = "");
    void cleanup();

    void log(LogLevel level, const std::string& message);
    void logVulkanValidation(LogLevel level, const std::string& message);

    // Prevent copying
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

private:
    Logger() = default;
    ~Logger();

    std::string getTimestamp();
    std::string getLevelString(LogLevel level);
    void writeToFile(const std::string& message);

    std::mutex logMutex_;
    std::ofstream logFile_;
    bool initialized_ = false;
    std::string logFilePath_;
}; 
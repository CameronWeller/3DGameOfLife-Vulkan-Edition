#pragma once

#include <string>
#include <fstream>
#include <mutex>
#include <memory>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <unordered_set>
#include <atomic>

enum class LogLevel {
    VERBOSE,
    INFO,
    WARNING,
    ERROR
};

enum class LogCategory {
    GENERAL,
    VULKAN,
    WINDOW,
    DEVICE,
    MEMORY,
    RENDERING
};

class Logger {
public:
    static Logger& getInstance();

    void init(const std::string& logFilePath = "");
    void cleanup();

    // Core logging functions
    void log(LogLevel level, LogCategory category, const std::string& message);
    void logVulkanValidation(LogLevel level, const std::string& message);

    // Configuration
    void setMinimumLogLevel(LogLevel level) { minimumLogLevel_ = level; }
    void enableCategory(LogCategory category) { enabledCategories_.insert(category); }
    void disableCategory(LogCategory category) { enabledCategories_.erase(category); }
    void setFlushOnLog(bool flush) { flushOnLog_ = flush; }
    void flush() { 
        std::lock_guard<std::mutex> lock(logMutex_);
        if (logFile_.is_open()) {
            logFile_.flush();
        }
    }

    // Prevent copying
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

private:
    Logger();
    ~Logger();

    std::string getTimestamp();
    std::string getLevelString(LogLevel level);
    std::string getCategoryString(LogCategory category);
    void writeToFile(const std::string& message);
    bool shouldLog(LogLevel level, LogCategory category) const;

    std::mutex logMutex_;
    std::ofstream logFile_;
    bool initialized_ = false;
    std::string logFilePath_;
    std::atomic<LogLevel> minimumLogLevel_{LogLevel::INFO};
    std::unordered_set<LogCategory> enabledCategories_{LogCategory::GENERAL};
    std::atomic<bool> flushOnLog_{true};
}; 
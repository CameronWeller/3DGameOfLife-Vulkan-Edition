#pragma once

#include <string>
#include <fstream>
#include <mutex>
#include <filesystem>

namespace VulkanHIP {

enum class LogLevel {
    Debug,
    Info,
    Warning,
    Error
};

class Logger {
public:
    Logger();
    ~Logger();

    void log(LogLevel level, const std::string& message);
    void cleanupOldLogs(int maxLogs);
    bool isInitialized() const { return file_.is_open(); }
    std::filesystem::path getCurrentLogFile() const { return logFile_; }

private:
    std::string getLevelString(LogLevel level);
    std::filesystem::path logDir_;
    std::filesystem::path logFile_;
    std::ofstream file_;
    std::mutex mutex_;
};

} // namespace VulkanHIP 
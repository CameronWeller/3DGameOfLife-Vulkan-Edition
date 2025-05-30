#pragma once

#include <string>
#include <fstream>
#include <mutex>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <filesystem>
#include <atomic>
#include <iostream>

namespace VulkanHIP {

class Logger {
public:
    enum class LogLevel {
        Debug,
        Info,
        Warning,
        Error,
        Fatal
    };

    static Logger& getInstance() {
        static Logger instance;
        return instance;
    }

    void log(LogLevel level, const std::string& message) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        try {
            if (!logFile_.is_open()) {
                initLogFile();
            }

            auto now = std::chrono::system_clock::now();
            auto time = std::chrono::system_clock::to_time_t(now);
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch()) % 1000;

            std::stringstream ss;
#ifdef _WIN32
            std::tm tm;
            localtime_s(&tm, &time);
            ss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
#else
            ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
#endif
            ss << '.' << std::setfill('0') << std::setw(3) << ms.count()
               << " [" << getLevelString(level) << "] " << message << std::endl;

            logFile_ << ss.str();
            logFile_.flush();

            // Clean up old logs if needed
            cleanupOldLogs();
        } catch (const std::exception& e) {
            // Log to stderr if file logging fails
            std::cerr << "Logger error: " << e.what() << std::endl;
        }
    }

    void setLogLevel(LogLevel level) {
        std::lock_guard<std::mutex> lock(mutex_);
        currentLogLevel_ = level;
    }

    LogLevel getLogLevel() const {
        return currentLogLevel_.load();
    }

    void setLogDirectory(const std::string& directory) {
        std::lock_guard<std::mutex> lock(mutex_);
        logDirectory_ = directory;
        if (logFile_.is_open()) {
            logFile_.close();
        }
        initLogFile();
    }

protected:
    Logger() : currentLogLevel_(LogLevel::Info) {
        initLogFile();
    }

    ~Logger() {
        if (logFile_.is_open()) {
            logFile_.close();
        }
    }

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

private:
    void initLogFile() {
        if (!std::filesystem::exists(logDirectory_)) {
            std::filesystem::create_directories(logDirectory_);
        }

        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
#ifdef _WIN32
        std::tm tm;
        localtime_s(&tm, &time);
        ss << std::put_time(&tm, "%Y%m%d_%H%M%S");
#else
        ss << std::put_time(std::localtime(&time), "%Y%m%d_%H%M%S");
#endif
        std::string filename = logDirectory_ + "/vulkan_hip_" + ss.str() + ".log";

        logFile_.open(filename, std::ios::app);
        if (!logFile_.is_open()) {
            throw std::runtime_error("Failed to open log file: " + filename);
        }
    }

    void cleanupOldLogs() {
        try {
            const size_t maxLogFiles = 10;
            std::vector<std::filesystem::path> logFiles;

            for (const auto& entry : std::filesystem::directory_iterator(logDirectory_)) {
                if (entry.path().extension() == ".log") {
                    logFiles.push_back(entry.path());
                }
            }

            if (logFiles.size() > maxLogFiles) {
                std::sort(logFiles.begin(), logFiles.end(),
                    [](const auto& a, const auto& b) {
                        return std::filesystem::last_write_time(a) < std::filesystem::last_write_time(b);
                    });

                for (size_t i = 0; i < logFiles.size() - maxLogFiles; ++i) {
                    try {
                        std::filesystem::remove(logFiles[i]);
                    } catch (const std::filesystem::filesystem_error& e) {
                        // Log error but continue with other files
                        std::cerr << "Failed to remove old log file: " << e.what() << std::endl;
                    }
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "Error during log cleanup: " << e.what() << std::endl;
        }
    }

    std::string getLevelString(LogLevel level) const {
        switch (level) {
            case LogLevel::Debug:   return "DEBUG";
            case LogLevel::Info:    return "INFO";
            case LogLevel::Warning: return "WARNING";
            case LogLevel::Error:   return "ERROR";
            case LogLevel::Fatal:   return "FATAL";
            default:                return "UNKNOWN";
        }
    }

    std::ofstream logFile_;
    std::mutex mutex_;
    std::atomic<LogLevel> currentLogLevel_;
    std::string logDirectory_ = "logs";
};

} // namespace VulkanHIP 
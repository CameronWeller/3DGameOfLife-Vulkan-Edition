#include "Logger.h"
#include <iostream>
#include <fstream>
#include <chrono>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <vector>
#include <algorithm>
#include <stdexcept>

namespace fs = std::filesystem;

namespace VulkanHIP {

Logger::Logger() {
    try {
        logDir_ = "logs";
        if (!fs::exists(logDir_)) {
            if (!fs::create_directory(logDir_)) {
                throw std::runtime_error("Failed to create log directory");
            }
        }
        logFile_ = logDir_ / ("log_" + std::to_string(std::time(nullptr)) + ".log");
        file_.open(logFile_, std::ios::app);
        if (!file_.is_open()) {
            throw std::runtime_error("Failed to open log file: " + logFile_.string());
        }
    } catch (const std::exception& e) {
        std::cerr << "Logger initialization failed: " << e.what() << std::endl;
        throw;
    }
}

Logger::~Logger() {
    try {
        if (file_.is_open()) {
            file_.close();
        }
    } catch (const std::exception& e) {
        std::cerr << "Logger cleanup failed: " << e.what() << std::endl;
    }
}

void Logger::log(LogLevel level, const std::string& message) {
    try {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!file_.is_open()) {
            file_.open(logFile_, std::ios::app);
            if (!file_.is_open()) {
                throw std::runtime_error("Failed to reopen log file");
            }
        }
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::tm tm;
#ifdef _WIN32
        localtime_s(&tm, &time);
#else
        localtime_r(&time, &tm);
#endif
        file_ << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << " ["
              << getLevelString(level) << "] " << message << std::endl;
        if (file_.fail()) {
            throw std::runtime_error("Failed to write to log file");
        }
        file_.flush();
    } catch (const std::exception& e) {
        std::cerr << "Logging failed: " << e.what() << std::endl;
    }
}

void Logger::cleanupOldLogs(int maxLogs) {
    try {
        std::vector<fs::directory_entry> logFiles;
        for (const auto& entry : fs::directory_iterator(logDir_)) {
            if (entry.is_regular_file() && entry.path().extension() == ".log") {
                logFiles.push_back(entry);
            }
        }
        // Sort by last write time, newest first
        std::sort(logFiles.begin(), logFiles.end(), [](const auto& a, const auto& b) {
            return fs::last_write_time(a) > fs::last_write_time(b);
        });
        // Remove old logs
        for (size_t i = maxLogs; i < logFiles.size(); ++i) {
            fs::remove(logFiles[i]);
        }
    } catch (const std::exception& e) {
        std::cerr << "Log cleanup failed: " << e.what() << std::endl;
    }
}

std::string Logger::getLevelString(LogLevel level) {
    switch (level) {
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info: return "INFO";
        case LogLevel::Warning: return "WARNING";
        case LogLevel::Error: return "ERROR";
        default: return "UNKNOWN";
    }
}

} // namespace VulkanHIP 
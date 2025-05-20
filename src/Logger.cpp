#include "Logger.h"
#include <iostream>

Logger::Logger() {
    // Enable all categories by default
    enabledCategories_.insert(LogCategory::GENERAL);
    enabledCategories_.insert(LogCategory::VULKAN);
    enabledCategories_.insert(LogCategory::WINDOW);
    enabledCategories_.insert(LogCategory::DEVICE);
    enabledCategories_.insert(LogCategory::MEMORY);
    enabledCategories_.insert(LogCategory::RENDERING);
}

Logger& Logger::getInstance() {
    static Logger instance;
    return instance;
}

void Logger::init(const std::string& logFilePath) {
    std::lock_guard<std::mutex> lock(logMutex_);
    
    if (initialized_) {
        return;
    }

    if (logFilePath.empty()) {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << "engine_log_" << std::put_time(std::localtime(&time), "%Y%m%d_%H%M%S") << ".log";
        logFilePath_ = ss.str();
    } else {
        logFilePath_ = logFilePath;
    }

    logFile_.open(logFilePath_, std::ios::out | std::ios::app);
    if (!logFile_.is_open()) {
        std::cerr << "Failed to open log file: " << logFilePath_ << std::endl;
        return;
    }

    initialized_ = true;
    log(LogLevel::INFO, LogCategory::GENERAL, "Logger initialized");
}

void Logger::cleanup() {
    std::lock_guard<std::mutex> lock(logMutex_);
    if (initialized_) {
        log(LogLevel::INFO, LogCategory::GENERAL, "Logger shutting down");
        logFile_.close();
        initialized_ = false;
    }
}

Logger::~Logger() {
    cleanup();
}

void Logger::log(LogLevel level, LogCategory category, const std::string& message) {
    if (!shouldLog(level, category)) {
        return;
    }

    std::lock_guard<std::mutex> lock(logMutex_);
    
    std::stringstream ss;
    ss << getTimestamp() << " [" << getCategoryString(category) << "] [" << getLevelString(level) << "] " << message << std::endl;
    
    // Always output to console
    std::cerr << ss.str();
    
    // Write to file if initialized
    if (initialized_) {
        writeToFile(ss.str());
        if (flushOnLog_) {
            logFile_.flush();
        }
    }
}

void Logger::logVulkanValidation(LogLevel level, const std::string& message) {
    log(level, LogCategory::VULKAN, message);
}

std::string Logger::getTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

std::string Logger::getLevelString(LogLevel level) {
    switch (level) {
        case LogLevel::VERBOSE: return "VERBOSE";
        case LogLevel::INFO: return "INFO";
        case LogLevel::WARNING: return "WARNING";
        case LogLevel::ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

std::string Logger::getCategoryString(LogCategory category) {
    switch (category) {
        case LogCategory::GENERAL: return "GENERAL";
        case LogCategory::VULKAN: return "VULKAN";
        case LogCategory::WINDOW: return "WINDOW";
        case LogCategory::DEVICE: return "DEVICE";
        case LogCategory::MEMORY: return "MEMORY";
        case LogCategory::RENDERING: return "RENDERING";
        default: return "UNKNOWN";
    }
}

bool Logger::shouldLog(LogLevel level, LogCategory category) const {
    return static_cast<int>(level) >= static_cast<int>(minimumLogLevel_) &&
           enabledCategories_.find(category) != enabledCategories_.end();
}

void Logger::writeToFile(const std::string& message) {
    if (logFile_.is_open()) {
        logFile_ << message;
    }
} 
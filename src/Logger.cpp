#include "Logger.h"
#include <iostream>

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
    log(LogLevel::INFO, "Logger initialized");
}

void Logger::cleanup() {
    std::lock_guard<std::mutex> lock(logMutex_);
    if (initialized_) {
        log(LogLevel::INFO, "Logger shutting down");
        logFile_.close();
        initialized_ = false;
    }
}

Logger::~Logger() {
    cleanup();
}

void Logger::log(LogLevel level, const std::string& message) {
    std::lock_guard<std::mutex> lock(logMutex_);
    
    std::stringstream ss;
    ss << getTimestamp() << " [" << getLevelString(level) << "] " << message << std::endl;
    
    // Always output to console
    std::cerr << ss.str();
    
    // Write to file if initialized
    if (initialized_) {
        writeToFile(ss.str());
    }
}

void Logger::logVulkanValidation(LogLevel level, const std::string& message) {
    std::lock_guard<std::mutex> lock(logMutex_);
    
    std::stringstream ss;
    ss << getTimestamp() << " [VULKAN] [" << getLevelString(level) << "] " << message << std::endl;
    
    // Always output to console
    std::cerr << ss.str();
    
    // Write to file if initialized
    if (initialized_) {
        writeToFile(ss.str());
    }
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

void Logger::writeToFile(const std::string& message) {
    if (logFile_.is_open()) {
        logFile_ << message;
        logFile_.flush();
    }
} 
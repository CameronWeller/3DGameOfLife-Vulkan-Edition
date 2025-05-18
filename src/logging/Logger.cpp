#include "Logger.h"
#include <filesystem>
#include <iostream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace logging {

Logger& Logger::getInstance() {
    static Logger instance;
    return instance;
}

Logger::Logger() : currentLevel(LogLevel::INFO), maxFileSize(10485760), maxFiles(5) {
    // Try to load configuration
    try {
        std::ifstream configFile("config/app_config.json");
        if (configFile.is_open()) {
            json config;
            configFile >> config;
            
            // Set log level
            std::string levelStr = config["logging"]["level"];
            if (levelStr == "trace") currentLevel = LogLevel::TRACE;
            else if (levelStr == "debug") currentLevel = LogLevel::DEBUG;
            else if (levelStr == "info") currentLevel = LogLevel::INFO;
            else if (levelStr == "warning") currentLevel = LogLevel::WARNING;
            else if (levelStr == "error") currentLevel = LogLevel::ERROR;
            else if (levelStr == "fatal") currentLevel = LogLevel::FATAL;
            
            // Set log file
            logFilename = config["logging"]["file"];
            maxFileSize = config["logging"]["maxFileSize"];
            maxFiles = config["logging"]["maxFiles"];
            
            setLogFile(logFilename);
        }
    } catch (const std::exception& e) {
        std::cerr << "Failed to load logging configuration: " << e.what() << std::endl;
    }
}

Logger::~Logger() {
    if (logFile.is_open()) {
        logFile.close();
    }
}

void Logger::setLogLevel(LogLevel level) {
    currentLevel = level;
}

void Logger::setLogFile(const std::string& filename) {
    std::lock_guard<std::mutex> lock(logMutex);
    
    if (logFile.is_open()) {
        logFile.close();
    }
    
    // Create directory if it doesn't exist
    std::filesystem::path logPath(filename);
    std::filesystem::create_directories(logPath.parent_path());
    
    logFile.open(filename, std::ios::app);
    if (!logFile.is_open()) {
        throw std::runtime_error("Failed to open log file: " + filename);
    }
    
    logFilename = filename;
}

void Logger::writeLog(LogLevel level, const std::string& message) {
    std::lock_guard<std::mutex> lock(logMutex);
    
    // Check if we need to rotate logs
    if (logFile.is_open()) {
        logFile.seekp(0, std::ios::end);
        if (logFile.tellp() >= maxFileSize) {
            logFile.close();
            
            // Rotate log files
            for (size_t i = maxFiles - 1; i > 0; --i) {
                std::string oldName = logFilename + "." + std::to_string(i);
                std::string newName = logFilename + "." + std::to_string(i + 1);
                std::filesystem::rename(oldName, newName);
            }
            
            std::filesystem::rename(logFilename, logFilename + ".1");
            logFile.open(logFilename, std::ios::app);
        }
    }
    
    // Write log message
    std::string output = getTimestamp() + " [" + getLevelString(level) + "] " + message + "\n";
    
    if (logFile.is_open()) {
        logFile << output;
        logFile.flush();
    }
    
    // Also output to console for warnings and above
    if (level >= LogLevel::WARNING) {
        std::cerr << output;
    } else {
        std::cout << output;
    }
}

std::string Logger::getLevelString(LogLevel level) {
    switch (level) {
        case LogLevel::TRACE:   return "TRACE";
        case LogLevel::DEBUG:   return "DEBUG";
        case LogLevel::INFO:    return "INFO";
        case LogLevel::WARNING: return "WARNING";
        case LogLevel::ERROR:   return "ERROR";
        case LogLevel::FATAL:   return "FATAL";
        default:               return "UNKNOWN";
    }
}

std::string Logger::getTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
    ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return ss.str();
}

} // namespace logging 
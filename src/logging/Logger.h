#pragma once

#include <string>
#include <fstream>
#include <mutex>
#include <memory>
#include <sstream>
#include <chrono>
#include <iomanip>

namespace logging {

enum class LogLevel {
    TRACE,
    DEBUG,
    INFO,
    WARNING,
    ERROR,
    FATAL
};

class Logger {
public:
    static Logger& getInstance();
    
    void setLogLevel(LogLevel level);
    void setLogFile(const std::string& filename);
    
    template<typename... Args>
    void log(LogLevel level, const std::string& format, Args&&... args) {
        if (level < currentLevel) return;
        
        std::stringstream ss;
        formatMessage(ss, format, std::forward<Args>(args)...);
        writeLog(level, ss.str());
    }
    
    // Convenience methods
    template<typename... Args>
    void trace(const std::string& format, Args&&... args) {
        log(LogLevel::TRACE, format, std::forward<Args>(args)...);
    }
    
    template<typename... Args>
    void debug(const std::string& format, Args&&... args) {
        log(LogLevel::DEBUG, format, std::forward<Args>(args)...);
    }
    
    template<typename... Args>
    void info(const std::string& format, Args&&... args) {
        log(LogLevel::INFO, format, std::forward<Args>(args)...);
    }
    
    template<typename... Args>
    void warning(const std::string& format, Args&&... args) {
        log(LogLevel::WARNING, format, std::forward<Args>(args)...);
    }
    
    template<typename... Args>
    void error(const std::string& format, Args&&... args) {
        log(LogLevel::ERROR, format, std::forward<Args>(args)...);
    }
    
    template<typename... Args>
    void fatal(const std::string& format, Args&&... args) {
        log(LogLevel::FATAL, format, std::forward<Args>(args)...);
    }

private:
    Logger();
    ~Logger();
    
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    
    void writeLog(LogLevel level, const std::string& message);
    std::string getLevelString(LogLevel level);
    std::string getTimestamp();
    
    template<typename T>
    void formatMessage(std::stringstream& ss, const std::string& format, T&& value) {
        size_t pos = format.find("{}");
        if (pos != std::string::npos) {
            ss << format.substr(0, pos) << value << format.substr(pos + 2);
        } else {
            ss << format;
        }
    }
    
    template<typename T, typename... Args>
    void formatMessage(std::stringstream& ss, const std::string& format, T&& value, Args&&... args) {
        size_t pos = format.find("{}");
        if (pos != std::string::npos) {
            ss << format.substr(0, pos) << value;
            formatMessage(ss, format.substr(pos + 2), std::forward<Args>(args)...);
        } else {
            ss << format;
        }
    }
    
    void formatMessage(std::stringstream& ss, const std::string& format) {
        ss << format;
    }
    
    LogLevel currentLevel;
    std::ofstream logFile;
    std::mutex logMutex;
    std::string logFilename;
    size_t maxFileSize;
    size_t maxFiles;
};

} // namespace logging

// Convenience macros
#define LOG_TRACE(...) logging::Logger::getInstance().trace(__VA_ARGS__)
#define LOG_DEBUG(...) logging::Logger::getInstance().debug(__VA_ARGS__)
#define LOG_INFO(...) logging::Logger::getInstance().info(__VA_ARGS__)
#define LOG_WARNING(...) logging::Logger::getInstance().warning(__VA_ARGS__)
#define LOG_ERROR(...) logging::Logger::getInstance().error(__VA_ARGS__)
#define LOG_FATAL(...) logging::Logger::getInstance().fatal(__VA_ARGS__) 
#pragma once

#include <string>
#include <fstream>
#include <iostream>
#include <sstream>
#include <memory>
#include <mutex>
#include <ctime>
#include <iomanip>
#include "logger_interface.h"

namespace rule_checker {


class DefaultLogger : public ILogger {
public:
    static DefaultLogger& instance();
    
    void set_log_file(const std::string& filepath);
    void enable_console(bool enable);
    void enable_file(bool enable);
    
    LogLevel get_level() const override { return level_; }
    
    // Main logging functions
    void log(LogLevel level, const std::string& msg, const std::string& file = "", int line = 0) override;
    
private:
    DefaultLogger();
    ~DefaultLogger();
    
    DefaultLogger(const DefaultLogger&) = delete;
    DefaultLogger& operator=(const DefaultLogger&) = delete;
    
    void init_from_env();
    std::string level_to_string(LogLevel level) const;
    std::string format_timestamp() const;
    std::string format_message(LogLevel level, const std::string& msg, const std::string& file, int line) const;
    
    LogLevel level_;
    bool console_enabled_;
    bool file_enabled_;
    std::string log_file_path_;
    std::ofstream log_file_;
    mutable std::mutex mutex_;
    int indent_level_;
};

// Logger facade that can use either default or injected logger
class Logger {
public:
    static Logger& instance();
    
    // Inject external logger (call this before using the logger)
    // Pass nullptr to reset to default logger
    static void set_logger(std::shared_ptr<ILogger> logger);
    
    // Reset to default logger
    static void reset_to_default();
    
    // Check if using external logger
    static bool is_using_external_logger();
    
    // Main logging functions
    void log(LogLevel level, const std::string& msg, const std::string& file = "", int line = 0);
    
    // Convenience functions
    void trace(const std::string& msg, const std::string& file = "", int line = 0);
    void debug(const std::string& msg, const std::string& file = "", int line = 0);
    void info(const std::string& msg, const std::string& file = "", int line = 0);
    void warn(const std::string& msg, const std::string& file = "", int line = 0);
    void error(const std::string& msg, const std::string& file = "", int line = 0);
    
    LogLevel get_level() const;
    // Access to default logger for configuration (only works when using default logger)
    DefaultLogger* get_default_logger();
    
private:
    Logger() = default;
    
    static std::shared_ptr<ILogger> current_logger_;
    static std::mutex logger_mutex_;
};

// Convenience macros (unchanged - they now use the Logger facade)
#define LOG_TRACE(msg) rule_checker::Logger::instance().trace(msg, __FILE__, __LINE__)
#define LOG_DEBUG(msg) rule_checker::Logger::instance().debug(msg, __FILE__, __LINE__)
#define LOG_INFO(msg) rule_checker::Logger::instance().info(msg, __FILE__, __LINE__)
#define LOG_WARN(msg) rule_checker::Logger::instance().warn(msg, __FILE__, __LINE__)
#define LOG_ERROR(msg) rule_checker::Logger::instance().error(msg, __FILE__, __LINE__)

} // namespace rule_checker

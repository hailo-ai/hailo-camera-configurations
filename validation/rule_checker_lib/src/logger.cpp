#include "logger.h"
#include "logger_interface.h" 
#include <cstdlib>
#include <algorithm>
#include <cctype>

namespace rule_checker {


DefaultLogger& DefaultLogger::instance() {
    static DefaultLogger instance;
    return instance;
}

DefaultLogger::DefaultLogger() 
    : level_(LogLevel::INFO)
    , console_enabled_(true)
    , file_enabled_(false)
    , indent_level_(0) {
    init_from_env();
}

DefaultLogger::~DefaultLogger() {
    if (log_file_.is_open()) {
        log_file_.close();
    }
}

void DefaultLogger::init_from_env() {
    // Read RULE_CHECKER_LOG_LEVEL environment variable
    const char* level_env = std::getenv("RULE_CHECKER_LOG_LEVEL");
    if (level_env) {
        std::string level_str(level_env);
        std::transform(level_str.begin(), level_str.end(), level_str.begin(), ::toupper);
        
        if (level_str == "TRACE") level_ = LogLevel::TRACE;
        else if (level_str == "DEBUG") level_ = LogLevel::DEBUG;
        else if (level_str == "INFO") level_ = LogLevel::INFO;
        else if (level_str == "WARN" || level_str == "WARNING") level_ = LogLevel::WARN;
        else if (level_str == "ERROR") level_ = LogLevel::ERROR;
        else if (level_str == "OFF") level_ = LogLevel::OFF;
    }
    
    // Read RULE_CHECKER_LOG_FILE environment variable
    const char* file_env = std::getenv("RULE_CHECKER_LOG_FILE");
    if (file_env) {
        set_log_file(file_env);
        file_enabled_ = true;
    }
    
    // Read RULE_CHECKER_LOG_CONSOLE environment variable (default: true)
    const char* console_env = std::getenv("RULE_CHECKER_LOG_CONSOLE");
    if (console_env) {
        std::string console_str(console_env);
        std::transform(console_str.begin(), console_str.end(), console_str.begin(), ::tolower);
        console_enabled_ = (console_str == "1" || console_str == "true" || console_str == "yes");
    }
}
void DefaultLogger::set_log_file(const std::string& filepath) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (log_file_.is_open()) {
        log_file_.close();
    }
    
    log_file_path_ = filepath;
    log_file_.open(filepath, std::ios::out | std::ios::app);
    
    if (!log_file_.is_open()) {
        std::cerr << "Failed to open log file: " << filepath << std::endl;
        file_enabled_ = false;
    }
}

void DefaultLogger::enable_console(bool enable) {
    std::lock_guard<std::mutex> lock(mutex_);
    console_enabled_ = enable;
}

void DefaultLogger::enable_file(bool enable) {
    std::lock_guard<std::mutex> lock(mutex_);
    file_enabled_ = enable;
}

std::string DefaultLogger::level_to_string(LogLevel level) const {
    switch (level) {
        case LogLevel::TRACE: return "TRACE";
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO:  return "INFO ";
        case LogLevel::WARN:  return "WARN ";
        case LogLevel::ERROR: return "ERROR";
        case LogLevel::OFF:   return "OFF  ";
        default: return "UNKNOWN";
    }
}

std::string DefaultLogger::format_timestamp() const {
    auto now = std::time(nullptr);
    auto tm = *std::localtime(&now);
    
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

std::string DefaultLogger::format_message(LogLevel level, const std::string& msg, 
                                   const std::string& file, int line) const {
    std::ostringstream oss;
    oss << "[" << format_timestamp() << "] "
        << "[" << level_to_string(level) << "] ";
    
    if (!file.empty() && line > 0) {
        // Extract just the filename from the full path
        size_t pos = file.find_last_of("/\\");
        std::string filename = (pos != std::string::npos) ? file.substr(pos + 1) : file;
        oss << "[" << filename << ":" << line << "] ";
    }
    
    oss << msg;
    return oss.str();
}

void DefaultLogger::log(LogLevel level, const std::string& msg, const std::string& file, int line) {
    if (level < level_) {
        return; // Message level below threshold
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::string formatted = format_message(level, msg, file, line);
    
    if (console_enabled_) {
        if (level >= LogLevel::ERROR) {
            std::cerr << formatted << std::endl;
        } else {
            std::cout << formatted << std::endl;
        }
    }
    
    if (file_enabled_ && log_file_.is_open()) {
        log_file_ << formatted << std::endl;
        log_file_.flush(); // Ensure immediate write
    }
}

// ============================================================================
// Logger Facade Implementation
// ============================================================================

std::shared_ptr<ILogger> Logger::current_logger_ = nullptr;
std::mutex Logger::logger_mutex_;

Logger& Logger::instance() {
    static Logger instance;
    return instance;
}

void Logger::set_logger(std::shared_ptr<ILogger> logger) {
    std::lock_guard<std::mutex> lock(logger_mutex_);
    current_logger_ = logger;
}

void Logger::reset_to_default() {
    std::lock_guard<std::mutex> lock(logger_mutex_);
    current_logger_ = nullptr;
}

bool Logger::is_using_external_logger() {
    std::lock_guard<std::mutex> lock(logger_mutex_);
    return current_logger_ != nullptr;
}

void Logger::log(LogLevel level, const std::string& msg, const std::string& file, int line) {
    std::lock_guard<std::mutex> lock(logger_mutex_);
    
    if (current_logger_) {
        current_logger_->log(level, msg, file, line);
    } else {
        DefaultLogger::instance().log(level, msg, file, line);
    }
}

void Logger::trace(const std::string& msg, const std::string& file, int line) {
    log(LogLevel::TRACE, msg, file, line);
}

void Logger::debug(const std::string& msg, const std::string& file, int line) {
    log(LogLevel::DEBUG, msg, file, line);
}

void Logger::info(const std::string& msg, const std::string& file, int line) {
    log(LogLevel::INFO, msg, file, line);
}

void Logger::warn(const std::string& msg, const std::string& file, int line) {
    log(LogLevel::WARN, msg, file, line);
}

void Logger::error(const std::string& msg, const std::string& file, int line) {
    log(LogLevel::ERROR, msg, file, line);
}

LogLevel Logger::get_level() const {
    std::lock_guard<std::mutex> lock(logger_mutex_);
    
    if (current_logger_) {
        return current_logger_->get_level();
    } else {
        return DefaultLogger::instance().get_level();
    }
}

DefaultLogger* Logger::get_default_logger() {
    std::lock_guard<std::mutex> lock(logger_mutex_);
    
    if (current_logger_) {
        return nullptr; // External logger is active
    }
    
    return &DefaultLogger::instance();
}

void set_logger(std::shared_ptr<ILogger> logger) {
    Logger::set_logger(logger);
}
} // namespace rule_checker

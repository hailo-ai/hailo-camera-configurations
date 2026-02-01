#pragma once
#include <string>

namespace rule_checker {
enum class LogLevel {
    TRACE = 0,
    DEBUG = 1,
    INFO = 2,
    WARN = 3,
    ERROR = 4,
    OFF = 5
};

// Abstract logger interface for external logger injection
class ILogger {
public:
    virtual ~ILogger() = default;
    
    virtual void log(LogLevel level, const std::string& msg, const std::string& file, int line) = 0;
    virtual LogLevel get_level() const = 0;
};

void set_logger(std::shared_ptr<ILogger> logger);
}
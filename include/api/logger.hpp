#pragma once

#include <mutex>
#include <string>

enum class LogLevel {
    Info,
    Warn,
    Error
};

std::string to_string(LogLevel level);

class Logger {
public:
    static Logger& instance();

    void log(LogLevel level, const std::string& message);
    void info(const std::string& message);
    void warn(const std::string& message);
    void error(const std::string& message);

private:
    Logger() = default;
    std::mutex mutex_;
};

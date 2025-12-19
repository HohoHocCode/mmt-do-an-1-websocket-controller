#include "api/logger.hpp"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>

namespace {
std::string timestamp_now() {
    using clock = std::chrono::system_clock;
    const auto now = clock::now();
    const auto time = clock::to_time_t(now);
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &time);
#else
    localtime_r(&time, &tm);
#endif

    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() % 1000;
    oss << "." << std::setw(3) << std::setfill('0') << ms;
    return oss.str();
}
} // namespace

Logger& Logger::instance() {
    static Logger logger;
    return logger;
}

std::string to_string(LogLevel level) {
    switch (level) {
        case LogLevel::Info: return "INFO";
        case LogLevel::Warn: return "WARN";
        case LogLevel::Error: return "ERROR";
    }
    return "INFO";
}

void Logger::log(LogLevel level, const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::cout << "[" << timestamp_now() << "][" << to_string(level) << "] "
              << message << std::endl;
}

void Logger::info(const std::string& message) { log(LogLevel::Info, message); }
void Logger::warn(const std::string& message) { log(LogLevel::Warn, message); }
void Logger::error(const std::string& message) { log(LogLevel::Error, message); }

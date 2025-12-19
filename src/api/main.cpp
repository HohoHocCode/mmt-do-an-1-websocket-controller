#include "api/http_server.hpp"
#include "api/logger.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
std::string env_or(const char* key, const std::string& fallback) {
    const char* value = std::getenv(key);
    if (value && *value) return std::string(value);
    return fallback;
}

unsigned int env_or_uint(const char* key, unsigned int fallback) {
    const char* value = std::getenv(key);
    if (!value || !*value) return fallback;
    try {
        return static_cast<unsigned int>(std::stoul(value));
    } catch (...) {
        return fallback;
    }
}
} // namespace

int main() {
    try {
        DbConfig cfg;
        cfg.host = env_or("DB_HOST", "127.0.0.1");
        cfg.user = env_or("DB_USER", "root");
        cfg.password = env_or("DB_PASSWORD", "");
        cfg.database = env_or("DB_NAME", "mmt_remote");
        cfg.port = env_or_uint("DB_PORT", 3306);

        const unsigned short port = static_cast<unsigned short>(env_or_uint("API_PORT", 8080));

        ApiServer server("0.0.0.0", port, Database(cfg));
        server.run();
    } catch (const std::exception& e) {
        Logger::instance().error(std::string("API crashed: ") + e.what());
        return 1;
    }
    return 0;
}

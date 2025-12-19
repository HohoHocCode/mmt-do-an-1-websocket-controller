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

bool parse_port_value(const std::string& value, unsigned short& port) {
    try {
        const auto parsed = std::stoul(value);
        if (parsed == 0 || parsed > 65535) return false;
        port = static_cast<unsigned short>(parsed);
        return true;
    } catch (...) {
        return false;
    }
}

struct ApiRuntimeConfig {
    std::string host;
    unsigned short port;
};

ApiRuntimeConfig resolve_runtime_config(int argc, char* argv[]) {
    ApiRuntimeConfig config;
    config.host = env_or("HOST", "0.0.0.0");

    unsigned int port_candidate = env_or_uint("PORT", 0);
    if (port_candidate == 0) {
        port_candidate = env_or_uint("API_PORT", 8080);
    }
    if (port_candidate == 0 || port_candidate > 65535) {
        port_candidate = 8080;
    }
    config.port = static_cast<unsigned short>(port_candidate);

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--host" && i + 1 < argc) {
            config.host = argv[++i];
            continue;
        }
        if (arg.rfind("--host=", 0) == 0) {
            config.host = arg.substr(std::string("--host=").size());
            continue;
        }
        if (arg == "--port" && i + 1 < argc) {
            unsigned short parsed = 0;
            if (parse_port_value(argv[i + 1], parsed)) {
                config.port = parsed;
            }
            ++i;
            continue;
        }
        if (arg.rfind("--port=", 0) == 0) {
            unsigned short parsed = 0;
            if (parse_port_value(arg.substr(std::string("--port=").size()), parsed)) {
                config.port = parsed;
            }
            continue;
        }
    }

    return config;
}
} // namespace

int main(int argc, char* argv[]) {
    try {
        DbConfig cfg;
        cfg.host = env_or("DB_HOST", "127.0.0.1");
        cfg.user = env_or("DB_USER", "root");
        cfg.password = env_or("DB_PASSWORD", "");
        cfg.database = env_or("DB_NAME", "mmt_remote");
        cfg.port = env_or_uint("DB_PORT", 3306);

        const ApiRuntimeConfig runtime = resolve_runtime_config(argc, argv);

        Logger::instance().info("DB config: host=" + cfg.host + " port=" + std::to_string(cfg.port) +
                                " db=" + cfg.database);

        Database database(cfg);
        try {
            database.connect();
        } catch (const std::exception& e) {
            Logger::instance().error(std::string("DB connection check failed: ") + e.what());
        }

        ApiServer server(runtime.host, runtime.port, std::move(database));
        server.run();
    } catch (const std::exception& e) {
        Logger::instance().error(std::string("API crashed: ") + e.what());
        return 1;
    }
    return 0;
}

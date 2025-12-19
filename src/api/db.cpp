#include "api/db.hpp"
#include "api/logger.hpp"

#include <stdexcept>

void MysqlDeleter::operator()(MYSQL* handle) const {
    if (handle != nullptr) {
        mysql_close(handle);
    }
}

Database::Database(DbConfig cfg) : config_(std::move(cfg)) {}

UniqueMysql Database::connect() const {
    UniqueMysql handle(mysql_init(nullptr));
    if (!handle) {
        throw std::runtime_error("Failed to initialize MySQL handle");
    }

    unsigned int timeout = 5;
    mysql_options(handle.get(), MYSQL_OPT_CONNECT_TIMEOUT, &timeout);

    if (!mysql_real_connect(handle.get(),
                            config_.host.c_str(),
                            config_.user.c_str(),
                            config_.password.c_str(),
                            config_.database.c_str(),
                            config_.port,
                            nullptr,
                            0)) {
        const char* err = mysql_error(handle.get());
        throw std::runtime_error(err ? err : "Unknown MySQL connection error");
    }

    Logger::instance().info("Connected to MySQL at " + config_.host + ":" + std::to_string(config_.port));
    return handle;
}

#pragma once

#include <mysql.h>

#include <memory>
#include <optional>
#include <string>

struct MysqlDeleter {
    void operator()(MYSQL* handle) const;
};

using UniqueMysql = std::unique_ptr<MYSQL, MysqlDeleter>;

struct DbConfig {
    std::string host = "127.0.0.1";
    std::string user = "root";
    std::string password;
    std::string database = "mmt_remote";
    unsigned int port = 3306;
};

class Database {
public:
    explicit Database(DbConfig cfg);

    UniqueMysql connect() const;
    const DbConfig& config() const { return config_; }

private:
    DbConfig config_;
};

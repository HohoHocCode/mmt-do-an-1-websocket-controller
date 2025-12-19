#include "api/auth_service.hpp"
#include "api/logger.hpp"

#include <cstring>
#include <memory>
#include <stdexcept>

#ifndef my_bool
#define my_bool bool
#endif
namespace {
void close_stmt(MYSQL_STMT* stmt) {
    if (stmt != nullptr) {
        mysql_stmt_close(stmt);
    }
}
} // namespace

AuthService::AuthService(Database db) : db_(std::move(db)) {}

UserLookupResult AuthService::get_user(const std::string& username) const {
    try {
        auto conn = db_.connect();

        std::unique_ptr<MYSQL_STMT, decltype(&mysql_stmt_close)> stmt(mysql_stmt_init(conn.get()), &mysql_stmt_close);
        if (!stmt) {
            Logger::instance().error("mysql_stmt_init failed");
            return UserLookupResult{std::nullopt, true};
        }

        const char* sql = "SELECT id, username, password_hash, created_at FROM users WHERE username = ?";
        if (mysql_stmt_prepare(stmt.get(), sql, static_cast<unsigned long>(std::strlen(sql))) != 0) {
            Logger::instance().error("mysql_stmt_prepare failed: " + std::string(mysql_stmt_error(stmt.get())));
            return UserLookupResult{std::nullopt, true};
        }

        MYSQL_BIND param{};
        std::memset(&param, 0, sizeof(param));
        param.buffer_type = MYSQL_TYPE_STRING;
        param.buffer = const_cast<char*>(username.c_str());
        param.buffer_length = static_cast<unsigned long>(username.size());

        if (mysql_stmt_bind_param(stmt.get(), &param) != 0) {
            Logger::instance().error("mysql_stmt_bind_param failed: " + std::string(mysql_stmt_error(stmt.get())));
            return UserLookupResult{std::nullopt, true};
        }

        if (mysql_stmt_execute(stmt.get()) != 0) {
            Logger::instance().error("mysql_stmt_execute failed: " + std::string(mysql_stmt_error(stmt.get())));
            return UserLookupResult{std::nullopt, true};
        }

        if (mysql_stmt_store_result(stmt.get()) != 0) {
            Logger::instance().error("mysql_stmt_store_result failed: " + std::string(mysql_stmt_error(stmt.get())));
            return UserLookupResult{std::nullopt, true};
        }

        int id = 0;
        char username_buf[256] = {0};
        unsigned long username_len = 0;
        char password_buf[512] = {0};
        unsigned long password_len = 0;
        bool password_null = false;
        char created_buf[64] = {0};
        unsigned long created_len = 0;
        bool created_null = false;

        MYSQL_BIND result[4];
        std::memset(result, 0, sizeof(result));

        result[0].buffer_type = MYSQL_TYPE_LONG;
        result[0].buffer = &id;

        result[1].buffer_type = MYSQL_TYPE_STRING;
        result[1].buffer = username_buf;
        result[1].buffer_length = sizeof(username_buf);
        result[1].length = &username_len;

        result[2].buffer_type = MYSQL_TYPE_STRING;
        result[2].buffer = password_buf;
        result[2].buffer_length = sizeof(password_buf);
        result[2].length = &password_len;
        result[2].is_null = reinterpret_cast<my_bool*>(&password_null);

        result[3].buffer_type = MYSQL_TYPE_STRING;
        result[3].buffer = created_buf;
        result[3].buffer_length = sizeof(created_buf);
        result[3].length = &created_len;
        result[3].is_null = reinterpret_cast<my_bool*>(&created_null);

        if (mysql_stmt_bind_result(stmt.get(), result) != 0) {
            Logger::instance().error("mysql_stmt_bind_result failed: " + std::string(mysql_stmt_error(stmt.get())));
            return UserLookupResult{std::nullopt, true};
        }

        const int fetch_code = mysql_stmt_fetch(stmt.get());
        if (fetch_code == MYSQL_NO_DATA) {
            return UserLookupResult{std::nullopt, false};
        }

        if (fetch_code != 0 && fetch_code != MYSQL_DATA_TRUNCATED) {
            Logger::instance().error("mysql_stmt_fetch failed: " + std::string(mysql_stmt_error(stmt.get())));
            return UserLookupResult{std::nullopt, true};
        }

        UserWithSecret result_row;
        result_row.user.id = id;
        result_row.user.username = std::string(username_buf, username_len);
        result_row.user.created_at = created_null ? "" : std::string(created_buf, created_len);
        result_row.user.has_password = !password_null && password_len > 0;
        result_row.user.role = "user";

        if (!password_null && password_len > 0) {
            result_row.password_hash = std::string(password_buf, password_len);
        }
        return UserLookupResult{result_row, false};
    } catch (const std::exception& e) {
        Logger::instance().error(std::string("get_user failed: ") + e.what());
        return UserLookupResult{std::nullopt, true};
    }
}

bool AuthService::save_user(const std::string& username, const std::string& password_hash) {
    try {
        auto conn = db_.connect();
        std::unique_ptr<MYSQL_STMT, decltype(&mysql_stmt_close)> stmt(mysql_stmt_init(conn.get()), &mysql_stmt_close);
        if (!stmt) return false;

        const char* sql = "INSERT INTO users(username, password_hash) VALUES(?, ?)";
        if (mysql_stmt_prepare(stmt.get(), sql, static_cast<unsigned long>(std::strlen(sql))) != 0) {
            Logger::instance().error("prepare insert failed: " + std::string(mysql_stmt_error(stmt.get())));
            return false;
        }

        MYSQL_BIND params[2];
        std::memset(params, 0, sizeof(params));

        params[0].buffer_type = MYSQL_TYPE_STRING;
        params[0].buffer = const_cast<char*>(username.c_str());
        params[0].buffer_length = static_cast<unsigned long>(username.size());

        params[1].buffer_type = MYSQL_TYPE_STRING;
        params[1].buffer = const_cast<char*>(password_hash.c_str());
        params[1].buffer_length = static_cast<unsigned long>(password_hash.size());
        my_bool password_is_null = password_hash.empty() ? 1 : 0;
        params[1].is_null = &password_is_null;

        if (mysql_stmt_bind_param(stmt.get(), params) != 0) {
            Logger::instance().error("bind insert failed: " + std::string(mysql_stmt_error(stmt.get())));
            return false;
        }

        const bool ok = mysql_stmt_execute(stmt.get()) == 0;
        if (!ok) {
            Logger::instance().warn("insert user failed: " + std::string(mysql_stmt_error(stmt.get())));
        }
        return ok;
    } catch (const std::exception& e) {
        Logger::instance().error(std::string("save_user failed: ") + e.what());
        return false;
    }
}

bool AuthService::update_password_if_missing(const std::string& username, const std::string& password_hash) {
    try {
        auto conn = db_.connect();
        std::unique_ptr<MYSQL_STMT, decltype(&mysql_stmt_close)> stmt(mysql_stmt_init(conn.get()), &mysql_stmt_close);
        if (!stmt) return false;

        const char* sql = "UPDATE users SET password_hash = ? WHERE username = ? AND password_hash IS NULL";
        if (mysql_stmt_prepare(stmt.get(), sql, static_cast<unsigned long>(std::strlen(sql))) != 0) {
            Logger::instance().error("prepare update failed: " + std::string(mysql_stmt_error(stmt.get())));
            return false;
        }

        MYSQL_BIND params[2];
        std::memset(params, 0, sizeof(params));

        params[0].buffer_type = MYSQL_TYPE_STRING;
        params[0].buffer = const_cast<char*>(password_hash.c_str());
        params[0].buffer_length = static_cast<unsigned long>(password_hash.size());

        params[1].buffer_type = MYSQL_TYPE_STRING;
        params[1].buffer = const_cast<char*>(username.c_str());
        params[1].buffer_length = static_cast<unsigned long>(username.size());

        if (mysql_stmt_bind_param(stmt.get(), params) != 0) {
            Logger::instance().error("bind update failed: " + std::string(mysql_stmt_error(stmt.get())));
            return false;
        }

        const bool ok = mysql_stmt_execute(stmt.get()) == 0;
        if (!ok) {
            Logger::instance().warn("update password failed: " + std::string(mysql_stmt_error(stmt.get())));
        }
        return ok;
    } catch (const std::exception& e) {
        Logger::instance().error(std::string("update_password failed: ") + e.what());
        return false;
    }
}

void AuthService::remember_token(const std::string& token, const AuthUserRecord& user) {
    std::unique_lock<std::shared_mutex> lock(tokens_mutex_);
    sessions_[token] = user;
}

Json AuthService::precheck(const std::string& username) {
    if (username.size() < 2) {
        return Json{
            {"exists", false},
            {"hasPassword", false}
        };
    }
    auto row = get_user(username);
    Json res;
    if (row.db_error) {
        res["exists"] = false;
        res["hasPassword"] = false;
        res["error"] = "db_unavailable";
        return res;
    }
    res["exists"] = row.user.has_value();
    res["hasPassword"] = row.user.has_value() && row.user->user.has_password;
    return res;
}

Json AuthService::register_user(const std::string& username, const std::string& password) {
    if (username.size() < 2) {
        return {{"ok", false}, {"error", "username_too_short"}};
    }

    try {
        auto existing = get_user(username);
        if (existing.db_error) {
            return {{"ok", false}, {"error", "db_unavailable"}};
        }
        if (existing.user.has_value()) {
            return {{"ok", false}, {"error", "user_exists"}};
        }

        std::string password_hash;
        if (!password.empty()) {
            password_hash = format_password_hash(derive_password_hash(password));
        }

        const bool ok = save_user(username, password_hash);
        if (!ok) {
            return {{"ok", false}, {"error", "create_failed"}};
        }

        Json user_json;
        user_json["username"] = username;
        user_json["role"] = "user";
        user_json["hasPassword"] = !password_hash.empty();
        return {{"ok", true}, {"user", user_json}};
    } catch (const std::exception& e) {
        Logger::instance().error(std::string("register_user failed: ") + e.what());
        return {{"ok", false}, {"error", "internal_error"}};
    }
}

LoginOutcome AuthService::login(const std::string& username, const std::string& password) {
    try {
        auto row = get_user(username);
        if (row.db_error) {
            return LoginOutcome{LoginStatus::DbUnavailable, std::nullopt};
        }
        if (!row.user.has_value()) {
            return LoginOutcome{LoginStatus::NotFound, std::nullopt};
        }
        if (!row.user->password_hash.has_value() || row.user->password_hash->empty()) {
            return LoginOutcome{LoginStatus::NeedsPasswordSet, std::nullopt};
        }

        if (!verify_password_hash(password, *row.user->password_hash)) {
            return LoginOutcome{LoginStatus::InvalidCredentials, std::nullopt};
        }

        auto token = generate_token();
        remember_token(token, row.user->user);
        return LoginOutcome{LoginStatus::Ok, LoginResult{token, row.user->user}};
    } catch (const std::exception& e) {
        Logger::instance().error(std::string("login failed: ") + e.what());
        return LoginOutcome{LoginStatus::Error, std::nullopt};
    }
}

Json AuthService::set_password(const std::string& username, const std::string& password) {
    if (password.size() < 8) {
        return {{"ok", false}, {"error", "weak_password"}};
    }
    try {
        auto row = get_user(username);
        if (row.db_error) {
            return {{"ok", false}, {"error", "db_unavailable"}};
        }
        if (!row.user.has_value()) {
            return {{"ok", false}, {"error", "not_found"}};
        }
        if (row.user->user.has_password) {
            return {{"ok", false}, {"error", "password_already_set"}};
        }

        const auto hashed = format_password_hash(derive_password_hash(password));
        const bool ok = update_password_if_missing(username, hashed);
        return Json{{"ok", ok}};
    } catch (const std::exception& e) {
        Logger::instance().error(std::string("set_password failed: ") + e.what());
        return {{"ok", false}, {"error", "internal_error"}};
    }
}

std::optional<AuthUserRecord> AuthService::verify(const std::string& token) const {
    std::shared_lock<std::shared_mutex> lock(tokens_mutex_);
    auto it = sessions_.find(token);
    if (it == sessions_.end()) return std::nullopt;
    return it->second;
}

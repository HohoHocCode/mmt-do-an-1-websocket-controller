#pragma once

#include "api/db.hpp"
#include "api/password_hash.hpp"
#include "utils/json.hpp"

#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>

struct AuthUserRecord {
    int id = 0;
    std::string username;
    std::string role = "user";
    bool has_password = false;
    std::string created_at;
};

struct LoginResult {
    std::string token;
    AuthUserRecord user;
};

struct UserWithSecret {
    AuthUserRecord user;
    std::optional<std::string> password_hash;
};

class AuthService {
public:
    explicit AuthService(Database db);

    Json precheck(const std::string& username);
    Json register_user(const std::string& username, const std::string& password);
    std::optional<LoginResult> login(const std::string& username, const std::string& password);
    Json set_password(const std::string& username, const std::string& password);
    std::optional<AuthUserRecord> verify(const std::string& token) const;

private:
    Database db_;
    mutable std::shared_mutex tokens_mutex_;
    std::unordered_map<std::string, AuthUserRecord> sessions_;

    std::optional<UserWithSecret> get_user(const std::string& username) const;
    bool save_user(const std::string& username, const std::string& password_hash);
    bool update_password_if_missing(const std::string& username, const std::string& password_hash);
    void remember_token(const std::string& token, const AuthUserRecord& user);
};

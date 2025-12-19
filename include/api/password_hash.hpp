#pragma once

#include <optional>
#include <string>
#include <vector>

struct PasswordHash {
    std::string salt_hex;
    std::string hash_hex;
};

PasswordHash derive_password_hash(const std::string& password, unsigned int iterations = 120000);
bool verify_password_hash(const std::string& password, const std::string& stored);
std::optional<PasswordHash> parse_password_hash(const std::string& stored);
std::string format_password_hash(const PasswordHash& hash);
std::string generate_token(std::size_t bytes = 32);

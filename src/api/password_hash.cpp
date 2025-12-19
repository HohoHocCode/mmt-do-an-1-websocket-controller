#include "api/password_hash.hpp"

#include <openssl/evp.h>
#include <openssl/rand.h>

#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace {
std::string to_hex(const std::vector<unsigned char>& data) {
    std::ostringstream oss;
    for (unsigned char c : data) {
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(c);
    }
    return oss.str();
}

std::vector<unsigned char> from_hex(const std::string& hex) {
    if (hex.size() % 2 != 0) return {};
    std::vector<unsigned char> out(hex.size() / 2);
    try {
        for (std::size_t i = 0; i < hex.size(); i += 2) {
            out[i / 2] = static_cast<unsigned char>(std::stoi(hex.substr(i, 2), nullptr, 16));
        }
    } catch (...) {
        return {};
    }
    return out;
}
} // namespace

PasswordHash derive_password_hash(const std::string& password, unsigned int iterations) {
    std::vector<unsigned char> salt(16);
    if (RAND_bytes(salt.data(), static_cast<int>(salt.size())) != 1) {
        throw std::runtime_error("Unable to generate salt");
    }

    std::vector<unsigned char> key(32);
    if (PKCS5_PBKDF2_HMAC(password.c_str(),
                          static_cast<int>(password.size()),
                          salt.data(),
                          static_cast<int>(salt.size()),
                          static_cast<int>(iterations),
                          EVP_sha256(),
                          static_cast<int>(key.size()),
                          key.data()) != 1) {
        throw std::runtime_error("Password hashing failed");
    }

    PasswordHash hash;
    hash.salt_hex = to_hex(salt);
    hash.hash_hex = to_hex(key);
    return hash;
}

bool verify_password_hash(const std::string& password, const std::string& stored) {
    auto parsed = parse_password_hash(stored);
    if (!parsed.has_value()) return false;

    auto salt = from_hex(parsed->salt_hex);
    auto expected = from_hex(parsed->hash_hex);
    if (salt.empty() || expected.empty()) return false;

    std::vector<unsigned char> key(expected.size());
    if (PKCS5_PBKDF2_HMAC(password.c_str(),
                          static_cast<int>(password.size()),
                          salt.data(),
                          static_cast<int>(salt.size()),
                          120000,
                          EVP_sha256(),
                          static_cast<int>(key.size()),
                          key.data()) != 1) {
        return false;
    }

    if (key.size() != expected.size()) return false;
    bool match = true;
    for (std::size_t i = 0; i < key.size(); ++i) {
        match = match && key[i] == expected[i];
    }
    return match;
}

std::optional<PasswordHash> parse_password_hash(const std::string& stored) {
    auto pos = stored.find(':');
    if (pos == std::string::npos) return std::nullopt;
    PasswordHash hash;
    hash.salt_hex = stored.substr(0, pos);
    hash.hash_hex = stored.substr(pos + 1);
    if (hash.salt_hex.size() < 2 || hash.hash_hex.size() < 2) return std::nullopt;
    return hash;
}

std::string format_password_hash(const PasswordHash& hash) {
    return hash.salt_hex + ":" + hash.hash_hex;
}

std::string generate_token(std::size_t bytes) {
    std::vector<unsigned char> raw(bytes);
    if (RAND_bytes(raw.data(), static_cast<int>(raw.size())) != 1) {
        throw std::runtime_error("Failed to generate secure token");
    }
    return to_hex(raw);
}

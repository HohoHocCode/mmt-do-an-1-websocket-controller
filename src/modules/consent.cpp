#include "modules/consent.hpp"
#include <iostream>
#include <cstdlib>
#include <string>
#include <cctype>

namespace {
bool env_flag(const char* key) {
    const char* val = std::getenv(key);
    if (!val) return false;
    std::string s(val);
    for (auto& c : s) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s == "1" || s == "true" || s == "yes" || s == "on";
}
} // namespace

bool ConsentManager::request_permission(const std::string& client_ip) {
    std::cout << "[ConsentManager] Stub request_permission từ client "
              << client_ip << "\n";
    if (env_flag("CONSENT_AUTO_APPROVE")) {
        session_active_ = true;
    } else {
        session_active_ = false;
    }
    return session_active_;
}

bool ConsentManager::is_session_active() const {
    return session_active_;
}

void ConsentManager::end_session() {
    std::cout << "[ConsentManager] end_session()\n";
    session_active_ = false;
}

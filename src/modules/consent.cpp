#include "modules/consent.hpp"
#include <iostream>

bool ConsentManager::request_permission(const std::string& client_ip) {
    std::cout << "[ConsentManager] Stub request_permission từ client "
              << client_ip << "\n";
    // Bước 8 sẽ hiện MessageBox / UI thật sự
    session_active_ = true; // tạm thời luôn cho phép
    return session_active_;
}

bool ConsentManager::is_session_active() const {
    return session_active_;
}

void ConsentManager::end_session() {
    std::cout << "[ConsentManager] end_session()\n";
    session_active_ = false;
}

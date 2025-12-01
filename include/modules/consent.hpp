#pragma once
#include <string>

class ConsentManager {
public:
    bool request_permission(const std::string& client_ip);
    bool is_session_active() const;
    void end_session();

private:
    bool session_active_ = false;
};

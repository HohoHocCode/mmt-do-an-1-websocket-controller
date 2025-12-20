#pragma once

#include <string>

class SystemControl {
public:
    bool shutdown();
    bool restart();
    bool get_clipboard_text(std::string& output, std::string& error) const;
    bool send_mouse_move(double x, double y, std::string& error) const;
    bool send_mouse_button(const std::string& action, const std::string& button, std::string& error) const;
    bool send_mouse_wheel(int delta_y, std::string& error) const;
    bool send_key_event(const std::string& action,
                        const std::string& code,
                        const std::string& key,
                        std::string& error) const;
};

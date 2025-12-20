#pragma once

#include <string>

class SystemControl {
public:
    bool shutdown();
    bool restart();
    bool get_clipboard_text(std::string& output, std::string& error) const;
};

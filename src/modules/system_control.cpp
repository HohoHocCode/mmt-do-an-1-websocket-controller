#include "modules/system_control.hpp"
#include <iostream>
#include <string>
#include <optional>
#include <cctype>

#ifdef _WIN32
#include <windows.h>
#include <cwchar>
#include <unordered_map>
#endif

bool SystemControl::shutdown() {
    std::cout << "[SystemControl] Stub shutdown()\n";
    return false;
}

bool SystemControl::restart() {
    std::cout << "[SystemControl] Stub restart()\n";
    return false;
}

#ifdef _WIN32
namespace {
constexpr double kAbsoluteScale = 65535.0;

WORD map_button_flag(const std::string& button, const std::string& action) {
    if (button == "left") {
        return action == "down" ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP;
    }
    if (button == "right") {
        return action == "down" ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP;
    }
    if (button == "middle") {
        return action == "down" ? MOUSEEVENTF_MIDDLEDOWN : MOUSEEVENTF_MIDDLEUP;
    }
    return 0;
}

std::optional<WORD> map_key_code(const std::string& code, const std::string& key) {
    if (code == "Enter") return VK_RETURN;
    if (code == "Escape") return VK_ESCAPE;
    if (code == "Backspace") return VK_BACK;
    if (code == "Tab") return VK_TAB;
    if (code == "Space") return VK_SPACE;
    if (code == "ArrowLeft") return VK_LEFT;
    if (code == "ArrowRight") return VK_RIGHT;
    if (code == "ArrowUp") return VK_UP;
    if (code == "ArrowDown") return VK_DOWN;
    if (code == "Delete") return VK_DELETE;
    if (code == "Home") return VK_HOME;
    if (code == "End") return VK_END;
    if (code == "PageUp") return VK_PRIOR;
    if (code == "PageDown") return VK_NEXT;
    if (code == "Insert") return VK_INSERT;
    if (code == "ControlLeft" || code == "ControlRight") return VK_CONTROL;
    if (code == "ShiftLeft" || code == "ShiftRight") return VK_SHIFT;
    if (code == "AltLeft" || code == "AltRight") return VK_MENU;
    if (code == "MetaLeft" || code == "MetaRight") return VK_LWIN;

    if (code.rfind("Key", 0) == 0 && code.size() == 4) {
        char letter = static_cast<char>(std::toupper(code[3]));
        return static_cast<WORD>(letter);
    }
    if (code.rfind("Digit", 0) == 0 && code.size() == 6) {
        char digit = code[5];
        return static_cast<WORD>(digit);
    }
    if (code.size() == 2 && code[0] == 'F') {
        int f_key = code[1] - '0';
        if (f_key >= 1 && f_key <= 9) {
            return static_cast<WORD>(VK_F1 + (f_key - 1));
        }
    }
    if (code.size() == 3 && code[0] == 'F') {
        int f_key = std::stoi(code.substr(1));
        if (f_key >= 10 && f_key <= 12) {
            return static_cast<WORD>(VK_F1 + (f_key - 1));
        }
    }

    if (!key.empty()) {
        wchar_t wide = static_cast<wchar_t>(key[0]);
        SHORT vk = VkKeyScanW(wide);
        if (vk != -1) {
            return static_cast<WORD>(vk & 0xFF);
        }
    }
    return std::nullopt;
}

bool is_extended_key(WORD vk) {
    switch (vk) {
        case VK_LEFT:
        case VK_RIGHT:
        case VK_UP:
        case VK_DOWN:
        case VK_HOME:
        case VK_END:
        case VK_PRIOR:
        case VK_NEXT:
        case VK_INSERT:
        case VK_DELETE:
            return true;
        default:
            return false;
    }
}
} // namespace
#endif

bool SystemControl::get_clipboard_text(std::string& output, std::string& error) const {
#ifdef _WIN32
    output.clear();
    error.clear();

    if (!OpenClipboard(nullptr)) {
        error = "OpenClipboard failed";
        return false;
    }

    struct ClipboardCloser {
        ~ClipboardCloser() { CloseClipboard(); }
    } close_guard;

    HANDLE handle = GetClipboardData(CF_UNICODETEXT);
    if (!handle) {
        error = "Clipboard has no text";
        return false;
    }

    const wchar_t* data = static_cast<const wchar_t*>(GlobalLock(handle));
    if (!data) {
        error = "Failed to read clipboard text";
        return false;
    }

    struct UnlockGuard {
        HANDLE handle;
        ~UnlockGuard() { GlobalUnlock(handle); }
    } unlock_guard{handle};

    size_t length = std::wcslen(data);
    const size_t max_chars = 100000;
    if (length > max_chars) length = max_chars;

    std::wstring wide_text(data, length);
    if (wide_text.empty()) {
        output.clear();
        return true;
    }

    int needed = WideCharToMultiByte(CP_UTF8, 0, wide_text.c_str(),
                                     static_cast<int>(wide_text.size()), nullptr, 0, nullptr, nullptr);
    if (needed <= 0) {
        error = "Failed to convert clipboard text";
        return false;
    }

    output.resize(static_cast<size_t>(needed));
    int written = WideCharToMultiByte(CP_UTF8, 0, wide_text.c_str(),
                                      static_cast<int>(wide_text.size()),
                                      output.data(), needed, nullptr, nullptr);
    if (written <= 0) {
        error = "Failed to convert clipboard text";
        output.clear();
        return false;
    }

    return true;
#else
    output.clear();
    error = "clipboard supported on Windows only";
    return false;
#endif
}

bool SystemControl::send_mouse_move(double x, double y, std::string& error) const {
#ifdef _WIN32
    if (x < 0.0 || x > 1.0 || y < 0.0 || y > 1.0) {
        error = "coordinates_out_of_range";
        return false;
    }

    INPUT input = {};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE;
    input.mi.dx = static_cast<LONG>(x * kAbsoluteScale);
    input.mi.dy = static_cast<LONG>(y * kAbsoluteScale);
    if (SendInput(1, &input, sizeof(INPUT)) != 1) {
        error = "sendinput_failed";
        return false;
    }
    return true;
#else
    (void)x;
    (void)y;
    error = "not_supported";
    return false;
#endif
}

bool SystemControl::send_mouse_button(const std::string& action, const std::string& button, std::string& error) const {
#ifdef _WIN32
    WORD flag = map_button_flag(button, action);
    if (flag == 0) {
        error = "invalid_button";
        return false;
    }

    INPUT input = {};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = flag;
    if (SendInput(1, &input, sizeof(INPUT)) != 1) {
        error = "sendinput_failed";
        return false;
    }
    return true;
#else
    (void)action;
    (void)button;
    error = "not_supported";
    return false;
#endif
}

bool SystemControl::send_mouse_wheel(int delta_y, std::string& error) const {
#ifdef _WIN32
    INPUT input = {};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_WHEEL;
    input.mi.mouseData = static_cast<DWORD>(delta_y);
    if (SendInput(1, &input, sizeof(INPUT)) != 1) {
        error = "sendinput_failed";
        return false;
    }
    return true;
#else
    (void)delta_y;
    error = "not_supported";
    return false;
#endif
}

bool SystemControl::send_key_event(const std::string& action,
                                   const std::string& code,
                                   const std::string& key,
                                   std::string& error) const {
#ifdef _WIN32
    auto maybe_vk = map_key_code(code, key);
    if (!maybe_vk) {
        error = "unsupported_key";
        return false;
    }

    INPUT input = {};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = *maybe_vk;
    input.ki.dwFlags = 0;
    if (is_extended_key(*maybe_vk)) {
        input.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
    }
    if (action == "up") {
        input.ki.dwFlags |= KEYEVENTF_KEYUP;
    } else if (action != "down") {
        error = "invalid_action";
        return false;
    }

    if (SendInput(1, &input, sizeof(INPUT)) != 1) {
        error = "sendinput_failed";
        return false;
    }
    return true;
#else
    (void)action;
    (void)code;
    (void)key;
    error = "not_supported";
    return false;
#endif
}

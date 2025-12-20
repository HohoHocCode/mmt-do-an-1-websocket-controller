#include "modules/system_control.hpp"
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#include <cwchar>
#endif

bool SystemControl::shutdown() {
    std::cout << "[SystemControl] Stub shutdown()\n";
    return false;
}

bool SystemControl::restart() {
    std::cout << "[SystemControl] Stub restart()\n";
    return false;
}

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

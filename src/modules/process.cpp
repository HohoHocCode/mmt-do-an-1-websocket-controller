#include "modules/process.hpp"

#include <filesystem>
#include <stdexcept>
#include <sstream>
#include <string>

#if defined(_WIN32)
#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <codecvt>
#include <locale>
#endif

namespace {
Json error_response(const std::string& code, const std::string& message, int native_code = 0) {
    Json resp;
    resp["status"] = "error";
    resp["code"] = code;
    resp["message"] = message;
    if (native_code != 0) {
        resp["nativeCode"] = native_code;
    }
    return resp;
}

#if defined(_WIN32)
std::wstring utf8_to_wide(const std::string& s) {
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> conv;
    return conv.from_bytes(s);
}

std::string wide_to_utf8(const std::wstring& ws) {
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> conv;
    return conv.to_bytes(ws);
}

std::string format_last_error(DWORD code) {
    LPWSTR buffer = nullptr;
    const DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
    DWORD length = FormatMessageW(flags, nullptr, code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                                  reinterpret_cast<LPWSTR>(&buffer), 0, nullptr);
    if (length == 0 || buffer == nullptr) return "unknown";
    std::wstring msg(buffer, length);
    LocalFree(buffer);
    return wide_to_utf8(msg);
}
#endif
} // namespace

Json ProcessManager::list_processes()
{
#if defined(_WIN32)
    Json arr = Json::array();

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return error_response("snapshot_failed", "Failed to create process snapshot", static_cast<int>(GetLastError()));
    }

    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);

    if (!Process32FirstW(snapshot, &entry)) {
        CloseHandle(snapshot);
        return error_response("enumeration_failed", "Process enumeration failed", static_cast<int>(GetLastError()));
    }

    do {
        Json p;
        p["pid"]  = static_cast<int>(entry.th32ProcessID);
        p["name"] = wide_to_utf8(entry.szExeFile);

        HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, entry.th32ProcessID);
        if (process) {
            std::wstring image(32768, L'\0');
            DWORD size = static_cast<DWORD>(image.size());
            if (QueryFullProcessImageNameW(process, 0, image.data(), &size)) {
                image.resize(size);
                p["cmdline"] = wide_to_utf8(image);
            }
            CloseHandle(process);
        }
        arr.push_back(p);

    } while (Process32NextW(snapshot, &entry));

    CloseHandle(snapshot);

    return {{"status","ok"},{"data", arr}};
#else
    return error_response("unsupported", "Process inspection is only supported on Windows builds");
#endif
}

Json ProcessManager::kill_process(int pid)
{
    if (pid <= 0) {
        return error_response("invalid_pid", "PID must be positive");
    }

#if defined(_WIN32)
    HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, static_cast<DWORD>(pid));
    if (!h) return error_response("open_process_failed", "OpenProcess failed", static_cast<int>(GetLastError()));

    BOOL ok = TerminateProcess(h, 1);
    const DWORD err = GetLastError();
    CloseHandle(h);

    if (!ok) return error_response("terminate_failed", "TerminateProcess failed", static_cast<int>(err));

    return {{"status","ok"},{"message","Process terminated"},{"pid", pid}};
#else
    return error_response("unsupported", "Process termination is only supported on Windows builds");
#endif
}

Json ProcessManager::start_process(const std::string& path)
{
    if (path.empty()) {
        return error_response("missing_path", "Executable path is required");
    }

#if defined(_WIN32)
    try {
        std::filesystem::path exe_path(path);
        if (!std::filesystem::exists(exe_path)) {
            return error_response("path_not_found", "Executable path does not exist");
        }

        std::wstring wpath = exe_path.wstring();

        STARTUPINFOW si{};
        PROCESS_INFORMATION pi{};
        si.cb = sizeof(si);

        std::wstring command = L"\"" + wpath + L"\"";
        wchar_t* cmd = _wcsdup(command.c_str());

        BOOL ok = CreateProcessW(
            nullptr,
            cmd,
            nullptr,
            nullptr,
            FALSE,
            CREATE_NEW_PROCESS_GROUP,
            nullptr,
            exe_path.parent_path().empty() ? nullptr : exe_path.parent_path().wstring().c_str(),
            &si,
            &pi
        );
        const DWORD err = GetLastError();
        free(cmd);

        if (!ok) {
            return error_response("create_process_failed", "CreateProcess failed: " + format_last_error(err), static_cast<int>(err));
        }

        const int new_pid = static_cast<int>(pi.dwProcessId);

        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);

        return {{"status","ok"},{"message","Process started"},{"pid", new_pid}};
    } catch (const std::exception& e) {
        return error_response("path_error", e.what());
    }
#else
    return error_response("unsupported", "Process start is only supported on Windows builds");
#endif
}

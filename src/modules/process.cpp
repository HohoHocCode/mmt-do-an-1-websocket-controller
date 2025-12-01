#include "modules/process.hpp"
#include <windows.h>
#include <tlhelp32.h>
#include <iostream>
#include <codecvt>
#include <locale>

static std::wstring utf8_to_wide(const std::string& s)
{
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> conv;
    return conv.from_bytes(s);
}

Json ProcessManager::list_processes()
{
    Json arr = Json::array();

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return {{"status","error"},{"message","Failed to snapshot"}};
    }

    PROCESSENTRY32 entry;
    entry.dwSize = sizeof(entry);

    if (!Process32First(snapshot, &entry)) {
        CloseHandle(snapshot);
        return {{"status","error"},{"message","Process32First failed"}};
    }

    do {
        Json p;
        p["pid"]  = (int)entry.th32ProcessID;
        p["name"] = std::string(entry.szExeFile);
        arr.push_back(p);

    } while (Process32Next(snapshot, &entry));

    CloseHandle(snapshot);

    return {{"status","ok"},{"data", arr}};
}

Json ProcessManager::kill_process(int pid)
{
    HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    if (!h) return {{"status","error"},{"message","OpenProcess failed"}};

    BOOL ok = TerminateProcess(h, 1);
    CloseHandle(h);

    if (!ok) return {{"status","error"},{"message","TerminateProcess failed"}};

    return {{"status","ok"},{"message","Process terminated"}};
}

Json ProcessManager::start_process(const std::string& path)
{
    std::wstring wpath = utf8_to_wide(path);

    STARTUPINFOW si{};
    PROCESS_INFORMATION pi{};
    si.cb = sizeof(si);

    wchar_t* cmd = _wcsdup(wpath.c_str());

    BOOL ok = CreateProcessW(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
    free(cmd);

    if (!ok) {
        return {{"status","error"},{"message","CreateProcess failed"},{"code",(int)GetLastError()}};
    }

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return {{"status","ok"},{"message","Process started"}};
}

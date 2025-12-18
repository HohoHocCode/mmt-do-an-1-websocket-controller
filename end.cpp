#include <windows.h>
#include <tlhelp32.h>
#include <iostream>
#include <vector>
#include <string>

// Kill 1 process theo PID
bool KillProcessByPID(DWORD pid) {
    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    if (!hProcess) return false;

    BOOL result = TerminateProcess(hProcess, 0);
    CloseHandle(hProcess);
    return result;
}

// Lấy danh sách PID theo tên process
std::vector<DWORD> GetPIDsByName(const std::wstring& processName) {
    std::vector<DWORD> pids;
    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);

    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

    if (hSnapshot == INVALID_HANDLE_VALUE) return pids;

    if (Process32First(hSnapshot, &pe32)) {
        do {
            // Convert CHAR[260] → wstring
            std::wstring exeName =
                std::wstring(pe32.szExeFile, pe32.szExeFile + strlen(pe32.szExeFile));

            if (processName == exeName) {
                pids.push_back(pe32.th32ProcessID);
            }

        } while (Process32Next(hSnapshot, &pe32));
    }

    CloseHandle(hSnapshot);
    return pids;
}

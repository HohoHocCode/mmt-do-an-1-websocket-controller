#include <iostream>
#include <string>
#include <windows.h>

#include "list.h"
#include "start.h"
#include "end.h"

int main() {
    std::wstring name = L"Zalo.exe";

    auto pids = GetPIDsByName(name);

    if (pids.empty()) {
        std::wcout << L"Khong tim thay process\n";
        return 0;
    }

    for (DWORD pid : pids) {
        if (KillProcessByPID(pid)) {
            std::wcout << L"Da kill PID: " << pid << "\n";
        } else {
            std::wcout << L"Khong kill duoc PID: " << pid << "\n";
        }
    }

    return 0;
}

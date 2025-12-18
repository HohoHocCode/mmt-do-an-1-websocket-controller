#include <windows.h>
#include <tlhelp32.h>
#include <iostream>
#include "list.h"

void ListApplication() {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

    PROCESSENTRY32 pe;
    pe.dwSize = sizeof(pe);

    if (Process32First(snap, &pe)) {
        do {
            std::wcout << pe.szExeFile << " | PID = " << pe.th32ProcessID << "\n";
        } while (Process32Next(snap, &pe));
    }

    CloseHandle(snap);
}

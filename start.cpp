#include <iostream>
#include <windows.h>
#include "start.h"
DWORD StartApplication(const std::string exePath){
    STARTUPINFO si;
    PROCESS_INFORMATION pi;

    //memset memory
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si); 
    ZeroMemory(&pi, sizeof(pi));
    
    std::string path = "C:\\Windows\\System32\\" + exePath;
    LPCSTR appPath = (path).c_str();

    BOOL ok = CreateProcessA(
        appPath,    // 1. lpApplicationName: đường dẫn EXE
        NULL,       // 2. lpCommandLine: chuỗi lệnh (NULL nếu không có tham số)
        NULL,       // 3. lpProcessAttributes: security process
        NULL,       // 4. lpThreadAttributes: security thread
        FALSE,      // 5. bInheritHandles: kế thừa handle từ cha?
        0,          // 6. dwCreationFlags: flags (ví dụ ẩn cửa sổ...)
        NULL,       // 7. lpEnvironment: biến môi trường
        NULL,       // 8. lpCurrentDirectory: thư mục làm việc
        &si,        // 9. lpStartupInfo: thông tin cách chạy
        &pi         // 10. lpProcessInformation: thông tin trả về
    );

    if(!ok){
        std::cout << "Tien trinh Start bi loi: " << GetLastError() << '\n';
        return 0;
    }
    
    DWORD pid = pi.dwProcessId;

    // Giải phóng handle
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return pid;
}
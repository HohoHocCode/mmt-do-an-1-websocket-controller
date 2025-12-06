#include <windows.h>
#include <vector>
bool KillProcessByPID(DWORD pid);
std::vector<DWORD> GetPIDsByName(const std::wstring& processName);
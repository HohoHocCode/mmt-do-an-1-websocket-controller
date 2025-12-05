# Remote Desktop Control System v2.0

A cross-platform, abstracted remote desktop control system for managing processes across different operating systems in a LAN network.

## 🎯 Features

### Core Functionality
- **Process Management**: List, start, and terminate processes remotely
- **System Monitoring**: Real-time system information (CPU, memory, OS details)
- **File Operations**: Browse directories, read/write files
- **Cross-Platform**: Seamless operation on Windows, Linux, and macOS
- **Protocol Flexibility**: Support for both TCP and UDP with automatic fallback
- **Platform Abstraction**: Server-side OS is completely abstracted from client

### Architecture Highlights
- **Clean Separation**: Divided into multiple modules for easy maintenance
- **Platform Abstraction Layer**: Unified interface for all OS-specific operations
- **Network Abstraction**: Protocol-agnostic communication layer
- **Command Handler**: Extensible command processing system
- **Error Handling**: Robust error detection and recovery

## 📁 Project Structure

```
remote-control/
├── Platform.h              # Platform abstraction interface
├── Platform.cpp            # Platform-specific implementations
├── Network.h               # Network abstraction interface
├── Network.cpp             # TCP/UDP implementations
├── CommandHandler.h        # Command processing interface
├── CommandHandler.cpp      # Command implementations
├── Server.cpp              # Server application
├── Client.cpp              # Client application
├── Makefile               # Build configuration
└── README.md              # This file
```

## 🛠️ Building the Project

### Prerequisites
- C++17 compatible compiler (GCC 7+, Clang 5+, MSVC 2017+)
- Make utility
- Platform-specific libraries:
  - **Windows**: Winsock2, PSAPI (included with Windows SDK)
  - **Linux**: POSIX threads
  - **macOS**: POSIX threads

### Compilation

```bash
# Build everything
make

# Build only server
make server

# Build only client
make client

# Clean build artifacts
make clean

# Rebuild from scratch
make rebuild
```

### Platform-Specific Notes

**Windows:**
```cmd
# Using MinGW or MSYS2
make

# Or compile directly with cl.exe
cl /EHsc /std:c++17 Platform.cpp Network.cpp CommandHandler.cpp Server.cpp /link ws2_32.lib psapi.lib
```

**Linux/macOS:**
```bash
make
# Binaries will be in bin/ directory
```

## 🚀 Usage

### Starting the Server

```bash
# Default (TCP on port 5555)
./bin/server

# Specify port
./bin/server --port 8080

# Use UDP protocol
./bin/server --udp

# Show help
./bin/server --help
```

### Starting the Client

```bash
# Connect to server
./bin/client 192.168.1.100

# Specify port
./bin/client --port 8080 192.168.1.100

# Use UDP protocol
./bin/client --udp 192.168.1.100

# Show help
./bin/client --help
```

## 📋 Available Commands

### Process Management
- `list` or `ps` - List all running processes
- `start <command>` - Start a new process
  - Example: `start notepad.exe` (Windows)
  - Example: `start /usr/bin/gedit` (Linux)
- `kill <pid>` - Terminate a process by PID
- `info <pid>` - Get detailed information about a process

### System Information
- `sysinfo` - Display complete system information
  - OS name and architecture
  - Hostname
  - CPU cores
  - Memory usage

### File Operations
- `ls <path>` or `dir <path>` - List directory contents
  - Example: `ls /home/user`
  - Example: `dir C:\Users`
- `read <filepath>` - Read and display file contents
- `write <filepath> <content>` - Write content to a file

### Utility Commands
- `help` or `?` - Show all available commands
- `exec <command>` - Execute a script or command
- `exit` or `quit` - Disconnect from server

### Client-Only Commands
- `clear` or `cls` - Clear the terminal screen
- `localinfo` - Display local client system information

## 🔧 Architecture Details

### Platform Abstraction Layer

The `Platform.h/cpp` files provide a unified interface for OS-specific operations:

```cpp
class IPlatform {
    virtual std::vector<ProcessInfo> listProcesses() = 0;
    virtual bool startProcess(const std::string& command, unsigned long& outPid) = 0;
    virtual bool killProcess(unsigned long pid) = 0;
    virtual SystemInfo getSystemInfo() = 0;
    virtual std::vector<std::string> listDirectory(const std::string& path) = 0;
    // ... more methods
};
```

Implementations:
- `WindowsPlatform` - Uses Windows API
- `UnixPlatform` - Uses POSIX API (Linux/macOS)

### Network Abstraction Layer

The `Network.h/cpp` files provide protocol-agnostic communication:

```cpp
class IConnection {
    virtual bool connect(const std::string& host, int port) = 0;
    virtual bool send(const Message& msg) = 0;
    virtual Message receive() = 0;
};

class IServer {
    virtual bool start(int port) = 0;
    virtual bool waitForClient() = 0;
    virtual bool send(const Message& msg) = 0;
    virtual Message receive() = 0;
};
```

### Message Protocol

Messages are structured with type, command, and payload:

```cpp
struct Message {
    MessageType type;      // COMMAND, RESPONSE, HEARTBEAT, ERROR
    std::string command;   // Command name
    std::string payload;   // Command arguments or response data
    size_t payloadSize;    // Size of payload
};
```

### Command Handler

Extensible command processing system:

```cpp
CommandHandler handler;

// Register custom command
handler.registerCommand("mycmd", [](const std::string& args) {
    return "Custom command executed: " + args;
});

// Execute command
std::string result = handler.execute("mycmd test");
```

## 🔐 Security Considerations

**Important:** This is a development/learning tool. For production use, implement:

1. **Authentication**: Add user authentication system
2. **Encryption**: Use TLS/SSL for network communication
3. **Authorization**: Implement role-based access control
4. **Input Validation**: Strengthen command validation
5. **Audit Logging**: Log all operations for security monitoring
6. **Rate Limiting**: Prevent command flooding
7. **Sandboxing**: Restrict file system access

## 🎨 Extending the System

### Adding New Commands

Edit `CommandHandler.cpp`:

```cpp
CommandHandler::CommandHandler() {
    // Add your custom command
    registerCommand("custom", [this](const std::string& args) {
        return "Custom implementation: " + args;
    });
}
```

### Adding Platform-Specific Features

Edit `Platform.cpp` and implement in the appropriate platform class:

```cpp
std::string WindowsPlatform::customFeature() {
    // Windows-specific implementation
}

std::string UnixPlatform::customFeature() {
    // Unix-specific implementation
}
```

### Supporting New Protocols

Extend `Network.h/cpp` with new protocol implementations:

```cpp
class WebSocketConnection : public IConnection {
    // WebSocket implementation
};
```

## 📊 Example Session

```bash
# Start server on Windows machine
C:\> server.exe
✓ Server started successfully!
Protocol: TCP
Port:     5555
Waiting for client connections...

# Connect from Linux client
$ ./client 192.168.1.100
✓ Connected successfully!
Server: Connected to WIN-PC (Windows)

remote> sysinfo
=== System Information ===
OS:             Windows
Architecture:   x64
Hostname:       WIN-PC
CPU Cores:      8
Total Memory:   16384 MB
Available Mem:  8192 MB

remote> list
=== Running Processes (247) ===
PID        Name              Memory (KB)
-------------------------------------------------------
4          System            1024
524        svchost.exe       12340
1248       explorer.exe      45678
...

remote> start notepad.exe
Process started successfully
PID: 5432
Command: notepad.exe

remote> exit
Connection closed. Goodbye!
```

## 🐛 Troubleshooting

### Connection Issues
- Ensure firewall allows the specified port
- Verify both machines are on the same network
- Check if the port is already in use
- Try alternative protocol (TCP/UDP)

### Permission Issues
- Run server with administrator/sudo privileges for full process access
- Some operations require elevated permissions

### Build Issues
- Ensure C++17 support is enabled
- Check that all dependencies are installed
- Verify platform-specific libraries are available

## 📝 License

This is educational software. Use responsibly and ensure compliance with local laws and regulations regarding remote access software.

## 🤝 Contributing

Contributions are welcome! Areas for improvement:
- Enhanced security features
- Additional platform support
- Screen capture functionality
- File transfer improvements
- GUI interface
- Web-based client

## 📧 Support

For issues and questions, please refer to the troubleshooting section or review the code documentation in the header files.
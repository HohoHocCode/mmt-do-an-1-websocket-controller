#include "C:\UNIVERSITY\ComputerNetwork\remote-desktop-control\server\include\core\Network.h"
#include "C:\UNIVERSITY\ComputerNetwork\remote-desktop-control\server\include\core\Platform.h"
#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <csignal>

std::unique_ptr<Network::IConnection> g_connection;

void signalHandler(int signum)
{
    std::cout << "\nInterrupt signal received. Disconnecting...\n";
    if (g_connection)
    {
        g_connection->disconnect();
    }
    exit(signum);
}

void printBanner()
{
    std::cout << "Remote Desktop Control Client v2.0\n "
              << "==================================\n Cross-Platform Remote Administration\n\n";
}

void printUsage(const char *programName)
{
    std::cout << "Usage: " << programName << " [options] <server_ip>\n"
              << "Options:\n"
              << "  -p, --port <port>     Specify port number (default: 5555)\n"
              << "  -t, --tcp             Use TCP protocol (default)\n"
              << "  -u, --udp             Use UDP protocol\n"
              << "  -h, --help            Show this help message\n\n"
              << "Example:\n"
              << "  " << programName << " 192.168.1.100\n"
              << "  " << programName << " -p 8080 -t 192.168.1.100\n\n";
}

void printCommands()
{
    std::cout << "\n=== Quick Command Reference ===\n"
              << "list/ps              - List all running processes\n"
              << "start <command>      - Start a new process\n"
              << "kill <pid>           - Terminate a process\n"
              << "info <pid>           - Get process details\n"
              << "sysinfo              - Display system information\n"
              << "ls/dir <path>        - List directory contents\n"
              << "read <file>          - Read file contents\n"
              << "help                 - Show all available commands\n"
              << "exit/quit            - Disconnect from server\n"
              << "================================\n\n";
}

bool sendCommand(Network::IConnection *conn, const std::string &command)
{
    if (command.empty())
        return true;

    // Parse command and arguments
    std::string cmd, args;
    auto space = command.find(' ');
    if (space == std::string::npos)
    {
        cmd = command;
    }
    else
    {
        cmd = command.substr(0, space);
        args = command.substr(space + 1);
    }

    Network::Message msg{
        Network::MessageType::COMMAND,
        cmd,
        args,
        args.size()};

    if (!conn->send(msg))
    {
        std::cerr << "Failed to send command\n";
        return false;
    }

    return true;
}

int main(int argc, char *argv[])
{
    // Parse command line arguments
    int port = 5555;
    Network::Protocol protocol = Network::Protocol::TCP;
    std::string serverIP;

    for (int i = 1; i < argc; i++)
    {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help")
        {
            printBanner();
            printUsage(argv[0]);
            return 0;
        }
        else if (arg == "-p" || arg == "--port")
        {
            if (i + 1 < argc)
            {
                port = std::stoi(argv[++i]);
            }
            else
            {
                std::cerr << "Error: Port number required\n";
                return 1;
            }
        }
        else if (arg == "-t" || arg == "--tcp")
        {
            protocol = Network::Protocol::TCP;
        }
        else if (arg == "-u" || arg == "--udp")
        {
            protocol = Network::Protocol::UDP;
        }
        else if (arg[0] != '-')
        {
            serverIP = arg;
        }
    }

    if (serverIP.empty())
    {
        std::cerr << "Error: Server IP address required\n\n";
        printUsage(argv[0]);
        return 1;
    }

    // Set up signal handlers
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    printBanner();

    // Get local platform information
    auto platform = Platform::createPlatform();
    auto sysInfo = platform->getSystemInfo();

    std::cout << "Client Information:\n"
              << "  OS:       " << sysInfo.osName << "\n"
              << "  Hostname: " << sysInfo.hostname << "\n\n";

    // Create connection
    g_connection = Network::createConnection(protocol);

    std::cout << "Connecting to " << serverIP << ":" << port
              << " using " << (protocol == Network::Protocol::TCP ? "TCP" : "UDP") << "...\n";

    if (!g_connection->connect(serverIP, port))
    {
        std::cerr << "Failed to connect to server\n";

        // Try alternative protocol
        std::cout << "Attempting to use alternative protocol...\n";
        protocol = (protocol == Network::Protocol::TCP) ? Network::Protocol::UDP : Network::Protocol::TCP;

        g_connection = Network::createConnection(protocol);
        if (!g_connection->connect(serverIP, port))
        {
            std::cerr << "Failed to connect with alternative protocol\n";
            return 1;
        }
    }

    std::cout << "✓ Connected successfully!\n";

    // Receive welcome message
    Network::Message welcome = g_connection->receive();
    if (welcome.type == Network::MessageType::RESPONSE)
    {
        std::cout << "\nServer: " << welcome.payload << "\n";
    }

    printCommands();

    // Main client loop
    while (g_connection->isConnected())
    {
        std::cout << "remote> ";
        std::string command;
        std::getline(std::cin, command);

        // Trim whitespace
        command.erase(0, command.find_first_not_of(" \t\n\r"));
        command.erase(command.find_last_not_of(" \t\n\r") + 1);

        if (command.empty())
            continue;

        // Local commands
        if (command == "clear" || command == "cls")
        {
#ifdef PLATFORM_WINDOWS
            system("cls");
#else
            system("clear");
#endif
            continue;
        }

        if (command == "localinfo")
        {
            std::cout << sysInfo.toString() << "\n";
            continue;
        }

        // Send command to server
        if (!sendCommand(g_connection.get(), command))
        {
            std::cerr << "Connection lost\n";
            break;
        }

        // Handle exit/quit locally
        if (command == "exit" || command == "quit")
        {
            Network::Message response = g_connection->receive();
            if (response.type == Network::MessageType::RESPONSE)
            {
                std::cout << "\n"
                          << response.payload << "\n";
            }
            break;
        }

        // Receive response
        Network::Message response = g_connection->receive();

        if (response.type == Network::MessageType::ERR)
        {
            std::cerr << "Error: Connection lost or server error\n";
            break;
        }

        // Display response
        std::cout << "\n"
                  << response.payload << "\n\n";

        // Small delay to prevent flooding
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    g_connection->disconnect();
    std::cout << "Disconnected from server\n";

    return 0;
}
#include "C:\UNIVERSITY\ComputerNetwork\remote-desktop-control\server\include\core\Network.h"
#include "C:\UNIVERSITY\ComputerNetwork\remote-desktop-control\server\include\core\CommandHandler.h"
#include "C:\UNIVERSITY\ComputerNetwork\remote-desktop-control\server\include\core\Platform.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <csignal>

// Global server instance for signal handling
std::unique_ptr<Network::IServer> g_server;

void signalHandler(int signum)
{
    std::cout << "\nInterrupt signal (" << signum << ") received. Shutting down...\n";
    if (g_server)
    {
        g_server->stop();
    }
    exit(signum);
}

void printBanner()
{
    std::cout << "Remote Desktop Control Server v2.0\n "
              << "==================================\n Cross-Platform Remote Administration\n\n";
}

void printUsage(const char *programName)
{
    std::cout << "Usage: " << programName << " [options]\n"
              << "Options:\n"
              << "  -p, --port <port>     Specify port number (default: 5555)\n"
              << "  -t, --tcp             Use TCP protocol (default)\n"
              << "  -u, --udp             Use UDP protocol\n"
              << "  -h, --help            Show this help message\n\n";
}

int main(int argc, char *argv[])
{
    // Parse command line arguments
    int port = 5555;
    Network::Protocol protocol = Network::Protocol::TCP;

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
    }

    // Set up signal handlers
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    printBanner();

    // Get platform information
    auto platform = Platform::createPlatform();
    auto sysInfo = platform->getSystemInfo();

    std::cout << "System Information:\n"
              << "  OS:           " << sysInfo.osName << "\n"
              << "  Architecture: " << sysInfo.architecture << "\n"
              << "  Hostname:     " << sysInfo.hostname << "\n"
              << "  CPU Cores:    " << sysInfo.cpuCores << "\n"
              << "  Memory:       " << sysInfo.availableMemory << "/"
              << sysInfo.totalMemory << " MB\n\n";

    // Create server
    g_server = Network::createServer(protocol);

    std::cout << "Starting server on port " << port << " using "
              << (protocol == Network::Protocol::TCP ? "TCP" : "UDP") << "...\n";

    if (!g_server->start(port))
    {
        std::cerr << "Failed to start server on port " << port << "\n";

        // Try alternative protocol
        std::cout << "Attempting to use alternative protocol...\n";
        protocol = (protocol == Network::Protocol::TCP) ? Network::Protocol::UDP : Network::Protocol::TCP;

        g_server = Network::createServer(protocol);
        if (!g_server->start(port))
        {
            std::cerr << "Failed to start server with alternative protocol\n";
            return 1;
        }
    }

    std::cout << "Server started successfully!\n";
    std::cout << "Protocol: " << (protocol == Network::Protocol::TCP ? "TCP" : "UDP") << "\n";
    std::cout << "Port:     " << port << "\n";
    std::cout << "Waiting for client connections...\n\n";

    // Create command handler
    CommandHandler handler;

    // Main server loop
    while (g_server->isRunning())
    {
        std::cout << "Waiting for client...\n";

        if (!g_server->waitForClient())
        {
            std::cerr << "Failed to accept client connection\n";
            std::this_thread::sleep_for(std::chrono::seconds(1));
            continue;
        }

        std::cout << "✓ Client connected: " << g_server->getClientInfo() << "\n";

        // Send welcome message
        Network::Message welcome{
            Network::MessageType::RESPONSE,
            "welcome",
            "Connected to " + sysInfo.hostname + " (" + sysInfo.osName + ")",
            0};
        g_server->send(welcome);

        // Handle client commands
        bool clientConnected = true;
        while (clientConnected && g_server->isRunning())
        {
            Network::Message request = g_server->receive();

            if (request.type == Network::MessageType::ERR)
            {
                std::cout << "Client disconnected or error occurred\n";
                clientConnected = false;
                break;
            }

            if (request.type == Network::MessageType::HEARTBEAT)
            {
                Network::Message heartbeatResponse{
                    Network::MessageType::HEARTBEAT,
                    "pong",
                    "alive",
                    0};
                g_server->send(heartbeatResponse);
                continue;
            }

            if (request.command == "exit" || request.command == "quit")
            {
                std::cout << "Client requested disconnect\n";
                Network::Message goodbye{
                    Network::MessageType::RESPONSE,
                    "exit",
                    "Connection closed. Goodbye!",
                    0};
                g_server->send(goodbye);
                clientConnected = false;
                break;
            }

            std::cout << "Received command: " << request.command << "\n";

            // Execute command
            std::string result = handler.execute(request.command + " " + request.payload);

            // Send response
            Network::Message response{
                Network::MessageType::RESPONSE,
                request.command,
                result,
                result.size()};

            if (!g_server->send(response))
            {
                std::cerr << "Failed to send response\n";
                clientConnected = false;
                break;
            }

            std::cout << "Response sent (" << result.size() << " bytes)\n";
        }

        std::cout << "Client session ended\n\n";
    }

    std::cout << "Server shutdown complete\n";
    return 0;
}
#include "C:\UNIVERSITY\ComputerNetwork\remote-desktop-control\server\include\network\web_server.h"
#include "C:\UNIVERSITY\ComputerNetwork\remote-desktop-control\server\include\network\WebSocketServer.h"
#include <csignal>
#include <memory>

std::unique_ptr<WebServer> g_webServer;

void signalHandler(int signum)
{
    std::cout << "\nInterrupt signal (" << signum << ") received. Shutting down web server...\n";
    if (g_webServer)
    {
        g_webServer->stop();
    }
    exit(signum);
}

int main()
{
    // Set up signal handlers for clean exit
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    // Create and start the web server (defaults to port 8080)
    g_webServer = std::make_unique<WebServer>(8080);

    if (!g_webServer->start())
    {
        std::cerr << "Failed to start web server on port 8080\n";
        return 1;
    }

    // Run the server loop
    g_webServer->run();

    std::cout << "Web server shut down\n";
    return 0;
}
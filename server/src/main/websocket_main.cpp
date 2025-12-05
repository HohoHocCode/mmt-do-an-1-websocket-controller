#include "C:\UNIVERSITY\ComputerNetwork\remote-desktop-control\server\include\network\WebSocketServer.h"
#include <csignal>
#include <memory>
#include <iostream>

std::unique_ptr<WebSocketServer> g_ws;

void signalHandler(int signum)
{
    std::cout << "\nInterrupt signal (" << signum << ") received. Shutting down WebSocket server...\n";
    if (g_ws)
    {
        g_ws->stop();
    }
    exit(signum);
}

int main()
{
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    g_ws = std::make_unique<WebSocketServer>(8081);

    if (!g_ws->start())
    {
        std::cerr << "Failed to start WebSocket server on port 8081\n";
        return 1;
    }

    std::cout << "WebSocket server running on ws://localhost:8081" << std::endl;
    g_ws->run();

    std::cout << "WebSocket server shutdown" << std::endl;
    return 0;
}
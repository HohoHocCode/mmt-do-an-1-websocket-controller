#include <iostream>
#include "network/ws_server.hpp"

int main() {
    std::cout << "[Server] Starting (stub)..." << std::endl;

    WsServer server;
    server.run("0.0.0.0", 9002);

    std::cout << "[Server] Exiting (stub)." << std::endl;
    return 0;
}

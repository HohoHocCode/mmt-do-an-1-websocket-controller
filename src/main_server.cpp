#include <iostream>
#include "server/ServerCore.hpp"

int main() {
    std::cout << "[Server] Starting...\n";
    ServerCore server;
    server.start();
    return 0;
}

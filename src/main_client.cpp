#include "client/ClientCore.hpp"
#include <iostream>

int main() {
    std::cout << "[Controller] Starting...\n";

    // Controller kết nối tới Agent tại localhost:9002
    ClientCore client("localhost", 9002);

    client.start();

    return 0;
}

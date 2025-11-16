#pragma once
#include <memory>
#include "modules/network/WebSocketServer.hpp"

// AgentCore – chạy trên máy bị điều khiển (sinh viên)
// -> Lắng nghe WebSocket
// -> Nhận command từ Controller
// -> Xin phép người dùng và thực thi

class ServerCore {
public:
    ServerCore();
    void start();
    void stop();

private:
    std::unique_ptr<WebSocketServer> wsServer;
};

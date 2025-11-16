#pragma once

#include <string>

// ControllerCore – chạy trên máy điều khiển (giảng viên)
// -> Kết nối tới Agent qua WebSocket Client
// -> Gửi command (screenshot, processes…)
// -> Nhận phản hồi từ Agent

class ClientCore {
public:
    ClientCore(const std::string& host, int port);
    void start();

private:
    std::string host;
    int port;
};

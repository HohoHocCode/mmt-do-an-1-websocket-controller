#pragma once
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>
#include <functional>
#include <string>
#include <vector>

class WebSocketServer {
public:
    using MessageHandler =
    std::function<void(const std::string& msg, websocketpp::connection_hdl)>;

    WebSocketServer(int port);
    void start();
    void stop();

    void setMessageHandler(MessageHandler handler);

    void sendText(const std::string& msg);
    void sendBinary(const std::vector<uint8_t>& data);

private:
    websocketpp::server<websocketpp::config::asio> server;
    websocketpp::connection_hdl currentClient;
    int port;
    MessageHandler msgHandler;
};

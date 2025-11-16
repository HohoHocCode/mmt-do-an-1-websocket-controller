#include "modules/network/WebSocketServer.hpp"
#include <spdlog/spdlog.h>

WebSocketServer::WebSocketServer(int port)
    : port(port)
{
    server.clear_access_channels(websocketpp::log::alevel::all);
    server.init_asio();

    // Khi có client gửi message
    server.set_message_handler([this](websocketpp::connection_hdl hdl, auto msg) {
    currentClient = hdl;
    std::string payload = msg->get_payload();

    if (msgHandler) {
        msgHandler(payload, hdl); // gửi đúng 2 đối số
    }
});


    server.set_open_handler([](websocketpp::connection_hdl){
        spdlog::info("[WS] Client connected ✔");
    });

    server.set_close_handler([](websocketpp::connection_hdl){
        spdlog::info("[WS] Client disconnected");
    });
}

void WebSocketServer::setMessageHandler(MessageHandler handler) {
    msgHandler = handler;
}

void WebSocketServer::start() {
    spdlog::info("[WS] Server starting at ws://localhost:{}", port);

    websocketpp::lib::error_code ec;
    server.listen(port, ec);

    if (ec) {
        spdlog::error("[WS] Listen failed: {}", ec.message());
        return;
    }

    server.start_accept();
    server.run();
}

void WebSocketServer::stop() {
    server.stop_listening();
}

void WebSocketServer::sendText(const std::string& msg) {
    if (!currentClient.lock()) return;
    server.send(currentClient, msg, websocketpp::frame::opcode::text);
}


void WebSocketServer::sendBinary(const std::vector<uint8_t>& data) {
    if (!currentClient.lock()) return;
    server.send(currentClient, data.data(), data.size(), websocketpp::frame::opcode::binary);
}

#include "client/ClientCore.hpp"
#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>
#include <spdlog/spdlog.h>
#include <thread>
#include <iostream>
#include <nlohmann/json.hpp>
#include <fstream>

using json = nlohmann::json;
typedef websocketpp::client<websocketpp::config::asio_client> ws_client;

static websocketpp::connection_hdl global_hdl;
static ws_client* global_client = nullptr;

ClientCore::ClientCore(const std::string& host, int port)
    : host(host), port(port) {}

void ClientCore::start() {
    spdlog::info("[Controller] Starting...");

    ws_client client;
    global_client = &client;

    client.init_asio();

    // --- Khi kết nối thành công ---
    client.set_open_handler([&](websocketpp::connection_hdl hdl) {
        spdlog::info("[Controller] Connected to Agent ✔");

        global_hdl = hdl;

        // THREAD để nhập lệnh
        std::thread inputThread([]() {
            std::string cmd;

            while (true) {
                std::getline(std::cin, cmd);

                // Lệnh thoát
                if (cmd == "exit") {
                    spdlog::info("[Controller] Exit -> closing");
                    exit(0);
                }

                // Lệnh screenshot
                if (cmd == "screenshot") {
                    json j = {
                        {"cmd", "screenshot"}
                    };

                    global_client->send(
                        global_hdl,
                        j.dump(),
                        websocketpp::frame::opcode::text
                    );

                    spdlog::info("[Controller] Sent screenshot command");
                    continue;
                }

                // Nếu là lệnh khác → gửi raw text như cũ
                global_client->send(
                    global_hdl,
                    cmd,
                    websocketpp::frame::opcode::text
                );
            }
        });

        inputThread.detach();
    });

    // --- Nhận tin nhắn từ Agent ---
    client.set_message_handler([&](websocketpp::connection_hdl, ws_client::message_ptr msg) {

    if (msg->get_opcode() == websocketpp::frame::opcode::binary) {
        auto data = msg->get_raw_payload();

        std::ofstream ofs("received_screenshot.jpg", std::ios::binary);
        ofs.write(data.data(), data.size());
        ofs.close();

        spdlog::info("[Controller] Screenshot received ({} bytes) -> saved to received_screenshot.jpg", data.size());
        return;
    }

    // text message
    spdlog::info("[Controller] From Agent: {}", msg->get_payload());
});



    // --- kết nối ---
    websocketpp::lib::error_code ec;
    std::string uri = "ws://" + host + ":" + std::to_string(port);

    auto con = client.get_connection(uri, ec);
    if (ec) {
        spdlog::error("[Controller] Connect error: {}", ec.message());
        return;
    }

    client.connect(con);
    client.run();
}

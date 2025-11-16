#include "server/ServerCore.hpp"
#include <spdlog/spdlog.h>
#include "modules/screen/ScreenCapturer.hpp"
#include <fstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

ServerCore::ServerCore() {
    wsServer = std::make_unique<WebSocketServer>(9002);

    // Handler đúng (2 tham số)
    wsServer->setMessageHandler(
        [this](const std::string& msg, websocketpp::connection_hdl hdl) {

            spdlog::info("[Agent] Command received: {}", msg);

            json j = json::parse(msg);

            if (j["cmd"] == "screenshot") {

                ScreenCapturer cap;
                cv::Mat img = cap.captureScreen();   // <-- thay captureDummy()
                auto jpeg = cap.encodeJpeg(img);

                wsServer->sendText(R"({"status":"ok","type":"screenshot"})");
                wsServer->sendBinary(jpeg);

                spdlog::info("[Agent] Screenshot sent ({} bytes)", jpeg.size());
            }
        }
    ); 
}


void ServerCore::start() {
    spdlog::info("[Agent] Starting ServerCore...");
    wsServer->start();
}


void ServerCore::stop() {
    spdlog::info("[Agent] Stopping WebSocket...");
    wsServer->stop();
}

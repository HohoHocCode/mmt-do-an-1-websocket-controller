#include "api/discovery.hpp"
#include "api/logger.hpp"

#include <boost/asio.hpp>

#include <array>
#include <chrono>
#include <random>
#include <thread>

namespace asio = boost::asio;
using udp = asio::ip::udp;

DiscoveryService::DiscoveryService(asio::io_context& ioc) : ioc_(ioc) {}

Json DiscoveryService::scan(unsigned int timeout_ms, unsigned short port, const std::string& nonce) {
    udp::socket socket(ioc_);
    boost::system::error_code ec;
    socket.open(udp::v4(), ec);
    if (ec) {
        Logger::instance().error("Discovery open failed: " + ec.message());
        return {{"ok", false}, {"error", "socket_open_failed"}};
    }

    socket.set_option(asio::socket_base::reuse_address(true), ec);
    socket.set_option(asio::socket_base::broadcast(true), ec);
    socket.non_blocking(true, ec);

    const std::string message = "MMT_DISCOVER " + nonce;
    const udp::endpoint broadcast_ep(asio::ip::address_v4::broadcast(), port);
    socket.send_to(asio::buffer(message), broadcast_ep, 0, ec);
    if (ec) {
        Logger::instance().warn("Discovery send failed: " + ec.message());
    }

    std::vector<Json> devices;
    std::array<char, 2048> buffer{};
    auto start = std::chrono::steady_clock::now();
    while (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count() <
           timeout_ms) {
        udp::endpoint sender;
        const auto result = socket.receive_from(asio::buffer(buffer), sender, 0, ec);
        if (ec == asio::error::would_block || ec == asio::error::try_again) {
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
            continue;
        }
        if (ec) {
            Logger::instance().warn("Discovery receive error: " + ec.message());
            break;
        }

        std::string payload(buffer.data(), buffer.data() + result);
        try {
            Json j = Json::parse(payload);
            if (!nonce.empty() && j.contains("nonce") && j["nonce"] != nonce) {
                continue;
            }
            j["received_from"] = sender.address().to_string();
            devices.push_back(j);
        } catch (const std::exception& e) {
            Logger::instance().warn(std::string("Discovery parse error: ") + e.what());
        }
    }

    return {
        {"ok", true},
        {"devices", devices},
        {"nonce", nonce},
        {"count", static_cast<int>(devices.size())}
    };
}

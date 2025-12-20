#include "doctest/doctest.h"
#include "network/ws_client.hpp"
#include "network/ws_server.hpp"
#include "utils/json.hpp"

#include <boost/asio.hpp>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <mutex>
#include <thread>
#include <vector>

namespace {
unsigned short find_free_port() {
    boost::asio::io_context ioc;
    tcp::acceptor acceptor(ioc, tcp::endpoint(tcp::v4(), 0));
    return acceptor.local_endpoint().port();
}

void set_env_flag(const char* key, const char* value) {
#ifdef _WIN32
    _putenv_s(key, value);
#else
    setenv(key, value, 1);
#endif
}

template <typename Predicate>
bool wait_for(Predicate&& predicate, std::chrono::milliseconds timeout) {
    const auto start = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - start < timeout) {
        if (predicate()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return false;
}

bool wait_for_response(const std::vector<Json>& responses,
                       const std::string& request_id,
                       Json& out) {
    for (const auto& resp : responses) {
        if (resp.contains("requestId") && resp["requestId"] == request_id) {
            out = resp;
            return true;
        }
    }
    return false;
}
} // namespace

TEST_CASE("websocket smoke test connects and handles commands") {
    set_env_flag("DISCOVERY_ENABLED", "0");

    unsigned short port = find_free_port();
    WsServer server;
    std::thread server_thread([&]() {
        server.run("127.0.0.1", port);
    });

    std::mutex mutex;
    std::condition_variable cv;
    std::vector<Json> responses;
    std::string error;

    WsClient client;
    client.set_message_handler([&](const std::string& msg) {
        JsonParseResult parsed = parse_json_safe(msg);
        if (!parsed.ok) {
            return;
        }
        {
            std::lock_guard<std::mutex> lock(mutex);
            responses.push_back(std::move(parsed.value));
        }
        cv.notify_all();
    });
    client.set_error_handler([&](const std::string& err) {
        {
            std::lock_guard<std::mutex> lock(mutex);
            error = err;
        }
        cv.notify_all();
    });

    client.connect("127.0.0.1", std::to_string(port), "/");

    CHECK(wait_for([&]() { return client.is_connected(); }, std::chrono::milliseconds(2000)));

    {
        Json ping_req;
        ping_req["cmd"] = "ping";
        ping_req["requestId"] = "smoke-1";
        client.send(ping_req.dump());
    }

    Json ping_resp;
    {
        std::unique_lock<std::mutex> lock(mutex);
        bool got_ping = cv.wait_for(lock, std::chrono::seconds(2), [&]() {
            return wait_for_response(responses, "smoke-1", ping_resp) || !error.empty();
        });
        CHECK(got_ping);
    }

    CHECK(ping_resp["status"] == "ok");
    CHECK(ping_resp["cmd"] == "ping");

    {
        Json unknown_req;
        unknown_req["cmd"] = "unknown_cmd";
        unknown_req["requestId"] = "smoke-2";
        client.send(unknown_req.dump());
    }

    Json unknown_resp;
    {
        std::unique_lock<std::mutex> lock(mutex);
        bool got_unknown = cv.wait_for(lock, std::chrono::seconds(2), [&]() {
            return wait_for_response(responses, "smoke-2", unknown_resp) || !error.empty();
        });
        CHECK(got_unknown);
    }

    CHECK(unknown_resp["status"] == "error");
    CHECK(unknown_resp["error"] == "unknown_command");

    client.close();
    server.stop();
    server_thread.join();
}

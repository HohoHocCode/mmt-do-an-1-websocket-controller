#include "network/ws_server.hpp"
#include "core/dispatcher.hpp"
#include "utils/json.hpp"
#include "modules/screen.hpp"
#include "modules/system_control.hpp"

#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/websocket.hpp>

#include <iostream>
#include <memory>
#include <string>
#include <chrono>
#include <optional>
#include <cstdlib>
#include <thread>
#include <atomic>
#include <array>
#include <cctype>

namespace asio  = boost::asio;
namespace beast = boost::beast;
namespace http  = beast::http;
namespace ws    = beast::websocket;
using tcp       = asio::ip::tcp;
using udp       = asio::ip::udp;

struct ParsedUrl {
    std::string host = "localhost";
    std::string port = "80";
    std::string base_path;
};

struct VerifiedUser {
    std::string username;
    std::string role;
};

static std::string env_string(const char* key, const std::string& fallback) {
    const char* val = std::getenv(key);
    if (val && *val) return std::string(val);
    return fallback;
}

static unsigned short env_port(const char* key, unsigned short fallback) {
    const char* val = std::getenv(key);
    if (!val || !*val) return fallback;
    try {
        int parsed = std::stoi(val);
        if (parsed > 0 && parsed < 65536) return static_cast<unsigned short>(parsed);
    } catch (...) {
    }
    return fallback;
}

static bool env_flag(const char* key, bool fallback) {
    const char* val = std::getenv(key);
    if (!val) return fallback;
    std::string s(val);
    for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (s == "1" || s == "true" || s == "yes" || s == "on") return true;
    if (s == "0" || s == "false" || s == "no" || s == "off") return false;
    return fallback;
}

static ParsedUrl parse_base_url(const std::string& url) {
    ParsedUrl result;
    std::string working = url;
    const std::string prefix = "http://";
    if (working.rfind(prefix, 0) == 0) {
        working = working.substr(prefix.size());
    }

    auto slash_pos = working.find('/');
    std::string host_port = slash_pos == std::string::npos ? working : working.substr(0, slash_pos);
    std::string path = slash_pos == std::string::npos ? "" : working.substr(slash_pos);

    auto colon_pos = host_port.find(':');
    if (colon_pos != std::string::npos) {
        result.host = host_port.substr(0, colon_pos);
        result.port = host_port.substr(colon_pos + 1);
    } else {
        result.host = host_port;
        result.port = "80";
    }

    if (!path.empty() && path.front() != '/') path = "/" + path;
    result.base_path = path;
    return result;
}

static std::optional<VerifiedUser> verify_with_auth_service(const std::string& base_url, const std::string& token) {
    try {
        auto parsed = parse_base_url(base_url);
        asio::io_context ioc;
        tcp::resolver resolver(ioc);
        auto results = resolver.resolve(parsed.host, parsed.port);
        beast::tcp_stream stream(ioc);
        stream.connect(results);

        std::string target = parsed.base_path + "/auth/verify";
        if (target.empty() || target.front() != '/') target = "/" + target;

        http::request<http::string_body> req{http::verb::post, target, 11};
        req.set(http::field::host, parsed.host);
        req.set(http::field::content_type, "application/json");
        req.body() = Json({{"token", token}}).dump();
        req.prepare_payload();

        http::write(stream, req);
        beast::flat_buffer buffer;
        http::response<http::string_body> res;
        http::read(stream, buffer, res);

        beast::error_code ec;
        stream.socket().shutdown(tcp::socket::shutdown_both, ec);

        if (res.result() != http::status::ok) return std::nullopt;
        Json body = Json::parse(res.body());
        if (body.contains("ok") && body["ok"] == true && body.contains("user")) {
            auto user = body["user"];
            if (user.contains("username") && user.contains("role")) {
                return VerifiedUser{user["username"], user["role"]};
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "[Auth] verify error: " << e.what() << "\n";
    }
    return std::nullopt;
}

static void send_audit_remote(const std::string& base_url, const std::string& token, const std::string& action, const Json& meta) {
    try {
        auto parsed = parse_base_url(base_url);
        asio::io_context ioc;
        tcp::resolver resolver(ioc);
        auto results = resolver.resolve(parsed.host, parsed.port);
        beast::tcp_stream stream(ioc);
        stream.connect(results);

        std::string target = parsed.base_path + "/audit";
        if (target.empty() || target.front() != '/') target = "/" + target;

        Json payload;
        payload["action"] = action;
        payload["meta"] = meta;

        http::request<http::string_body> req{http::verb::post, target, 11};
        req.set(http::field::host, parsed.host);
        req.set(http::field::content_type, "application/json");
        req.set(http::field::authorization, "Bearer " + token);
        req.body() = payload.dump();
        req.prepare_payload();

        http::write(stream, req);
        beast::flat_buffer buffer;
        http::response<http::string_body> res;
        http::read(stream, buffer, res);

        beast::error_code ec;
        stream.socket().shutdown(tcp::socket::shutdown_both, ec);
        (void)res;
    } catch (const std::exception& e) {
        std::cerr << "[Audit] send failed: " << e.what() << "\n";
    }
}

// ============================================================================
// DiscoveryResponder (UDP)
// ============================================================================
class DiscoveryResponder {
public:
    DiscoveryResponder(unsigned short listen_port, unsigned short ws_port)
        : socket_(io_)
        , listen_port_(listen_port)
        , ws_port_(ws_port)
        , name_(env_string("AGENT_NAME", "mmt-controller"))
        , version_(env_string("AGENT_VERSION", "dev"))
    {}

    ~DiscoveryResponder() {
        stop();
    }

    bool start() {
        if (running_) return true;

        boost::system::error_code ec;
        udp::endpoint ep(udp::v4(), listen_port_);
        socket_.open(ep.protocol(), ec);
        if (ec) {
            std::cerr << "[Discovery] open failed: " << ec.message() << "\n";
            return false;
        }
        socket_.set_option(asio::socket_base::reuse_address(true), ec);
        socket_.set_option(asio::socket_base::broadcast(true), ec);
        socket_.bind(ep, ec);
        if (ec) {
            std::cerr << "[Discovery] bind failed: " << ec.message() << "\n";
            return false;
        }

        running_ = true;
        do_receive();
        worker_ = std::thread([this]() { io_.run(); });
        std::cout << "[Discovery] Listening for MMT_DISCOVER on UDP " << listen_port_ << "\n";
        return true;
    }

    void stop() {
        if (!running_) return;
        running_ = false;
        boost::system::error_code ec;
        socket_.close(ec);
        io_.stop();
        if (worker_.joinable()) worker_.join();
        io_.restart();
    }

private:
    asio::io_context io_;
    udp::socket socket_;
    udp::endpoint remote_endpoint_;
    std::array<char, 2048> buffer_{};
    std::thread worker_;
    unsigned short listen_port_;
    unsigned short ws_port_;
    std::string name_;
    std::string version_;
    std::atomic<bool> running_{false};

    void do_receive() {
        socket_.async_receive_from(
            asio::buffer(buffer_),
            remote_endpoint_,
            [this](boost::system::error_code ec, std::size_t bytes) {
                if (ec) {
                    if (running_) {
                        do_receive();
                    }
                    return;
                }
                handle_receive(bytes);
                if (running_) {
                    do_receive();
                }
            }
        );
    }

    void handle_receive(std::size_t bytes) {
        if (bytes == 0) return;
        const std::string text(buffer_.data(), buffer_.data() + bytes);
        if (text.rfind("MMT_DISCOVER", 0) != 0) return;

        std::string nonce;
        auto space_pos = text.find(' ');
        if (space_pos != std::string::npos && space_pos + 1 < text.size()) {
            nonce = text.substr(space_pos + 1);
            if (nonce.size() > 64) {
                nonce = nonce.substr(0, 64);
            }
        }

        Json payload;
        payload["type"] = "mmt_discover_response";
        payload["nonce"] = nonce;
        payload["wsPort"] = ws_port_;
        payload["name"] = name_;
        payload["version"] = version_;
        payload["ip"] = remote_endpoint_.address().to_string();
        payload["timestamp"] = static_cast<long long>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count()
        );

        auto message = std::make_shared<std::string>(payload.dump());
        socket_.async_send_to(
            asio::buffer(*message),
            remote_endpoint_,
            [message](boost::system::error_code /*ec*/, std::size_t /*bytes*/) {}
        );
    }
};

// ============================================================================
// WebSocketSession
// ============================================================================
class WebSocketSession : public std::enable_shared_from_this<WebSocketSession> {
public:
    explicit WebSocketSession(tcp::socket socket)
        : ws_(std::move(socket))
        , stream_timer_(ws_.get_executor())   // timer dùng chung executor với websocket
        , stream_guard_timer_(ws_.get_executor())
        , auth_api_base_(std::getenv("AUTH_API_URL") ? std::getenv("AUTH_API_URL") : "http://localhost:5179")
    {}

    void start() {
        ws_.set_option(ws::stream_base::timeout::suggested(beast::role_type::server));

        ws_.async_accept(
            beast::bind_front_handler(
                &WebSocketSession::on_accept,
                shared_from_this()
            )
        );
    }

private:
    ws::stream<tcp::socket> ws_;

    beast::flat_buffer buffer_;
    Dispatcher dispatcher_;
    std::optional<VerifiedUser> verified_user_;
    std::string auth_token_;
    std::string auth_api_base_;

    // Stream state
    asio::steady_timer stream_timer_;
    asio::steady_timer stream_guard_timer_;
    bool streaming_ = false;
    int stream_seq_ = 0;
    int stream_interval_ms_ = 0;
    int stream_total_frames_ = 0;
    std::uint64_t stream_generation_ = 0;


    // ------------------------------------------------------------------------
    void on_accept(beast::error_code ec) {
        if (ec) {
            std::cerr << "[WsServer] Accept error: " << ec.message() << "\n";
            return;
        }
        do_read();
    }

    // ------------------------------------------------------------------------
    void do_read() {
        ws_.async_read(
            buffer_,
            beast::bind_front_handler(
                &WebSocketSession::on_read,
                shared_from_this()
            )
        );
    }

    // ------------------------------------------------------------------------
    void on_read(beast::error_code ec, std::size_t) {
        if (ec == ws::error::closed)
            return;
        if (ec) {
            std::cerr << "[WsServer] Read error: " << ec.message() << "\n";
            return;
        }

        std::string req = beast::buffers_to_string(buffer_.data());
        buffer_.consume(buffer_.size());

        std::cout << "[WsServer] Received: " << req << "\n";

        // ---- Check for screen_stream command ----
        try {
            Json j = Json::parse(req);
            if (j.contains("cmd") && j["cmd"].is_string()) {
                std::string cmd = j["cmd"];

                if (cmd == "auth") {
                    Json resp;
                    resp["cmd"] = "auth";
                    if (!j.contains("token") || !j["token"].is_string()) {
                        resp["status"] = "error";
                        resp["message"] = "token required";
                    } else {
                        std::string incoming_token = j["token"];
                        auto verified = verify_with_auth_service(auth_api_base_, incoming_token);
                        if (verified) {
                            verified_user_ = verified;
                            auth_token_ = incoming_token;
                            resp["status"] = "ok";
                            resp["username"] = verified->username;
                            resp["role"] = verified->role;
                        } else {
                            verified_user_.reset();
                            auth_token_.clear();
                            resp["status"] = "error";
                            resp["message"] = "Invalid token";
                        }
                    }
                    send_text(resp.dump());
                    do_read();
                    return;
                }

                if (cmd == "restart" || cmd == "shutdown") {
                    Json resp;
                    resp["cmd"] = cmd;
                    if (!verified_user_ || verified_user_->role != "admin") {
                        resp["status"] = "error";
                        resp["message"] = "admin_token_required";
                        send_text(resp.dump());
                        do_read();
                        return;
                    }
                    SystemControl control;
                    bool ok = cmd == "shutdown" ? control.shutdown() : control.restart();
                    resp["status"] = ok ? "accepted" : "error";
                    if (!ok) resp["message"] = "not_supported";

                    if (ok && !auth_token_.empty()) {
                        Json meta;
                        meta["cmd"] = cmd;
                        send_audit_remote(auth_api_base_, auth_token_, cmd, meta);
                    }

                    send_text(resp.dump());
                    do_read();
                    return;
                }

                if (cmd == "cancel_all") {
                    stop_stream("cancel_all");
                    Json ack;
                    ack["cmd"] = "cancel_all";
                    ack["status"] = "ok";
                    send_text(ack.dump());
                    do_read();
                    return;
                }

                if (cmd == "reset") {
                    stop_stream("reset");
                    Json ack;
                    ack["cmd"] = "reset";
                    ack["status"] = "ok";
                    ack["message"] = "Session reset";
                    send_text(ack.dump());
                    do_read();
                    return;
                }

                if (cmd == "screen_stream") {
                    int duration = j.value("duration", 5);
                    int fps = j.value("fps", 5);

                    start_screen_stream(duration, fps);

                    Json ack;
                    ack["cmd"] = "screen_stream";
                    ack["status"] = "started";
                    ack["duration"] = duration;
                    ack["fps"] = fps;

                    send_text(ack.dump());

                    do_read();
                    return;
                }
            }
        }
        catch (...) {}

        // ---- Default: use dispatcher ----
        std::string resp = dispatcher_.handle(req);
        send_text(resp);

        do_read();
    }

    // ------------------------------------------------------------------------
    void send_text(const std::string& s) {
        auto msg = std::make_shared<std::string>(s);

        ws_.text(true);
        ws_.async_write(
            asio::buffer(*msg),
            [self = shared_from_this(), msg](beast::error_code ec, std::size_t) {
                if (ec) {
                    std::cerr << "[WsServer] Write error: " << ec.message() << "\n";
                }
            }
        );
    }

    // ------------------------------------------------------------------------
    void start_screen_stream(int duration, int fps) {
        if (streaming_) {
            std::cout << "[WsServer] Already streaming\n";
            return;
        }

        if (fps < 1) fps = 1;
        if (fps > 30) fps = 30;
        if (duration < 1) duration = 3;
        if (duration > 60) duration = 60;

        streaming_ = true;
        stream_seq_ = 0;
        stream_interval_ms_ = 1000 / fps;
        stream_total_frames_ = duration * fps;
        stream_generation_++;
        const auto generation = stream_generation_;

        stream_guard_timer_.expires_after(std::chrono::seconds(60));
        stream_guard_timer_.async_wait([self = shared_from_this(), generation](const beast::error_code& ec) {
            if (!ec && generation == self->stream_generation_) {
                self->stop_stream("timeout");
            }
        });

        std::cout << "[WsServer] Streaming start: "
                  << fps << " fps, "
                  << duration << " sec, total frames = "
                  << stream_total_frames_ << "\n";

        stream_timer_.expires_after(std::chrono::milliseconds(stream_interval_ms_));
        stream_timer_.async_wait(
            [self = shared_from_this(), generation](const beast::error_code& ec) {
                self->do_stream_frame(ec, generation);
            }
        );
    }

    // ------------------------------------------------------------------------
    void stop_stream(const std::string& reason) {
        if (!streaming_) return;
        streaming_ = false;
        stream_generation_++;
        beast::error_code cancel_ec;
        stream_timer_.cancel(cancel_ec);
        stream_guard_timer_.cancel(cancel_ec);
        std::cout << "[WsServer] Stream stopped (" << reason << ")\n";
    }

    // ------------------------------------------------------------------------
    void do_stream_frame(beast::error_code ec, std::uint64_t generation) {
        if (ec == asio::error::operation_aborted) return;
        if (!streaming_ || generation != stream_generation_) return;

        if (stream_seq_ >= stream_total_frames_) {
            std::cout << "[WsServer] Stream finished\n";
            stop_stream("complete");
            return;
        }

        std::string b64 = ScreenCapture::capture_base64();
        if (b64.empty()) {
            std::cerr << "[WsServer] ScreenCapture failed\n";
            stop_stream("capture_failed");
            return;
        }

        Json j;
        j["cmd"] = "screen_stream";
        j["seq"] = stream_seq_;
        j["image_base64"] = b64;

        auto msg = std::make_shared<std::string>(j.dump());

        ws_.text(true);
        ws_.async_write(
            asio::buffer(*msg),
            [self = shared_from_this(), msg](beast::error_code ec, std::size_t) {
                if (ec) {
                    std::cerr << "[WsServer] Stream write error: "
                              << ec.message() << "\n";
                    self->stop_stream("write_failed");
                    return;
                }

                if (!self->streaming_) return;

                self->stream_seq_++;

                self->stream_timer_.expires_after(
                    std::chrono::milliseconds(self->stream_interval_ms_)
                );

                self->stream_timer_.async_wait(
                    [self, generation](const beast::error_code& timer_ec) {
                        self->do_stream_frame(timer_ec, generation);
                    }
                );
            }
        );
    }
};



// ============================================================================
// Listener
// ============================================================================
class Listener : public std::enable_shared_from_this<Listener> {
public:
    Listener(asio::io_context& ioc, tcp::endpoint endpoint)
        : ioc_(ioc)
        , acceptor_(ioc)
    {
        beast::error_code ec;

        acceptor_.open(endpoint.protocol(), ec);
        acceptor_.set_option(asio::socket_base::reuse_address(true), ec);
        acceptor_.bind(endpoint, ec);
        acceptor_.listen(asio::socket_base::max_listen_connections, ec);
    }

    void run() {
        do_accept();
    }

private:
    asio::io_context& ioc_;
    tcp::acceptor acceptor_;

    void do_accept() {
        acceptor_.async_accept(
            asio::make_strand(ioc_),
            beast::bind_front_handler(
                &Listener::on_accept,
                shared_from_this()
            )
        );
    }

    void on_accept(beast::error_code ec, tcp::socket socket) {
        if (!ec) {
            std::make_shared<WebSocketSession>(std::move(socket))->start();
        }
        do_accept();
    }
};


// ============================================================================
// WsServer PIMPL
// ============================================================================
struct WsServer::Impl {
    asio::io_context ioc;
    std::unique_ptr<DiscoveryResponder> discovery;

    void start(const std::string& addr, unsigned short port) {
        const bool enable_discovery = env_flag("DISCOVERY_ENABLED", true);
        const unsigned short discovery_port = env_port("DISCOVERY_PORT", 41000);
        if (enable_discovery) {
            auto responder = std::make_unique<DiscoveryResponder>(discovery_port, port);
            if (responder->start()) {
                discovery = std::move(responder);
            } else {
                std::cerr << "[Discovery] Disabled (failed to bind port " << discovery_port << ")\n";
            }
        }

        tcp::endpoint ep(asio::ip::make_address(addr), port);
        std::make_shared<Listener>(ioc, ep)->run();
        std::cout << "[WsServer] Listening on " << addr << ":" << port << "\n";
        ioc.run();

        if (discovery) {
            discovery->stop();
        }
    }
};

WsServer::WsServer() : pimpl_(std::make_unique<Impl>()) {}
WsServer::~WsServer() = default;

void WsServer::run(const std::string& addr, unsigned short port) {
    pimpl_->start(addr, port);
}

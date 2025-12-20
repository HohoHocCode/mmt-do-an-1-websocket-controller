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
#include <algorithm>
#include <deque>
#include <functional>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

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

class WebSocketSession;

class RoomManager {
public:
    void join_room(const std::string& room_id,
                   const std::string& role,
                   const std::string& session_id,
                   const std::weak_ptr<WebSocketSession>& session);
    void leave_room(const std::string& room_id,
                    const std::string& role,
                    const std::string& session_id);
    void relay_signal(const std::string& room_id,
                      const std::string& role,
                      const Json& data,
                      const std::string& session_id);
    void remove_session(const std::string& session_id);

private:
    struct RoomEntry {
        std::string host_id;
        std::weak_ptr<WebSocketSession> host;
        std::string viewer_id;
        std::weak_ptr<WebSocketSession> viewer;
    };

    std::mutex mutex_;
    std::unordered_map<std::string, RoomEntry> rooms_;
    std::unordered_map<std::string, std::string> session_room_;
    std::unordered_map<std::string, std::string> session_role_;
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

static void apply_request_id(const Json& req, Json& resp) {
    if (req.contains("requestId") && req["requestId"].is_string()) {
        resp["requestId"] = req["requestId"];
    }
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
    WebSocketSession(tcp::socket socket,
                     asio::thread_pool& dispatcher_pool,
                     asio::thread_pool& stream_pool,
                     std::shared_ptr<RoomManager> room_manager)
        : ws_(std::move(socket))
        , strand_(asio::make_strand(ws_.get_executor()))
        , stream_timer_(strand_)   // timer dùng chung executor với websocket
        , stream_guard_timer_(strand_)
        , dispatcher_pool_(dispatcher_pool)
        , stream_pool_(stream_pool)
        , auth_api_base_(std::getenv("AUTH_API_URL") ? std::getenv("AUTH_API_URL") : "http://localhost:5179")
        , room_manager_(std::move(room_manager))
    {
        static std::atomic<std::uint64_t> session_counter{0};
        session_id_ = "sess-" + std::to_string(++session_counter);
        boost::system::error_code ec;
        auto ep = ws_.next_layer().remote_endpoint(ec);
        if (!ec) {
            remote_ip_ = ep.address().to_string();
        }
    }

    ~WebSocketSession() {
        if (room_manager_) {
            room_manager_->remove_session(session_id_);
        }
    }

    void start() {
        ws_.set_option(ws::stream_base::timeout::suggested(beast::role_type::server));

        ws_.async_accept(
            asio::bind_executor(
                strand_,
                beast::bind_front_handler(
                    &WebSocketSession::on_accept,
                    shared_from_this()
                )
            )
        );
    }

private:
    friend class RoomManager;
    ws::stream<tcp::socket> ws_;
    asio::strand<asio::any_io_executor> strand_;

    beast::flat_buffer buffer_;
    Dispatcher dispatcher_;
    std::optional<VerifiedUser> verified_user_;
    std::string auth_token_;
    std::string auth_api_base_;
    asio::thread_pool& dispatcher_pool_;
    asio::thread_pool& stream_pool_;
    std::size_t pending_jobs_ = 0;
    static constexpr std::size_t max_pending_jobs_ = 32;
    std::unordered_set<std::string> inflight_cmds_;

    std::deque<std::shared_ptr<std::string>> outbox_;
    bool write_in_progress_ = false;
    static constexpr std::size_t max_stream_backlog_ = 5;
    std::string session_id_;
    std::string remote_ip_ = "unknown";
    std::shared_ptr<RoomManager> room_manager_;
    std::chrono::steady_clock::time_point mouse_window_start_{std::chrono::steady_clock::now()};
    std::size_t mouse_move_count_ = 0;

    // Stream state
    asio::steady_timer stream_timer_;
    asio::steady_timer stream_guard_timer_;
    bool streaming_ = false;
    int stream_seq_ = 0;
    int stream_interval_ms_ = 0;
    int stream_total_frames_ = 0;
    std::atomic<std::uint64_t> stream_generation_{0};
    std::atomic<bool> stream_cancelled_{true};
    std::atomic<std::size_t> stream_pending_jobs_{0};
    std::atomic<std::uint64_t> stream_pending_generation_{0};
    static constexpr std::size_t max_stream_pending_jobs_ = 3;

    struct StreamConfig {
        int fps = 5;
        int jpeg_quality = 80;
        int max_width = 0;
        int max_height = 0;
    };

    struct StreamTelemetry {
        std::uint64_t frames_sent = 0;
        std::uint64_t frames_dropped = 0;
        double total_capture_ms = 0.0;
        double total_encode_ms = 0.0;
        std::uint64_t samples = 0;
        std::size_t last_bytes = 0;
        std::chrono::steady_clock::time_point last_stats_log = std::chrono::steady_clock::now();
    };

    StreamConfig stream_config_;
    StreamTelemetry stream_stats_;


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
            asio::bind_executor(
                strand_,
                beast::bind_front_handler(
                    &WebSocketSession::on_read,
                    shared_from_this()
                )
            )
        );
    }

    // ------------------------------------------------------------------------
    void on_read(beast::error_code ec, std::size_t) {
        if (ec == ws::error::closed) {
            handle_disconnect();
            return;
        }
        if (ec) {
            std::cerr << "[WsServer] Read error: " << ec.message() << "\n";
            handle_disconnect();
            return;
        }

        std::string req = beast::buffers_to_string(buffer_.data());
        buffer_.consume(buffer_.size());

        std::cout << "[WsServer] Received: " << req << "\n";
        static constexpr std::size_t kMaxMessageBytes = 256 * 1024;
        if (req.size() > kMaxMessageBytes) {
            Json resp;
            resp["cmd"] = "unknown";
            resp["status"] = "error";
            resp["message"] = "message_too_large";
            send_text(resp.dump());
            do_read();
            return;
        }

        Json j;
        try {
            j = Json::parse(req);
        } catch (const std::exception& e) {
            Json resp;
            resp["cmd"] = "unknown";
            resp["status"] = "error";
            resp["message"] = std::string("invalid_json: ") + e.what();
            send_text(resp.dump());
            do_read();
            return;
        }

        if (j.contains("type") && j["type"].is_string() && j["type"] == "webrtc") {
            handle_webrtc_message(j);
            do_read();
            return;
        }

        if (!j.contains("cmd") || !j["cmd"].is_string()) {
            Json resp;
            resp["cmd"] = "unknown";
            resp["status"] = "error";
            resp["message"] = "missing_cmd";
            apply_request_id(j, resp);
            send_text(resp.dump());
            do_read();
            return;
        }

        std::string cmd = j["cmd"];

        if (cmd == "input-event") {
            const std::string kind = j.value("kind", "");
            const std::string action = j.value("action", "");
            if (kind == "mouse" && action == "move") {
                if (!allow_mouse_move()) {
                    Json resp;
                    resp["cmd"] = "input-event";
                    resp["status"] = "error";
                    resp["error"] = "rate_limited";
                    resp["message"] = "Too many mouse move events";
                    apply_request_id(j, resp);
                    send_text(resp.dump());
                    do_read();
                    return;
                }
            }
            j["client_ip"] = remote_ip_;
            req = j.dump();
        }

        if (cmd == "screen_stream") {
            int duration = j.value("duration", 5);
            int fps = j.value("fps", 5);
            int jpeg_quality = j.value("jpeg_quality", 80);
            int max_width = j.value("max_width", 0);
            int max_height = j.value("max_height", 0);

            StreamConfig config;
            config.fps = clamp_int(fps, 1, 30);
            config.jpeg_quality = clamp_int(jpeg_quality, 30, 95);
            config.max_width = clamp_int(max_width, 0, 7680);
            config.max_height = clamp_int(max_height, 0, 4320);

            if (config.fps != fps) {
                std::cout << "[WsServer] stream config: fps clamped from " << fps << " to " << config.fps << "\n";
            }
            if (config.jpeg_quality != jpeg_quality) {
                std::cout << "[WsServer] stream config: jpeg_quality clamped from " << jpeg_quality
                          << " to " << config.jpeg_quality << "\n";
            }
            if (config.max_width != max_width || config.max_height != max_height) {
                std::cout << "[WsServer] stream config: max dimensions clamped to "
                          << config.max_width << "x" << config.max_height << "\n";
            }
            if ((config.max_width > 0 || config.max_height > 0) && !ScreenCapture::supports_resize()) {
                std::cout << "[WsServer] stream config: resize unsupported, streaming full resolution\n";
                config.max_width = 0;
                config.max_height = 0;
            }

            Json ack;
            ack["cmd"] = "screen_stream";
            if (!start_screen_stream(duration, config)) {
                ack["status"] = "already_running";
            } else {
                ack["status"] = "started";
                ack["duration"] = duration;
                ack["fps"] = config.fps;
                ack["jpeg_quality"] = config.jpeg_quality;
                if (config.max_width > 0) ack["max_width"] = config.max_width;
                if (config.max_height > 0) ack["max_height"] = config.max_height;
            }
            apply_request_id(j, ack);
            send_text(ack.dump());
            do_read();
            return;
        }

        if (cmd == "stop_stream") {
            stop_stream("user");
            Json ack;
            ack["cmd"] = "screen_stream";
            ack["status"] = "stopped";
            apply_request_id(j, ack);
            send_text(ack.dump());
            do_read();
            return;
        }

        if (cmd == "cancel_all") {
            stop_stream("cancel_all");
            Json ack;
            ack["cmd"] = "cancel_all";
            ack["status"] = "ok";
            apply_request_id(j, ack);
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
            apply_request_id(j, ack);
            send_text(ack.dump());
            do_read();
            return;
        }

        if (cmd == "auth") {
            if (!j.contains("token") || !j["token"].is_string()) {
                Json resp;
                resp["cmd"] = "auth";
                resp["status"] = "error";
                resp["message"] = "token required";
                apply_request_id(j, resp);
                send_text(resp.dump());
                do_read();
                return;
            }

            if (!reserve_job(cmd, j)) {
                do_read();
                return;
            }

            std::string incoming_token = j["token"];
            auto self = shared_from_this();
            asio::post(dispatcher_pool_, [self, request = std::move(j), incoming_token]() mutable {
                Json resp;
                resp["cmd"] = "auth";
                auto verified = verify_with_auth_service(self->auth_api_base_, incoming_token);
                if (verified) {
                    resp["status"] = "ok";
                    resp["username"] = verified->username;
                    resp["role"] = verified->role;
                } else {
                    resp["status"] = "error";
                    resp["message"] = "Invalid token";
                }
                apply_request_id(request, resp);

                asio::post(self->strand_, [self, incoming_token, verified, resp = std::move(resp)]() mutable {
                    if (verified) {
                        self->verified_user_ = verified;
                        self->auth_token_ = incoming_token;
                    } else {
                        self->verified_user_.reset();
                        self->auth_token_.clear();
                    }
                    self->send_text(resp.dump());
                    self->finish_job("auth");
                });
            });
            do_read();
            return;
        }

        if (cmd == "restart" || cmd == "shutdown") {
            if (!verified_user_ || verified_user_->role != "admin") {
                Json resp;
                resp["cmd"] = cmd;
                resp["status"] = "error";
                resp["message"] = "admin_token_required";
                apply_request_id(j, resp);
                send_text(resp.dump());
                do_read();
                return;
            }

            if (!reserve_job(cmd, j)) {
                do_read();
                return;
            }

            const std::string token = auth_token_;
            const std::string auth_base = auth_api_base_;
            auto self = shared_from_this();
            asio::post(dispatcher_pool_, [self, request = std::move(j), cmd, token, auth_base]() mutable {
                Json resp;
                resp["cmd"] = cmd;
                SystemControl control;
                bool ok = cmd == "shutdown" ? control.shutdown() : control.restart();
                resp["status"] = ok ? "accepted" : "error";
                if (!ok) resp["message"] = "not_supported";

                if (ok && !token.empty()) {
                    Json meta;
                    meta["cmd"] = cmd;
                    send_audit_remote(auth_base, token, cmd, meta);
                }

                apply_request_id(request, resp);
                asio::post(self->strand_, [self, cmd, resp = std::move(resp)]() mutable {
                    self->send_text(resp.dump());
                    self->finish_job(cmd);
                });
            });
            do_read();
            return;
        }

        enqueue_dispatch_job(cmd, std::move(req), j);
        do_read();
    }

    void handle_disconnect() {
        if (room_manager_) {
            room_manager_->remove_session(session_id_);
        }
    }

    bool allow_mouse_move() {
        const auto now = std::chrono::steady_clock::now();
        if (now - mouse_window_start_ > std::chrono::seconds(1)) {
            mouse_window_start_ = now;
            mouse_move_count_ = 0;
        }
        mouse_move_count_++;
        return mouse_move_count_ <= 200;
    }

    static int clamp_int(int value, int min_value, int max_value) {
        return std::clamp(value, min_value, max_value);
    }

    void send_json(const Json& payload) {
        send_text(payload.dump());
    }

    void handle_webrtc_message(const Json& message) {
        Json resp;
        resp["type"] = "webrtc";
        resp["roomId"] = message.value("roomId", "");
        resp["role"] = message.value("role", "");

        if (!message.contains("roomId") || !message["roomId"].is_string()) {
            resp["action"] = "error";
            resp["message"] = "missing_room";
            send_json(resp);
            return;
        }
        if (!message.contains("role") || !message["role"].is_string()) {
            resp["action"] = "error";
            resp["message"] = "missing_role";
            send_json(resp);
            return;
        }
        if (!message.contains("action") || !message["action"].is_string()) {
            resp["action"] = "error";
            resp["message"] = "missing_action";
            send_json(resp);
            return;
        }

        const std::string room_id = message["roomId"].get<std::string>();
        const std::string role = message["role"].get<std::string>();
        const std::string action = message["action"].get<std::string>();
        if (room_id.empty() || room_id.size() > 64) {
            resp["action"] = "error";
            resp["message"] = "invalid_room";
            send_json(resp);
            return;
        }
        if (role != "host" && role != "viewer") {
            resp["action"] = "error";
            resp["message"] = "invalid_role";
            send_json(resp);
            return;
        }

        if (action == "join") {
            if (room_manager_) {
                room_manager_->join_room(room_id, role, session_id_, weak_from_this());
            }
            return;
        }

        if (action == "leave") {
            if (room_manager_) {
                room_manager_->leave_room(room_id, role, session_id_);
            }
            return;
        }

        if (action == "signal") {
            if (!message.contains("data") || !message["data"].is_object()) {
                resp["action"] = "error";
                resp["message"] = "missing_signal_data";
                send_json(resp);
                return;
            }
            if (room_manager_) {
                room_manager_->relay_signal(room_id, role, message["data"], session_id_);
            }
            return;
        }

        resp["action"] = "error";
        resp["message"] = "unknown_action";
        send_json(resp);
    }

    void send_error_response(const std::string& cmd, const Json& req, const std::string& message) {
        Json resp;
        resp["cmd"] = cmd;
        resp["status"] = "error";
        resp["message"] = message;
        apply_request_id(req, resp);
        send_text(resp.dump());
    }

    bool reserve_job(const std::string& cmd, const Json& req) {
        if (pending_jobs_ >= max_pending_jobs_) {
            send_error_response(cmd, req, "too many pending requests");
            return false;
        }
        if (cmd != "input-event" && inflight_cmds_.count(cmd) > 0) {
            send_error_response(cmd, req, "busy");
            return false;
        }
        if (cmd != "input-event") {
            inflight_cmds_.insert(cmd);
        }
        pending_jobs_++;
        return true;
    }

    void finish_job(const std::string& cmd) {
        inflight_cmds_.erase(cmd);
        if (pending_jobs_ > 0) {
            pending_jobs_--;
        }
    }

    void enqueue_dispatch_job(const std::string& cmd, std::string request, const Json& req) {
        if (!reserve_job(cmd, req)) {
            return;
        }

        auto self = shared_from_this();
        asio::post(dispatcher_pool_, [self, cmd, request = std::move(request)]() mutable {
            std::string resp = self->dispatcher_.handle(request);
            asio::post(self->strand_, [self, cmd, response = std::move(resp)]() mutable {
                self->send_text(response);
                self->finish_job(cmd);
            });
        });
    }

    // ------------------------------------------------------------------------
    void send_text(const std::string& s) {
        enqueue_write(std::make_shared<std::string>(s), false);
    }

    void enqueue_write(std::shared_ptr<std::string> msg, bool drop_if_busy = false) {
        asio::dispatch(
            strand_,
            [self = shared_from_this(), msg = std::move(msg), drop_if_busy]() mutable {
                if (drop_if_busy && self->outbox_.size() >= max_stream_backlog_) {
                    return;
                }
                self->outbox_.push_back(std::move(msg));
                if (!self->write_in_progress_) {
                    self->write_in_progress_ = true;
                    self->do_write();
                }
            }
        );
    }

    bool enqueue_stream_write(std::shared_ptr<std::string> msg) {
        if (outbox_.size() >= max_stream_backlog_) {
            return false;
        }
        outbox_.push_back(std::move(msg));
        if (!write_in_progress_) {
            write_in_progress_ = true;
            do_write();
        }
        return true;
    }

    void do_write() {
        if (outbox_.empty()) {
            write_in_progress_ = false;
            return;
        }

        auto msg = outbox_.front();
        ws_.text(true);
        ws_.async_write(
            asio::buffer(*msg),
            asio::bind_executor(
                strand_,
                [self = shared_from_this(), msg](beast::error_code ec, std::size_t) {
                    self->on_write(ec);
                }
            )
        );
    }

    void on_write(const beast::error_code& ec) {
        if (outbox_.empty()) {
            write_in_progress_ = false;
            return;
        }
        if (ec) {
            std::cerr << "[WsServer] Write error: " << ec.message() << "\n";
            stop_stream("write_failed");
        }
        outbox_.pop_front();
        do_write();
    }

    // ------------------------------------------------------------------------
    bool start_screen_stream(int duration, const StreamConfig& config) {
        if (streaming_) {
            std::cout << "[WsServer] Screen stream request rejected: already streaming\n";
            return false;
        }

        if (duration < 1) duration = 3;
        if (duration > 60) duration = 60;

        streaming_ = true;
        stream_seq_ = 0;
        stream_config_ = config;
        stream_interval_ms_ = 1000 / stream_config_.fps;
        stream_total_frames_ = duration * stream_config_.fps;
        const auto generation = stream_generation_.fetch_add(1) + 1;
        stream_cancelled_.store(false);
        stream_pending_generation_.store(generation);
        stream_pending_jobs_.store(0);
        stream_stats_ = StreamTelemetry{};

        stream_guard_timer_.expires_after(std::chrono::seconds(60));
        stream_guard_timer_.async_wait([self = shared_from_this(), generation](const beast::error_code& ec) {
            if (!ec && generation == self->stream_generation_.load()) {
                self->stop_stream("timeout");
            }
        });

        std::cout << "[WsServer] Streaming start: "
                  << stream_config_.fps << " fps, "
                  << duration << " sec, total frames = "
                  << stream_total_frames_
                  << ", jpeg_quality=" << stream_config_.jpeg_quality;
        if (stream_config_.max_width > 0 || stream_config_.max_height > 0) {
            std::cout << ", max=" << stream_config_.max_width << "x" << stream_config_.max_height;
        }
        std::cout << "\n";

        stream_timer_.expires_after(std::chrono::milliseconds(stream_interval_ms_));
        stream_timer_.async_wait(
            [self = shared_from_this(), generation](const beast::error_code& ec) {
                self->do_stream_frame(ec, generation);
            }
        );
        return true;
    }

    // ------------------------------------------------------------------------
    void stop_stream(const std::string& reason) {
        streaming_ = false;
        stream_cancelled_.store(true);
        stream_generation_.fetch_add(1);
        beast::error_code cancel_ec;
        stream_timer_.cancel(cancel_ec);
        stream_guard_timer_.cancel(cancel_ec);
        std::cout << "[WsServer] Stream stopped (" << reason << ")\n";
    }

    // ------------------------------------------------------------------------
    void do_stream_frame(beast::error_code ec, std::uint64_t generation) {
        if (ec == asio::error::operation_aborted) return;
        if (!streaming_ || generation != stream_generation_.load() || stream_cancelled_.load()) return;

        if (stream_seq_ >= stream_total_frames_) {
            std::cout << "[WsServer] Stream finished\n";
            stop_stream("complete");
            return;
        }

        maybe_log_stream_stats();

        const auto pending_jobs = stream_pending_jobs_.load();
        if (pending_jobs >= max_stream_pending_jobs_ || outbox_.size() >= max_stream_backlog_) {
            stream_stats_.frames_dropped++;
            std::cout << "[WsServer] stream_drop_frame reason=backpressure pending=" << pending_jobs << "\n";
            stream_seq_++;
            schedule_next_stream_tick(generation);
            return;
        }

        stream_pending_jobs_.fetch_add(1);
        const int seq = stream_seq_;
        const auto config = stream_config_;
        auto self = shared_from_this();
        asio::post(stream_pool_, [self, generation, seq, config]() {
            if (self->stream_cancelled_.load() || generation != self->stream_generation_.load()) {
                asio::post(self->strand_, [self, generation]() {
                    self->complete_stream_job(generation);
                    if (generation != self->stream_generation_.load()) {
                        self->stream_stats_.frames_dropped++;
                    }
                });
                return;
            }

            ScreenCaptureOptions options;
            options.jpeg_quality = config.jpeg_quality;
            options.max_width = config.max_width;
            options.max_height = config.max_height;
            auto result = ScreenCapture::capture_base64(options);
            asio::post(self->strand_, [self, generation, seq, result = std::move(result)]() mutable {
                self->handle_stream_result(generation, seq, std::move(result));
            });
        });

        stream_seq_++;
        schedule_next_stream_tick(generation);
    }

    void schedule_next_stream_tick(std::uint64_t generation) {
        stream_timer_.expires_after(std::chrono::milliseconds(stream_interval_ms_));
        stream_timer_.async_wait(
            [self = shared_from_this(), generation](const beast::error_code& timer_ec) {
                self->do_stream_frame(timer_ec, generation);
            }
        );
    }

    void complete_stream_job(std::uint64_t generation) {
        if (generation != stream_pending_generation_.load()) {
            return;
        }
        auto pending = stream_pending_jobs_.load();
        if (pending > 0) {
            stream_pending_jobs_.fetch_sub(1);
        }
    }

    void handle_stream_result(std::uint64_t generation, int seq, ScreenCaptureResult result) {
        complete_stream_job(generation);
        if (!streaming_ || generation != stream_generation_.load() || stream_cancelled_.load()) {
            stream_stats_.frames_dropped++;
            return;
        }

        if (result.base64.empty()) {
            std::cerr << "[WsServer] ScreenCapture failed\n";
            stop_stream("capture_failed");
            return;
        }

        stream_stats_.samples++;
        stream_stats_.total_capture_ms += result.capture_ms;
        stream_stats_.total_encode_ms += result.encode_ms;
        stream_stats_.last_bytes = result.bytes;

        Json j;
        j["cmd"] = "screen_stream";
        j["seq"] = seq;
        j["streamId"] = generation;
        j["image_base64"] = result.base64;
        j["width"] = result.width;
        j["height"] = result.height;
        if (result.resized) {
            j["resized"] = true;
        }

        if (enqueue_stream_write(std::make_shared<std::string>(j.dump()))) {
            stream_stats_.frames_sent++;
        } else {
            stream_stats_.frames_dropped++;
            std::cout << "[WsServer] stream_drop_frame reason=backpressure pending=" << stream_pending_jobs_.load() << "\n";
        }
    }

    void maybe_log_stream_stats() {
        const auto now = std::chrono::steady_clock::now();
        if (now - stream_stats_.last_stats_log < std::chrono::seconds(2)) {
            return;
        }
        stream_stats_.last_stats_log = now;
        const double avg_capture = stream_stats_.samples > 0 ? stream_stats_.total_capture_ms / stream_stats_.samples : 0.0;
        const double avg_encode = stream_stats_.samples > 0 ? stream_stats_.total_encode_ms / stream_stats_.samples : 0.0;
        std::cout << "[WsServer] stream_stats sent=" << stream_stats_.frames_sent
                  << " dropped=" << stream_stats_.frames_dropped
                  << " avg_capture_ms=" << avg_capture
                  << " avg_encode_ms=" << avg_encode
                  << " last_bytes=" << stream_stats_.last_bytes
                  << "\n";
    }
};

// ============================================================================
// RoomManager
// ============================================================================
void RoomManager::join_room(const std::string& room_id,
                            const std::string& role,
                            const std::string& session_id,
                            const std::weak_ptr<WebSocketSession>& session) {
    std::shared_ptr<WebSocketSession> sender = session.lock();
    std::shared_ptr<WebSocketSession> peer;
    std::shared_ptr<WebSocketSession> replaced;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto& entry = rooms_[room_id];
        if (role == "host") {
            if (!entry.host_id.empty() && entry.host_id != session_id) {
                replaced = entry.host.lock();
                session_room_.erase(entry.host_id);
                session_role_.erase(entry.host_id);
            }
            entry.host_id = session_id;
            entry.host = session;
            peer = entry.viewer.lock();
        } else {
            if (!entry.viewer_id.empty() && entry.viewer_id != session_id) {
                replaced = entry.viewer.lock();
                session_room_.erase(entry.viewer_id);
                session_role_.erase(entry.viewer_id);
            }
            entry.viewer_id = session_id;
            entry.viewer = session;
            peer = entry.host.lock();
        }
        session_room_[session_id] = room_id;
        session_role_[session_id] = role;
    }

    if (sender) {
        Json joined;
        joined["type"] = "webrtc";
        joined["roomId"] = room_id;
        joined["role"] = role;
        joined["action"] = "joined";
        joined["status"] = "ok";
        sender->send_json(joined);
    }

    if (replaced && replaced != sender) {
        Json payload;
        payload["type"] = "webrtc";
        payload["roomId"] = room_id;
        payload["role"] = role;
        payload["action"] = "peer-left";
        replaced->send_json(payload);
    }

    if (peer && sender) {
        Json ready_for_sender;
        ready_for_sender["type"] = "webrtc";
        ready_for_sender["roomId"] = room_id;
        ready_for_sender["role"] = role;
        ready_for_sender["action"] = "peer-ready";
        sender->send_json(ready_for_sender);

        Json ready_for_peer;
        ready_for_peer["type"] = "webrtc";
        ready_for_peer["roomId"] = room_id;
        ready_for_peer["role"] = role == "host" ? "viewer" : "host";
        ready_for_peer["action"] = "peer-ready";
        peer->send_json(ready_for_peer);
    }
}

void RoomManager::leave_room(const std::string& room_id,
                             const std::string& role,
                             const std::string& session_id) {
    std::shared_ptr<WebSocketSession> peer;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = rooms_.find(room_id);
        if (it == rooms_.end()) return;
        auto& entry = it->second;

        if (role == "host" && entry.host_id == session_id) {
            entry.host_id.clear();
            entry.host.reset();
            peer = entry.viewer.lock();
        } else if (role == "viewer" && entry.viewer_id == session_id) {
            entry.viewer_id.clear();
            entry.viewer.reset();
            peer = entry.host.lock();
        }

        session_room_.erase(session_id);
        session_role_.erase(session_id);

        if (entry.host_id.empty() && entry.viewer_id.empty()) {
            rooms_.erase(it);
        }
    }

    if (peer) {
        Json payload;
        payload["type"] = "webrtc";
        payload["roomId"] = room_id;
        payload["role"] = role == "host" ? "viewer" : "host";
        payload["action"] = "peer-left";
        peer->send_json(payload);
    }
}

void RoomManager::relay_signal(const std::string& room_id,
                               const std::string& role,
                               const Json& data,
                               const std::string& session_id) {
    (void)session_id;
    std::shared_ptr<WebSocketSession> peer;
    std::shared_ptr<WebSocketSession> sender;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = rooms_.find(room_id);
        if (it == rooms_.end()) {
            return;
        }
        auto& entry = it->second;
        if (role == "host") {
            peer = entry.viewer.lock();
            sender = entry.host.lock();
        } else {
            peer = entry.host.lock();
            sender = entry.viewer.lock();
        }
    }

    if (!peer) {
        if (sender) {
            Json error;
            error["type"] = "webrtc";
            error["roomId"] = room_id;
            error["role"] = role;
            error["action"] = "error";
            error["message"] = "peer_not_ready";
            sender->send_json(error);
        }
        return;
    }

    Json relay;
    relay["type"] = "webrtc";
    relay["roomId"] = room_id;
    relay["role"] = role;
    relay["action"] = "signal";
    relay["data"] = data;
    peer->send_json(relay);
}

void RoomManager::remove_session(const std::string& session_id) {
    std::string room;
    std::string role;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = session_room_.find(session_id);
        if (it == session_room_.end()) return;
        room = it->second;
        auto role_it = session_role_.find(session_id);
        if (role_it == session_role_.end()) return;
        role = role_it->second;
    }
    leave_room(room, role, session_id);
}


// ============================================================================
// Listener
// ============================================================================
class Listener : public std::enable_shared_from_this<Listener> {
public:
    Listener(asio::io_context& ioc,
             tcp::endpoint endpoint,
             asio::thread_pool& dispatcher_pool,
             asio::thread_pool& stream_pool,
             std::shared_ptr<RoomManager> room_manager)
        : ioc_(ioc)
        , acceptor_(ioc)
        , dispatcher_pool_(dispatcher_pool)
        , stream_pool_(stream_pool)
        , room_manager_(std::move(room_manager))
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
    asio::thread_pool& dispatcher_pool_;
    asio::thread_pool& stream_pool_;
    std::shared_ptr<RoomManager> room_manager_;

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
            std::make_shared<WebSocketSession>(std::move(socket), dispatcher_pool_, stream_pool_, room_manager_)->start();
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
    asio::thread_pool dispatcher_pool{std::max(2u, std::thread::hardware_concurrency())};
    asio::thread_pool stream_pool{std::max(2u, std::thread::hardware_concurrency())};
    std::shared_ptr<RoomManager> room_manager = std::make_shared<RoomManager>();

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
        std::make_shared<Listener>(ioc, ep, dispatcher_pool, stream_pool, room_manager)->run();
        std::cout << "[WsServer] Listening on " << addr << ":" << port << "\n";
        ioc.run();

        dispatcher_pool.join();
        stream_pool.join();
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

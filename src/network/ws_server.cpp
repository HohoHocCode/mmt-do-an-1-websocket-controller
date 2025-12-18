#include "network/ws_server.hpp"
#include "core/dispatcher.hpp"
#include "utils/json.hpp"
#include "modules/screen.hpp"

#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>

#include <algorithm>
#include <iostream>
#include <memory>
#include <string>
#include <chrono>

namespace asio  = boost::asio;
namespace beast = boost::beast;
namespace ws    = beast::websocket;
using tcp       = asio::ip::tcp;

namespace {
constexpr std::size_t kMaxMessageBytes = 512 * 1024; // 512KB guard
constexpr int kMaxStreamSeconds = 60;
}


// ============================================================================
// WebSocketSession
// ============================================================================
class WebSocketSession : public std::enable_shared_from_this<WebSocketSession> {
public:
    explicit WebSocketSession(tcp::socket socket)
        : ws_(std::move(socket))
        , stream_timer_(ws_.get_executor())   // timer dùng chung executor với websocket
        , stream_timeout_timer_(ws_.get_executor())
    {}

    void start() {
        ws_.set_option(ws::stream_base::timeout::suggested(beast::role_type::server));
        ws_.read_message_max(kMaxMessageBytes);

        ws_.async_accept(
            beast::bind_front_handler(
                &WebSocketSession::on_accept,
                shared_from_this()
            )
        );
    }

private:
    struct SessionState {
        bool streaming = false;
        int stream_seq = 0;
        int stream_interval_ms = 0;
        int stream_total_frames = 0;
        int requested_duration = 0;
        int requested_fps = 0;
        std::size_t stream_generation = 0;
        std::string active_request_id;
    };

    ws::stream<tcp::socket> ws_;

    beast::flat_buffer buffer_;
    Dispatcher dispatcher_;

    // Stream state
    asio::steady_timer stream_timer_;
    asio::steady_timer stream_timeout_timer_;
    SessionState state_;


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

        handle_message(req);

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
    void attach_request_id(Json& response, const Json& request) {
        auto it = request.find("requestId");
        if (it != request.end() && it->is_string()) {
            response["requestId"] = *it;
        }
    }

    void send_json(Json response, const Json& request) {
        if (!response.contains("cmd") && request.contains("cmd") && request["cmd"].is_string()) {
            response["cmd"] = request["cmd"];
        }
        attach_request_id(response, request);
        send_text(response.dump());
    }

    void send_error(const std::string& code, const std::string& message, const Json& request = Json::object()) {
        Json err;
        err["status"] = "error";
        err["error"] = code;
        err["message"] = message;
        send_json(err, request);
    }

    void handle_message(const std::string& raw) {
        if (raw.size() > kMaxMessageBytes) {
            send_error("request_too_large", "Request exceeds maximum size limit.");
            return;
        }

        Json req;
        try {
            req = Json::parse(raw);
        } catch (const std::exception& e) {
            send_error("invalid_json", e.what());
            return;
        }

        if (!req.is_object()) {
            send_error("invalid_request", "Expected a JSON object for requests.");
            return;
        }

        if (!req.contains("cmd") || !req["cmd"].is_string()) {
            send_error("invalid_command", "Field 'cmd' is required and must be a string.", req);
            return;
        }

        std::string cmd = req["cmd"];

        if (state_.streaming && cmd != "screen_stream") {
            cancel_stream("superseded_by_command");
        }

        if (cmd == "cancel_all" || cmd == "reset") {
            cancel_stream("reset_command");
            state_ = SessionState{};
            Json resp = dispatcher_.handle(req);
            send_json(resp, req);
            return;
        }

        if (cmd == "screen_stream") {
            int duration = 5;
            int fps = 3;

            if (req.contains("duration")) {
                if (!req["duration"].is_number_integer()) {
                    send_error("invalid_duration", "'duration' must be an integer.", req);
                    return;
                }
                duration = req["duration"];
            }

            if (req.contains("fps")) {
                if (!req["fps"].is_number_integer()) {
                    send_error("invalid_fps", "'fps' must be an integer.", req);
                    return;
                }
                fps = req["fps"];
            }

            duration = std::max(1, std::min(duration, kMaxStreamSeconds));
            fps = std::max(1, std::min(fps, 30));

            start_screen_stream(duration, fps, req);

            Json ack;
            ack["cmd"] = "screen_stream";
            ack["status"] = "started";
            ack["duration"] = duration;
            ack["fps"] = fps;
            ack["message"] = "Screen stream started.";

            send_json(ack, req);
            return;
        }

        Json resp = dispatcher_.handle(req);
        send_json(resp, req);
    }

    void start_screen_stream(int duration, int fps, const Json& req) {
        cancel_stream("restarting_stream");

        state_.stream_generation++;
        state_.streaming = true;
        state_.stream_seq = 0;
        state_.requested_duration = duration;
        state_.requested_fps = fps;
        state_.stream_interval_ms = 1000 / fps;
        state_.stream_total_frames = duration * fps;
        state_.active_request_id = req.contains("requestId") && req["requestId"].is_string()
            ? req["requestId"].get<std::string>()
            : std::string{};

        std::cout << "[WsServer] Streaming start: "
                  << fps << " fps, "
                  << duration << " sec, total frames = "
                  << state_.stream_total_frames << "\n";

        stream_timer_.expires_after(std::chrono::milliseconds(state_.stream_interval_ms));
        stream_timer_.async_wait(
            beast::bind_front_handler(
                &WebSocketSession::do_stream_frame,
                shared_from_this(),
                state_.stream_generation
            )
        );

        stream_timeout_timer_.expires_after(std::chrono::seconds(kMaxStreamSeconds));
        stream_timeout_timer_.async_wait(
            [self = shared_from_this(), token = state_.stream_generation](beast::error_code ec) {
                if (ec == asio::error::operation_aborted) return;
                self->cancel_stream("timeout", token);
            }
        );
    }

    Json build_stream_status(const std::string& reason) const {
        Json status;
        status["cmd"] = "screen_stream";
        status["status"] = "stopped";
        status["reason"] = reason;
        if (!state_.active_request_id.empty()) {
            status["requestId"] = state_.active_request_id;
        }
        return status;
    }

    void cancel_stream(const std::string& reason, std::size_t token = 0, bool notify = true) {
        if (!state_.streaming) {
            return;
        }
        if (token != 0 && token != state_.stream_generation) {
            return;
        }

        state_.streaming = false;
        stream_timer_.cancel();
        stream_timeout_timer_.cancel();

        if (notify) {
            send_text(build_stream_status(reason).dump());
        }

        reset_stream_fields();
    }

    void reset_stream_fields() {
        state_.stream_seq = 0;
        state_.stream_total_frames = 0;
        state_.stream_interval_ms = 0;
        state_.requested_duration = 0;
        state_.requested_fps = 0;
        state_.active_request_id.clear();
    }

    // ------------------------------------------------------------------------
    void do_stream_frame(beast::error_code ec, std::size_t token) {
        if (ec == asio::error::operation_aborted) return;
        if (!state_.streaming || token != state_.stream_generation) return;

        if (state_.stream_seq >= state_.stream_total_frames) {
            std::cout << "[WsServer] Stream finished\n";
            state_.streaming = false;
            stream_timeout_timer_.cancel();
            send_text(build_stream_status("completed").dump());
            reset_stream_fields();
            return;
        }

        std::string b64 = ScreenCapture::capture_base64();
        if (b64.empty()) {
            std::cerr << "[WsServer] ScreenCapture failed\n";
            cancel_stream("capture_failed", token);
            return;
        }

        Json j;
        j["cmd"] = "screen_stream";
        j["seq"] = state_.stream_seq;
        j["image_base64"] = b64;
        if (!state_.active_request_id.empty()) {
            j["requestId"] = state_.active_request_id;
        }

        auto msg = std::make_shared<std::string>(j.dump());

        ws_.text(true);
        ws_.async_write(
            asio::buffer(*msg),
            [self = shared_from_this(), msg, token](beast::error_code write_ec, std::size_t) {
                if (write_ec) {
                    std::cerr << "[WsServer] Stream write error: "
                              << write_ec.message() << "\n";
                    self->cancel_stream("write_error", token, false);
                    return;
                }

                if (!self->state_.streaming || token != self->state_.stream_generation) return;

                self->state_.stream_seq++;

                self->stream_timer_.expires_after(
                    std::chrono::milliseconds(self->state_.stream_interval_ms)
                );

                self->stream_timer_.async_wait(
                    beast::bind_front_handler(
                        &WebSocketSession::do_stream_frame,
                        self,
                        token
                    )
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

    void start(const std::string& addr, unsigned short port) {
        tcp::endpoint ep(asio::ip::make_address(addr), port);
        std::make_shared<Listener>(ioc, ep)->run();
        std::cout << "[WsServer] Listening on " << addr << ":" << port << "\n";
        ioc.run();
    }
};

WsServer::WsServer() : pimpl_(std::make_unique<Impl>()) {}
WsServer::~WsServer() = default;

void WsServer::run(const std::string& addr, unsigned short port) {
    pimpl_->start(addr, port);
}

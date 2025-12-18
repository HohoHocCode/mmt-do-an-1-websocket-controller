#include "network/ws_server.hpp"
#include "core/dispatcher.hpp"
#include "utils/json.hpp"
#include "modules/screen.hpp"

#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>

#include <iostream>
#include <memory>
#include <string>
#include <chrono>

namespace asio  = boost::asio;
namespace beast = boost::beast;
namespace ws    = beast::websocket;
using tcp       = asio::ip::tcp;


// ============================================================================
// WebSocketSession
// ============================================================================
class WebSocketSession : public std::enable_shared_from_this<WebSocketSession> {
public:
    explicit WebSocketSession(tcp::socket socket)
        : ws_(std::move(socket))
        , stream_timer_(ws_.get_executor())   // timer dùng chung executor với websocket
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

    // Stream state
    asio::steady_timer stream_timer_;
    bool streaming_ = false;
    int stream_seq_ = 0;
    int stream_interval_ms_ = 0;
    int stream_total_frames_ = 0;


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

                if (j["cmd"] == "screen_stream") {
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

        streaming_ = true;
        stream_seq_ = 0;
        stream_interval_ms_ = 1000 / fps;
        stream_total_frames_ = duration * fps;

        std::cout << "[WsServer] Streaming start: "
                  << fps << " fps, "
                  << duration << " sec, total frames = "
                  << stream_total_frames_ << "\n";

        stream_timer_.expires_after(std::chrono::milliseconds(stream_interval_ms_));
        stream_timer_.async_wait(
            beast::bind_front_handler(
                &WebSocketSession::do_stream_frame,
                shared_from_this()
            )
        );
    }

    // ------------------------------------------------------------------------
    void do_stream_frame(beast::error_code ec) {
        if (ec == asio::error::operation_aborted) return;
        if (!streaming_) return;

        if (stream_seq_ >= stream_total_frames_) {
            std::cout << "[WsServer] Stream finished\n";
            streaming_ = false;
            return;
        }

        std::string b64 = ScreenCapture::capture_base64();
        if (b64.empty()) {
            std::cerr << "[WsServer] ScreenCapture failed\n";
            streaming_ = false;
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
                    self->streaming_ = false;
                    return;
                }

                if (!self->streaming_) return;

                self->stream_seq_++;

                self->stream_timer_.expires_after(
                    std::chrono::milliseconds(self->stream_interval_ms_)
                );

                self->stream_timer_.async_wait(
                    beast::bind_front_handler(
                        &WebSocketSession::do_stream_frame,
                        self
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

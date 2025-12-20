#include "network/ws_client.hpp"
#include <iostream>

WsClient::WsClient()
    : resolver_(ioc_)
    , work_(net::make_work_guard(ioc_))   
{
}

WsClient::~WsClient()
{
    close();
}

void WsClient::connect(const std::string& host,
                       const std::string& port,
                       const std::string& target)
{
    host_ = host;
    port_ = port;
    target_ = target;

    std::cout << "[DEBUG] host_=\"" << host_ << "\" size=" << host_.size() << std::endl;

    ws_ = std::make_unique<websocket::stream<tcp::socket>>(ioc_);

    io_thread_ = std::make_unique<std::thread>([this]() {
        std::cout << "[Client] io_context thread started" << std::endl;
        ioc_.run();
        std::cout << "[Client] io_context thread stopped" << std::endl;
    });

    do_resolve();
}

void WsClient::do_resolve()
{
    std::cout << "[Client] Resolving..." << std::endl;

    resolver_.async_resolve(
        host_,
        port_,
        [this](beast::error_code ec, tcp::resolver::results_type results)
        {
            if (ec)
            {
                if (on_error_) on_error_("Resolve failed: " + ec.message());
                return;
            }

            std::cout << "[Client] Resolve OK, got endpoints" << std::endl;
            do_connect(results);
        }
    );
}

void WsClient::do_connect(tcp::resolver::results_type results)
{
    net::async_connect(
        ws_->next_layer(),      // tcp::socket
        results,
        [this](beast::error_code ec, const tcp::endpoint& ep)
        {
            if (ec)
            {
                if (on_error_) on_error_("Connect failed: " + ec.message());
                return;
            }

            std::cout << "[Client] TCP connected OK to "
                      << ep.address().to_string() << ":" << ep.port() << std::endl;

            do_handshake();
        }
    );
}

void WsClient::do_handshake()
{
    ws_->async_handshake(
        host_,
        target_,
        [this](beast::error_code ec)
        {
            if (ec)
            {
                if (on_error_) on_error_("Handshake failed: " + ec.message());
                return;
            }

            connected_ = true;
            std::cout << "[Client] Handshake OK." << std::endl;
            send(R"({"cmd":"ping"})");

            start_read_loop();
        }
    );
}

void WsClient::set_message_handler(MessageHandler handler)
{
    on_message_ = std::move(handler);
}

void WsClient::set_error_handler(ErrorHandler handler)
{
    on_error_ = std::move(handler);
}

void WsClient::send(const std::string& msg)
{
    if (!connected_ || !ws_) return;

    auto shared_msg = std::make_shared<std::string>(msg);

    ws_->async_write(
        net::buffer(*shared_msg),
        [this, shared_msg](beast::error_code ec, std::size_t)
        {
            if (ec && on_error_)
                on_error_("Send failed: " + ec.message());
        }
    );
}

void WsClient::close()
{
    if (!ws_) return;

    beast::error_code ec;
    ws_->close(websocket::close_code::normal, ec);

    connected_ = false;

    work_.reset();
    ioc_.stop();

    if (io_thread_ && io_thread_->joinable())
        io_thread_->join();
}

bool WsClient::is_connected() const {
    return connected_.load();
}

void WsClient::start_read_loop()
{
    auto buffer = std::make_shared<beast::flat_buffer>();

    ws_->async_read(
        *buffer,
        [this, buffer](beast::error_code ec, std::size_t /*bytes_transferred*/)
        {
            if (ec)
            {
                if (on_error_) on_error_("Read failed: " + ec.message());
                return;
            }

            std::string msg(
                beast::buffers_to_string(buffer->data())
            );

            if (on_message_)
                on_message_(msg);

            start_read_loop();
        }
    );
}

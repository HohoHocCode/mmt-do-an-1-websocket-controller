#pragma once

#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <functional>
#include <string>
#include <thread>
#include <memory>
#include <atomic>

namespace net  = boost::asio;
namespace beast = boost::beast;
namespace websocket = beast::websocket;
using tcp = net::ip::tcp;

class WsClient {
public:
    using MessageHandler = std::function<void(const std::string&)>;
    using ErrorHandler   = std::function<void(const std::string&)>;

    WsClient();
    ~WsClient();

    void connect(const std::string& host,
                 const std::string& port,
                 const std::string& target = "/");

    void send(const std::string& msg);
    void close();

    void set_message_handler(MessageHandler handler);
    void set_error_handler(ErrorHandler handler);

private:
    void do_resolve();
    void do_connect(tcp::resolver::results_type results);
    void do_handshake();
    void start_read_loop();

private:
    net::io_context ioc_;
    tcp::resolver resolver_;

    net::executor_work_guard<net::io_context::executor_type> work_;

    std::unique_ptr<websocket::stream<tcp::socket>> ws_;
    std::unique_ptr<std::thread> io_thread_;

    std::string host_;
    std::string port_;
    std::string target_;

    MessageHandler on_message_;
    ErrorHandler   on_error_;

    std::atomic<bool> connected_{false};
};

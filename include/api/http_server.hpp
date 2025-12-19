#pragma once

#include "api/auth_service.hpp"
#include "api/discovery.hpp"
#include "api/stream_manager.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <memory>
#include <string>

class ApiServer {
public:
    ApiServer(const std::string& address, unsigned short port, Database db);
    void run();

private:
    boost::asio::io_context ioc_;
    boost::asio::ip::tcp::acceptor acceptor_;
    std::string address_;
    unsigned short port_;
    std::shared_ptr<AuthService> auth_;
    std::shared_ptr<ScreenStreamManager> stream_manager_;
    std::shared_ptr<DiscoveryService> discovery_;

    void do_accept();
};

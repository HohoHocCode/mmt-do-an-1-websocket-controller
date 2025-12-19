#pragma once

#include "utils/json.hpp"

#include <boost/asio/io_context.hpp>

#include <string>
#include <vector>

class DiscoveryService {
public:
    explicit DiscoveryService(boost::asio::io_context& ioc);

    Json scan(unsigned int timeout_ms, unsigned short port, const std::string& nonce);

private:
    boost::asio::io_context& ioc_;
};

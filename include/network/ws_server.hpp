#pragma once
#include <string>
#include <memory>
#include "core/dispatcher.hpp"

class WsServer {
public:
    WsServer();
    ~WsServer();

    void run(const std::string& address, unsigned short port);
    void stop();

private:
    struct Impl;
    std::unique_ptr<Impl> pimpl_;
    Dispatcher dispatcher_;

};

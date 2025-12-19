#pragma once

#include "utils/json.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

struct StreamSessionState {
    bool running = false;
    int fps = 0;
    int duration = 0;
    int seq = 0;
    std::unique_ptr<boost::asio::steady_timer> deadline;
};

class ScreenStreamManager {
public:
    explicit ScreenStreamManager(boost::asio::io_context& ioc);

    Json start(const std::string& session_id, int duration, int fps);
    Json stop(const std::string& session_id);
    Json reset(const std::string& session_id);
    Json cancel_all(const std::string& session_id);

private:
    boost::asio::io_context& ioc_;
    std::unordered_map<std::string, StreamSessionState> sessions_;
    std::mutex mutex_;

    void stop_locked(const std::string& session_id, const char* reason);
};

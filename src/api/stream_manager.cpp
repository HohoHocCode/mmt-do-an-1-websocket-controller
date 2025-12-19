#include "api/stream_manager.hpp"
#include "api/logger.hpp"

#include <algorithm>
#include <chrono>

namespace asio = boost::asio;

ScreenStreamManager::ScreenStreamManager(asio::io_context& ioc) : ioc_(ioc) {}

Json ScreenStreamManager::start(const std::string& session_id, int duration, int fps) {
    const int capped_duration = std::max(1, std::min(duration, 60));
    const int capped_fps = std::max(1, std::min(fps, 30));

    std::lock_guard<std::mutex> lock(mutex_);
    auto& state = sessions_[session_id];
    if (state.running) {
        return {{"ok", false}, {"error", "already_running"}, {"fps", state.fps}, {"duration", state.duration}};
    }

    state.running = true;
    state.fps = capped_fps;
    state.duration = capped_duration;
    state.seq = 0;
    state.deadline = std::make_unique<asio::steady_timer>(ioc_);
    state.deadline->expires_after(std::chrono::seconds(capped_duration));
    state.deadline->async_wait([this, session_id](const boost::system::error_code& ec) {
        if (!ec) {
            stop_locked(session_id, "timeout");
        }
    });

    return {
        {"ok", true},
        {"status", "started"},
        {"duration", capped_duration},
        {"fps", capped_fps}
    };
}

void ScreenStreamManager::stop_locked(const std::string& session_id, const char* reason) {
    auto it = sessions_.find(session_id);
    if (it == sessions_.end()) return;
    auto& state = it->second;
    if (state.deadline) {
        boost::system::error_code ec;
        state.deadline->cancel(ec);
    }
    if (state.running) {
        Logger::instance().info("Stopping stream for session '" + session_id + "' (" + reason + ")");
    }
    state.running = false;
    state.fps = 0;
    state.duration = 0;
    state.seq = 0;
}

Json ScreenStreamManager::stop(const std::string& session_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    stop_locked(session_id, "stop_request");
    return {{"ok", true}, {"status", "stopped"}};
}

Json ScreenStreamManager::reset(const std::string& session_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    stop_locked(session_id, "reset_request");
    sessions_.erase(session_id);
    return {{"ok", true}, {"status", "reset"}};
}

Json ScreenStreamManager::cancel_all(const std::string& session_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    stop_locked(session_id, "cancel_request");
    return {{"ok", true}, {"status", "cancelled"}};
}

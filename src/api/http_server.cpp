#include "api/http_server.hpp"

#include "api/logger.hpp"
#include "api/password_hash.hpp"
#include "modules/process.hpp"
#include "utils/json.hpp"

#include <boost/asio/dispatch.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>

#include <memory>
#include <string>

namespace beast = boost::beast;
namespace http = beast::http;
namespace asio = boost::asio;
using tcp = asio::ip::tcp;

namespace {
Json parse_json_body(const http::request<http::string_body>& req) {
    if (req.body().empty()) return Json::object();
    try {
        return Json::parse(req.body());
    } catch (...) {
        return Json::object();
    }
}

std::string extract_bearer(const http::request<http::string_body>& req) {
    auto it = req.find(http::field::authorization);
    if (it == req.end()) return {};
    const std::string value = it->value().to_string();
    const std::string prefix = "Bearer ";
    if (value.rfind(prefix, 0) == 0) {
        return value.substr(prefix.size());
    }
    return value;
}

template <class Body, class Allocator, class Send>
void handle_bad_request(http::request<Body, http::basic_fields<Allocator>>&& req,
                        const std::string& why,
                        Send&& send) {
    http::response<http::string_body> res{http::status::bad_request, req.version()};
    res.set(http::field::content_type, "application/json");
    res.keep_alive(req.keep_alive());
    Json body;
    body["error"] = why;
    res.body() = body.dump();
    res.prepare_payload();
    send(std::move(res));
}

template <class Body, class Allocator, class Send>
void handle_not_found(http::request<Body, http::basic_fields<Allocator>>&& req,
                      Send&& send) {
    http::response<http::string_body> res{http::status::not_found, req.version()};
    res.set(http::field::content_type, "application/json");
    res.keep_alive(req.keep_alive());
    Json body;
    body["error"] = "not_found";
    res.body() = body.dump();
    res.prepare_payload();
    send(std::move(res));
}
} // namespace

class HttpSession : public std::enable_shared_from_this<HttpSession> {
public:
    HttpSession(tcp::socket socket,
                std::shared_ptr<AuthService> auth,
                std::shared_ptr<ScreenStreamManager> stream,
                std::shared_ptr<DiscoveryService> discovery,
                unsigned short port)
        : stream_(std::move(stream))
        , auth_(std::move(auth))
        , discovery_(std::move(discovery))
        , port_(port)
        , socket_(std::move(socket)) {}

    void run() {
        do_read();
    }

private:
    std::shared_ptr<ScreenStreamManager> stream_;
    std::shared_ptr<AuthService> auth_;
    std::shared_ptr<DiscoveryService> discovery_;
    unsigned short port_;
    tcp::socket socket_;
    beast::flat_buffer buffer_;

    template <class Response>
    void write_response(Response&& res) {
        const bool keep = res.keep_alive();
        auto sp = std::make_shared<http::response<http::string_body>>(std::move(res));
        sp->set(http::field::access_control_allow_origin, "*");
        sp->set(http::field::access_control_allow_headers, "Content-Type, Authorization");
        auto self = shared_from_this();
        http::async_write(socket_, *sp, [self, sp, keep](beast::error_code ec, std::size_t) {
            if (ec) {
                Logger::instance().warn("HTTP write failed: " + ec.message());
            }
            if (keep) {
                self->do_read();
            } else {
                beast::error_code ignore;
                self->socket_.shutdown(tcp::socket::shutdown_send, ignore);
            }
        });
    }

    void do_read() {
        auto req = std::make_shared<http::request<http::string_body>>();
        auto self = shared_from_this();
        http::async_read(socket_, buffer_, *req, [self, req](beast::error_code ec, std::size_t) {
            if (ec == http::error::end_of_stream) {
                beast::error_code ignore;
                self->socket_.shutdown(tcp::socket::shutdown_send, ignore);
                return;
            }
            if (ec) {
                Logger::instance().warn("HTTP read failed: " + ec.message());
                return;
            }
            self->handle_request(std::move(*req));
        });
    }

    void handle_request(http::request<http::string_body>&& req) {
        const std::string target = req.target().to_string();
        Logger::instance().info("HTTP " + std::string(req.method_string()) + " " + target);

        auto send = [this](auto&& response) { write_response(std::forward<decltype(response)>(response)); };

        if (req.method() == http::verb::options) {
            http::response<http::string_body> res{http::status::ok, req.version()};
            res.set(http::field::access_control_allow_origin, "*");
            res.set(http::field::access_control_allow_headers, "Content-Type, Authorization");
            res.set(http::field::access_control_allow_methods, "GET,POST,OPTIONS");
            res.keep_alive(req.keep_alive());
            send(std::move(res));
            return;
        }

        if (req.method() != http::verb::post && req.method() != http::verb::get) {
            handle_bad_request(std::move(req), "invalid_method", send);
            return;
        }

        if (target == "/health" && req.method() == http::verb::get) {
            http::response<http::string_body> res{http::status::ok, req.version()};
            res.set(http::field::content_type, "application/json");
            res.keep_alive(req.keep_alive());
            res.body() = Json({{"ok", true}, {"service", "mmt_api"}, {"port", port_}}).dump();
            res.prepare_payload();
            send(std::move(res));
            return;
        }

        const Json body = parse_json_body(req);
        const std::string token = extract_bearer(req);

        if (target == "/api/auth/precheck" && req.method() == http::verb::post) {
            const auto username = body.value("username", std::string{});
            Json resp = auth_->precheck(username);
            http::response<http::string_body> res{http::status::ok, req.version()};
            res.set(http::field::content_type, "application/json");
            res.keep_alive(req.keep_alive());
            res.body() = resp.dump();
            res.prepare_payload();
            send(std::move(res));
            return;
        }

        if (target == "/api/auth/register" && req.method() == http::verb::post) {
            const auto username = body.value("username", std::string{});
            const auto password = body.value("password", std::string{});
            Json resp = auth_->register_user(username, password);
            http::response<http::string_body> res{http::status::ok, req.version()};
            res.set(http::field::content_type, "application/json");
            res.keep_alive(req.keep_alive());
            res.body() = resp.dump();
            res.prepare_payload();
            send(std::move(res));
            return;
        }

        if (target == "/api/auth/login" && req.method() == http::verb::post) {
            const auto username = body.value("username", std::string{});
            const auto password = body.value("password", std::string{});
            auto login_result = auth_->login(username, password);
            http::response<http::string_body> res{
                login_result ? http::status::ok : http::status::unauthorized,
                req.version()};
            res.set(http::field::content_type, "application/json");
            res.keep_alive(req.keep_alive());
            Json resp;
            if (login_result) {
                resp["token"] = login_result->token;
                resp["user"] = {{"username", login_result->user.username}, {"role", login_result->user.role}};
            } else {
                resp["error"] = "invalid_credentials";
            }
            res.body() = resp.dump();
            res.prepare_payload();
            send(std::move(res));
            return;
        }

        if (target == "/api/auth/set-password" && req.method() == http::verb::post) {
            const auto username = body.value("username", std::string{});
            const auto password = body.value("password", std::string{});
            Json resp = auth_->set_password(username, password);
            http::response<http::string_body> res{
                resp.value("ok", false) ? http::status::ok : http::status::bad_request,
                req.version()};
            res.set(http::field::content_type, "application/json");
            res.keep_alive(req.keep_alive());
            res.body() = resp.dump();
            res.prepare_payload();
            send(std::move(res));
            return;
        }

        if (target == "/api/auth/verify" && req.method() == http::verb::post) {
            std::string candidate = token.empty() ? body.value("token", std::string{}) : token;
            auto verified = auth_->verify(candidate);
            http::response<http::string_body> res{
                verified ? http::status::ok : http::status::unauthorized,
                req.version()};
            res.set(http::field::content_type, "application/json");
            res.keep_alive(req.keep_alive());
            Json resp;
            resp["ok"] = verified.has_value();
            if (verified) {
                resp["user"] = {{"username", verified->username}, {"role", verified->role}};
            }
            res.body() = resp.dump();
            res.prepare_payload();
            send(std::move(res));
            return;
        }

        if (target == "/api/audit" && req.method() == http::verb::post) {
            Logger::instance().info("AUDIT: " + req.body());
            http::response<http::string_body> res{http::status::ok, req.version()};
            res.set(http::field::content_type, "application/json");
            res.keep_alive(req.keep_alive());
            res.body() = Json({{"ok", true}}).dump();
            res.prepare_payload();
            send(std::move(res));
            return;
        }

        if (target == "/api/discover/start" && req.method() == http::verb::post) {
            const unsigned short port = static_cast<unsigned short>(body.value("port", 41000));
            const unsigned int timeout = static_cast<unsigned int>(body.value("timeoutMs", 1200));
            const std::string nonce = body.value("nonce", generate_token(6));
            Json resp = discovery_->scan(timeout, port, nonce);
            http::response<http::string_body> res{http::status::ok, req.version()};
            res.set(http::field::content_type, "application/json");
            res.keep_alive(req.keep_alive());
            res.body() = resp.dump();
            res.prepare_payload();
            send(std::move(res));
            return;
        }

        if (target == "/api/stream/start" && req.method() == http::verb::post) {
            const int duration = body.value("duration", 5);
            const int fps = body.value("fps", 5);
            Json resp = stream_->start(token.empty() ? "anon" : token, duration, fps);
            http::response<http::string_body> res{http::status::ok, req.version()};
            res.set(http::field::content_type, "application/json");
            res.keep_alive(req.keep_alive());
            res.body() = resp.dump();
            res.prepare_payload();
            send(std::move(res));
            return;
        }

        if (target == "/api/stream/stop" && req.method() == http::verb::post) {
            Json resp = stream_->stop(token.empty() ? "anon" : token);
            http::response<http::string_body> res{http::status::ok, req.version()};
            res.set(http::field::content_type, "application/json");
            res.keep_alive(req.keep_alive());
            res.body() = resp.dump();
            res.prepare_payload();
            send(std::move(res));
            return;
        }

        if (target == "/api/stream/reset" && req.method() == http::verb::post) {
            Json resp = stream_->reset(token.empty() ? "anon" : token);
            http::response<http::string_body> res{http::status::ok, req.version()};
            res.set(http::field::content_type, "application/json");
            res.keep_alive(req.keep_alive());
            res.body() = resp.dump();
            res.prepare_payload();
            send(std::move(res));
            return;
        }

        if (target == "/api/stream/cancel-all" && req.method() == http::verb::post) {
            Json resp = stream_->cancel_all(token.empty() ? "anon" : token);
            http::response<http::string_body> res{http::status::ok, req.version()};
            res.set(http::field::content_type, "application/json");
            res.keep_alive(req.keep_alive());
            res.body() = resp.dump();
            res.prepare_payload();
            send(std::move(res));
            return;
        }

        if (target == "/api/process/list") {
            ProcessManager manager;
            Json resp = manager.list_processes();
            http::response<http::string_body> res{http::status::ok, req.version()};
            res.set(http::field::content_type, "application/json");
            res.keep_alive(req.keep_alive());
            res.body() = resp.dump();
            res.prepare_payload();
            send(std::move(res));
            return;
        }

        if (target == "/api/process/start" && req.method() == http::verb::post) {
            ProcessManager manager;
            Json resp = manager.start_process(body.value("path", std::string{}));
            http::response<http::string_body> res{http::status::ok, req.version()};
            res.set(http::field::content_type, "application/json");
            res.keep_alive(req.keep_alive());
            res.body() = resp.dump();
            res.prepare_payload();
            send(std::move(res));
            return;
        }

        if (target == "/api/process/end" && req.method() == http::verb::post) {
            ProcessManager manager;
            Json resp = manager.kill_process(body.value("pid", -1));
            http::response<http::string_body> res{http::status::ok, req.version()};
            res.set(http::field::content_type, "application/json");
            res.keep_alive(req.keep_alive());
            res.body() = resp.dump();
            res.prepare_payload();
            send(std::move(res));
            return;
        }

        if (target == "/api/controller/status") {
            Json resp;
            resp["ok"] = true;
            resp["status"] = {{"status", "running"}};
            http::response<http::string_body> res{http::status::ok, req.version()};
            res.set(http::field::content_type, "application/json");
            res.keep_alive(req.keep_alive());
            res.body() = resp.dump();
            res.prepare_payload();
            send(std::move(res));
            return;
        }

        if ((target == "/api/controller/restart" || target == "/api/controller/stop") &&
            req.method() == http::verb::post) {
            Json resp;
            resp["ok"] = true;
            resp["status"] = {{"status", target == "/api/controller/restart" ? "running" : "stopped"}};
            http::response<http::string_body> res{http::status::ok, req.version()};
            res.set(http::field::content_type, "application/json");
            res.keep_alive(req.keep_alive());
            res.body() = resp.dump();
            res.prepare_payload();
            send(std::move(res));
            return;
        }

        handle_not_found(std::move(req), send);
    }
};

ApiServer::ApiServer(const std::string& address, unsigned short port, Database db)
    : ioc_(1)
    , acceptor_(ioc_)
    , address_(address)
    , port_(port)
{
    tcp::endpoint endpoint{asio::ip::make_address(address), port};
    acceptor_.open(endpoint.protocol());
    acceptor_.set_option(asio::socket_base::reuse_address(true));
    acceptor_.bind(endpoint);
    acceptor_.listen();

    auth_ = std::make_shared<AuthService>(std::move(db));
    stream_manager_ = std::make_shared<ScreenStreamManager>(ioc_);
    discovery_ = std::make_shared<DiscoveryService>(ioc_);
}

void ApiServer::run() {
    Logger::instance().info("API listening on " + address_ + ":" + std::to_string(port_));
    do_accept();
    ioc_.run();
}

void ApiServer::do_accept() {
    acceptor_.async_accept(asio::make_strand(ioc_),
        [this](beast::error_code ec, tcp::socket socket) {
            if (!ec) {
                std::make_shared<HttpSession>(std::move(socket), auth_, stream_manager_, discovery_, port_)->run();
            } else {
                Logger::instance().warn("Accept error: " + ec.message());
            }
            do_accept();
        });
}

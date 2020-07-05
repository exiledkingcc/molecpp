#pragma once

#include <deque>

#include "mole_crypto.hpp"
#include "utils.hpp"

namespace mole {

using asio::ip::tcp;

template<typename Session>
class tcp_srv {
public:
    explicit tcp_srv(uint16_t port) :
        conn_ctx_{},
        accept_ctx_{},
        acceptor_{accept_ctx_, tcp::endpoint{tcp::v4(), port}},
        con_guard_{asio::make_work_guard(conn_ctx_)} {
    }

    tcp_srv(const tcp_srv &) = delete;

    tcp_srv(tcp_srv &&) = delete;

    ~tcp_srv() = default;

    tcp_srv &operator=(const tcp_srv &) = delete;

    tcp_srv &operator=(tcp_srv &&) = delete;

    void run() {
        std::thread([this]() {
            conn_ctx_.run();
        }).detach();

        spdlog::info("listen at {}", acceptor_.local_endpoint().port());
        acceptor_.set_option(tcp::acceptor::reuse_address(true));
        _accept();
        accept_ctx_.run();
    }

private:
    void _accept() {
        auto sess = std::make_shared<Session>(conn_ctx_);
        acceptor_.async_accept(sess->socket(), [this, sess](const std::error_code &ec) {
            if (ec) {
                spdlog::error("async_accept error:{}", ec.message());
            } else {
                const auto &ep = sess->socket().remote_endpoint();
                spdlog::debug("accept from {}:{}", ep.address().to_string(), ep.port());
                conn_ctx_.post([sess]() {
                    sess->start();
                });
            }
            _accept();
        });
    }

private:
    using executor_work_guard_t = asio::executor_work_guard<asio::io_context::executor_type>;

    asio::io_context conn_ctx_;
    asio::io_context accept_ctx_;
    tcp::acceptor acceptor_;
    executor_work_guard_t con_guard_;
};


}

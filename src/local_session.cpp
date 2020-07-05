#include "local_session.hpp"

namespace mole {

local_session::local_session(asio::io_context& ctx):
    local_socket_{ctx}, remote_socket_{ctx},
    local_buff_{0}, remote_buff_{0}, local_received_{0}, remote_received_{0},
    crypto_{mole_cfg::self().key()} {
    local_tx_data_.reserve(BUFF_SIZE);
}

void local_session::start() {
    spdlog::debug("start");
    local_socket_.non_blocking(true);

    remote_connect();
}

void local_session::local_receive(size_t expected, void (local_session::*handler)()) {
    if (expected <= local_received_) {
        (this->*handler)();
        return;
    }
    auto buff = local_buff_.data() + local_received_;
    auto remain = local_buff_.size() - local_received_;
    auto self = shared_from_this();
    asio::async_read(local_socket_, asio::buffer(buff, remain), asio::transfer_at_least(expected - local_received_),
         [this, handler, self](const std::error_code& ec, size_t sz) {
             if (ec) {
                 spdlog::debug("local_receive error: {}", ec.message());
                 return;
             }
             local_received_ += sz;
             (this->*handler)();
         }
    );
}

void local_session::local_hello() {
    if (local_received_ < local_buff_[1] + 2u) {
        local_receive(local_buff_[1] + 2, &local_session::local_hello);
        return;
    }

    spdlog::debug("local_hello reply");
    const uint8_t reply[2] = {0x05u, 0x00u};
    auto self = shared_from_this();
    asio::async_write(local_socket_, asio::buffer(reply), [this, self](const std::error_code& ec, size_t) {
        if (ec) {
            spdlog::debug("local_hello write error: {}", ec.message());
            return;
        }
        local_received_ = 0;
        local_receive(10, &local_session::local_command);
    });
}

void local_session::local_command() {
    auto cmd = local_buff_[1];
    if (cmd != 0x01) {
        // fail
        spdlog::warn("only support CONNECT");
        local_reply(0x07); // Command not supported
        return;
    }
    auto addr_type = local_buff_[3];
    if (addr_type == 0x01) {
        // ipv4
        uint16_t port = static_cast<uint16_t>(local_buff_[8] << 8u) | local_buff_[9];
        spdlog::info("target: {}.{}.{}.{}:{}", local_buff_[4], local_buff_[5], local_buff_[6], local_buff_[7], port);
        target_.assign(local_buff_.data(), local_buff_.data() + local_received_);
        remote_hello();
        return;
    } else if (addr_type == 0x03) {
        // domain
        size_t dl = local_buff_[4];
        if (local_received_ < dl + 6) {
            local_receive(dl + 6, &local_session::local_command);
            return;
        }
        // domain
        const auto *dm = local_buff_.data() + 5;
        const auto *pt = local_buff_.data() + 5 + dl;
        uint16_t port = static_cast<uint16_t>(pt[0] << 8u) | pt[1];
        auto domain = std::string{dm, pt};
        spdlog::info("target: {}:{}", domain, port);
        target_.assign(local_buff_.data(), local_buff_.data() + local_received_);
        remote_hello();
    } else {
        // do NOT support
        spdlog::warn("do NOT support IPV6");
        local_reply(0x08); // Address type not supported
    }
}

void local_session::local_reply(uint8_t reply) {
    local_buff_[1] = reply;
    asio::async_write(local_socket_, asio::buffer(local_buff_, local_received_), [](const std::error_code&, size_t) {
        // ignore
    });
}

void local_session::local_stream() {
    auto self = shared_from_this();
    local_socket_.async_read_some(asio::buffer(local_buff_), [this, self](const std::error_code& ec, std::size_t sz) {
        if (ec) {
            spdlog::debug("local_stream async_read_some error: {}", ec.message());
            return;
        }
        spdlog::debug("local_socket_ async_read_some: {}", sz);
        auto pl = sz + mole_crypto::extra_size();
        local_rx_data_.resize(pl + 2);
        auto ok = crypto_.encrypt(local_buff_.data(), sz, local_rx_data_.data() + 2);
        if (!ok) {
            spdlog::error("encrypt error");
            return;
        }
        local_rx_data_[0] = pl >> 8u;
        local_rx_data_[1] = pl & 0xffu;

        asio::async_write(remote_socket_, asio::buffer(local_rx_data_), [this, self](const std::error_code& ec, std::size_t) {
            if (ec) {
                spdlog::debug("local_stream async_write error: {}", ec.message());
                return;
            }
            local_stream();
        });
    });
}


void local_session::remote_receive(size_t expected, void (local_session::*handler)()) {
    if (expected <= remote_received_) {
        (this->*handler)();
        return;
    }
    auto buff = remote_buff_.data() + remote_received_;
    auto remain = remote_buff_.size() - remote_received_;
    auto self = shared_from_this();
    asio::async_read(remote_socket_, asio::buffer(buff, remain), asio::transfer_at_least(expected - remote_received_),
         [this, handler, self](const std::error_code& ec, size_t sz) {
             if (ec) {
                 spdlog::debug("remote_receive error: {}", ec.message());
                 return;
             }
             remote_received_ += sz;
             (this->*handler)();
         }
    );
}

void local_session::remote_connect() {
    auto self = shared_from_this();
    auto&& ep = mole_cfg::self().remote_endpoint();
    remote_socket_.async_connect(ep, [this, self](const std::error_code& ec) {
        if (ec) {
            spdlog::debug("remote_connect error: {}", ec.message());
            return;
        }
        spdlog::debug("remote connected");
        remote_socket_.non_blocking(true);

        local_receive(3, &local_session::local_hello);
    });
}

void local_session::remote_hello() {
    spdlog::debug("remote_hello");
    auto nl = mole_crypto::nonce_size();
    auto pl = target_.size() + mole_crypto::extra_size();
    local_tx_data_.resize(nl + 2 + target_.size() + mole_crypto::extra_size());
    auto ok = crypto_.encrypt(target_.data(), target_.size(), local_tx_data_.data() + nl + 2);
    if (!ok) {
        spdlog::debug("encrypt failed");
        local_reply(0x01); // general SOCKS server failure
        return;
    }
    crypto_.nonce_copy_to(local_tx_data_.data());
    local_tx_data_[nl] = pl >> 8u;
    local_tx_data_[nl + 1] = pl & 0xffu;

    auto self = shared_from_this();
    asio::async_write(remote_socket_, asio::buffer(local_tx_data_), [this, self](const std::error_code& ec, std::size_t) {
        if (ec) {
            spdlog::debug("remote_hello error: {}", ec.message());
            return;
        }

        spdlog::debug("start stream");
        remote_received_ = 0;
        remote_stream();

        local_stream();
    });
}

void local_session::remote_stream() {
    if (remote_received_ == 0) {
        remote_receive(2 + mole_crypto::extra_size(), &local_session::remote_stream);
        return;
    }
    size_t pl = static_cast<size_t>(remote_buff_[0] << 8u) | remote_buff_[1];
    if (remote_received_ < pl + 2) {
        remote_receive(pl + 2, &local_session::remote_stream);
        return;
    }
    local_tx_data_.resize(pl - mole_crypto::extra_size());
    auto ok = crypto_.decrypt(remote_buff_.data() + 2, pl, local_tx_data_.data());
    if (!ok) {
        spdlog::error("decrypt error");
        return;
    }

    remote_received_ -= (pl + 2);
    if (remote_received_ > 0) {
        std::memmove(remote_buff_.data(), remote_buff_.data() + pl + 2, remote_received_);
    }

    auto self = shared_from_this();
    asio::async_write(local_socket_, asio::buffer(local_tx_data_), [this, self](const std::error_code& ec, std::size_t sz) {
        if (ec) {
            spdlog::debug("remote_stream async_write error: {}", ec.message());
            return;
        }
        spdlog::debug("local_socket_ async_write: {}", sz);
        remote_stream();
    });
}

}

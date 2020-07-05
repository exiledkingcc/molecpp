#include "remote_session.hpp"

namespace mole {

remote_session::remote_session(asio::io_context& ctx):
    local_socket_{ctx}, target_socket_{ctx}, target_resolver_{ctx},
    local_buff_{}, target_buff_{}, local_received_{0},
    crypto_{mole_cfg::self().key()} {
    local_rx_data_.reserve(BUFF_SIZE);
    local_tx_data_.reserve(BUFF_SIZE);
}

void remote_session::start() {
    spdlog::debug("start");
    local_socket_.non_blocking(true);
    local_hello();
}

void remote_session::local_receive(size_t expected, void (remote_session::*handler)()) {
    if (expected <= local_received_) {
        (this->*handler)();
        return;
    }
    auto buff = local_buff_.data() + local_received_;
    auto remain = BUFF_SIZE - local_received_;
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

void remote_session::local_hello() {
    auto nl = mole_crypto::nonce_size();
    if (local_received_ < nl + 2) {
        local_receive(nl + 2, &remote_session::local_hello);
        return;
    }
    auto pl = static_cast<size_t>(local_buff_[nl] << 8u) | local_buff_[nl + 1];
    if (local_received_ < pl + nl + 2) {
        local_receive(pl + nl + 2, &remote_session::local_command);
        return;
    }
    local_command();
}

void remote_session::local_command() {
    auto nl = mole_crypto::nonce_size();
    size_t pl = static_cast<size_t>(local_buff_[nl] << 8u) | local_buff_[nl + 1];
    crypto_.nonce(local_buff_.data());
    local_rx_data_.resize(pl - mole_crypto::extra_size());
    auto ok = crypto_.decrypt(local_buff_.data() + nl + 2, pl, local_rx_data_.data());
    if (!ok) {
        spdlog::debug("decrypt error");
        return;
    }

    auto addr_type = local_rx_data_[3];
    if (addr_type == 0x01) {
        // ipv4
        uint16_t port = static_cast<uint16_t>(local_rx_data_[8] << 8u) | local_rx_data_[9];
        spdlog::info("target: {}.{}.{}.{}:{}", local_rx_data_[4], local_rx_data_[5], local_rx_data_[6], local_rx_data_[7], port);
        auto ip = asio::ip::address_v4{{local_rx_data_[4], local_rx_data_[5], local_rx_data_[6], local_rx_data_[7]}};
        target_connect(tcp::resolver::results_type::create({ip, port}, "", ""));
        return;
    } else if (addr_type == 0x03) {
        // domain
        size_t dl = local_rx_data_[4];
        const auto *dm = local_rx_data_.data() + 5;
        const auto *pt = local_rx_data_.data() + 5 + dl;
        uint16_t port = static_cast<uint16_t>(pt[0] << 8u) | pt[1];
        auto domain = std::string{dm, pt};
        spdlog::info("target: {}:{}", domain, port);
        target_resolve(std::move(domain), port);
    } else {
        spdlog::warn("type 0x{:02x} NOT support", addr_type);
        local_reply(0x08); // Address type not supported
        return;
    }
}

void remote_session::local_reply(uint8_t reply) {
    local_rx_data_[1] = reply;
    auto pl = local_rx_data_.size() + mole_crypto::extra_size();
    local_tx_data_.resize(pl + 2);
    crypto_.encrypt(local_rx_data_.data(), local_rx_data_.size(), local_tx_data_.data() + 2);
    local_tx_data_[0] = pl >> 8u;
    local_tx_data_[1] = pl & 0xffu;
    auto self = shared_from_this();
    asio::async_write(local_socket_, asio::buffer(local_tx_data_), [this, self](const std::error_code& ec, std::size_t) {
        if (ec) {
            spdlog::debug("local_reply error: {}", ec.message());
            return;
        }

        spdlog::debug("stream");

        local_received_ = 0;
        local_stream();

        target_stream();
    });
}


void remote_session::local_stream() {
    if (local_received_ == 0) {
        local_receive(2 + mole_crypto::extra_size(), &remote_session::local_stream);
        return;
    }
    size_t pl = static_cast<size_t>(local_buff_[0] << 8u) | local_buff_[1];
    if (local_received_ < pl + 2) {
        local_receive(pl + 2, &remote_session::local_stream);
        return;
    }
    local_rx_data_.resize(pl - mole_crypto::extra_size());
    auto ok = crypto_.decrypt(local_buff_.data() + 2, pl, local_rx_data_.data());
    if (!ok) {
        spdlog::error("decrypt error");
        return;
    }

    local_received_ -= (pl + 2);
    if (local_received_ > 0) {
        std::memmove(local_buff_.data(), local_buff_.data() + pl + 2, local_received_);
    }

    auto self = shared_from_this();
    asio::async_write(target_socket_, asio::buffer(local_rx_data_), [this, self](const std::error_code& ec, std::size_t sz) {
        if (ec) {
            spdlog::debug("local_stream async_write error: {}", ec.message());
            return;
        }
        spdlog::debug("target_socket_ async_write: {}", sz);
        local_stream();
    });
}

void remote_session::target_resolve(std::string&& domain, uint16_t port) {
    auto self = shared_from_this();
    target_resolver_.async_resolve(domain, std::to_string(port),
        [this, self](const std::error_code& ec, const tcp::resolver::results_type& endpoints) {
        if (ec) {
            spdlog::debug("target_resolve error: {}", ec.message());
            local_reply(0x04); // Host unreachable
            return;
        }
#if MOLE_DEBUG
        for (auto&& ep: endpoints) {
            auto&& x = ep.endpoint();
            spdlog::debug("resolve result: {}", x.address().to_string(), x.port());
        }
#endif
        target_connect(endpoints);
    });
}

void remote_session::target_connect(const tcp::resolver::results_type& endpoints) {
    auto self = shared_from_this();
    asio::async_connect(target_socket_, endpoints, [this, self](const std::error_code& ec, const tcp::endpoint&){
        if (ec) {
            spdlog::debug("target_connect error: {}", ec.message());
            local_reply(0x03); // Network unreachable
            return;
        }
        spdlog::debug("target connected");
        target_socket_.non_blocking(true);
        local_reply(0x00); // succeeded
    });
}

void remote_session::target_stream() {
    auto self = shared_from_this();
    target_socket_.async_read_some(asio::buffer(target_buff_), [this, self](const std::error_code& ec, std::size_t sz) {
        if (ec) {
            spdlog::debug("target_stream async_read_some error: {}", ec.message());
            return;
        }
        spdlog::debug("target_socket_ async_read_some: {}", sz);
        auto pl = sz + mole_crypto::extra_size();
        local_tx_data_.resize(pl + 2);
        auto ok = crypto_.encrypt(target_buff_.data(), sz, local_tx_data_.data() + 2);
        if (!ok) {
            spdlog::error("encrypt error");
            return;
        }
        local_tx_data_[0] = pl >> 8u;
        local_tx_data_[1] = pl & 0xffu;

        asio::async_write(local_socket_, asio::buffer(local_tx_data_), [this, self](const std::error_code& ec, std::size_t) {
            if (ec) {
                spdlog::debug("target_stream async_write error: {}", ec.message());
                return;
            }
            target_stream();
        });
    });
}

}

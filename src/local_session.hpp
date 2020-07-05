#pragma once

#include "mole_crypto.hpp"
#include "utils.hpp"

namespace mole {

using asio::ip::tcp;

class local_session: public std::enable_shared_from_this<local_session> {
public:
    explicit local_session(asio::io_context& ctx);
    ~local_session() = default;

    local_session(const local_session&) = delete;
    local_session(local_session&&) = delete;

    tcp::socket& socket() {
        return local_socket_;
    }

    void start();

private:
    void local_receive(size_t expected, void (local_session::*handler)());

    void local_hello();
    void local_command();
    void local_reply(uint8_t reply);
    void local_stream();

    void remote_receive(size_t expected, void (local_session::*handler)());

    void remote_connect();
    void remote_hello();
    void remote_stream();

private:
    asio::io_context& io_ctx_;
    tcp::socket local_socket_;
    tcp::socket remote_socket_;

    static constexpr size_t BUFF_SIZE = 8192;

    std::array<uint8_t, BUFF_SIZE> local_buff_;
    std::array<uint8_t, BUFF_SIZE * 2> remote_buff_;

    size_t local_received_;
    size_t remote_received_;

    std::vector<uint8_t> local_rx_data_;
    std::vector<uint8_t> local_tx_data_;
    std::vector<uint8_t> target_;

    mole_crypto crypto_;
};

}

#pragma once

#include "mole_crypto.hpp"
#include "utils.hpp"

namespace mole {

using asio::ip::tcp;

class remote_session: public std::enable_shared_from_this<remote_session> {
public:
    explicit remote_session(asio::io_context& ctx);
    ~remote_session() = default;

    remote_session(const remote_session&) = delete;
    remote_session(remote_session&&) = delete;

    tcp::socket& socket() {
        return local_socket_;
    }

    void start();

private:
    void local_receive(size_t expected, void (remote_session::*handler)());

    void local_hello();
    void local_command();
    void local_reply(uint8_t reply);
    void local_stream();

    void target_resolve(std::string&& domain, uint16_t port);
    void target_connect(const tcp::resolver::results_type& endpoints);
    void target_stream();

private:
    tcp::socket local_socket_;
    tcp::socket target_socket_;
    tcp::resolver target_resolver_;

    static constexpr size_t BUFF_SIZE = 1024 * 32;

    std::array<uint8_t, BUFF_SIZE> local_buff_;
    std::array<uint8_t, BUFF_SIZE> target_buff_;

    std::vector<uint8_t> local_rx_data_;
    std::vector<uint8_t> local_tx_data_;

    size_t local_received_;

    mole_crypto crypto_;
};

}

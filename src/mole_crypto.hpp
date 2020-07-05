#pragma once

#include <string>
#include <vector>

namespace mole {

class mole_crypto {
public:
    explicit mole_crypto(const std::string& key);
    ~mole_crypto() = default;

    mole_crypto(const mole_crypto&) = default;
    mole_crypto(mole_crypto&&) = default;

public:
    bool encrypt(const uint8_t *data, std::size_t len, uint8_t *out);
    bool decrypt(const uint8_t *data, std::size_t len, uint8_t *out);

    void nonce_copy_to(uint8_t *dd) const;
    void nonce(const uint8_t *dd);

    static size_t nonce_size();
    static size_t extra_size();
    static std::vector<uint8_t> make_key(const std::string& key);

private:
    const std::vector<uint8_t> key_;
    std::vector<uint8_t> nonce_;
};

}

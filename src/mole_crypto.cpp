#include <sodium.h>

#include <cstring>
#include "mole_crypto.hpp"

namespace mole {

mole_crypto::mole_crypto(const std::string& key):key_{make_key(key)} {
    nonce_.resize(nonce_size());
    randombytes_buf(nonce_.data(), nonce_.size());
}

bool mole_crypto::encrypt(const uint8_t *data, std::size_t len, uint8_t *out) {
    auto ok = crypto_aead_xchacha20poly1305_ietf_encrypt(
        out, nullptr, data, len, nullptr, 0, nullptr, nonce_.data(), key_.data());
    return ok == 0;
}

bool mole_crypto::decrypt(const uint8_t *data, std::size_t len, uint8_t *out) {
    auto ok = crypto_aead_xchacha20poly1305_ietf_decrypt(
        out, nullptr, nullptr, data, len, nullptr, 0, nonce_.data(), key_.data());
    return ok == 0;
}


void mole_crypto::nonce_copy_to(uint8_t *dd) const {
    std::memcpy(dd, nonce_.data(), nonce_.size());
}

void mole_crypto::nonce(const uint8_t *dd) {
    nonce_.assign(dd, dd + nonce_size());
}

size_t mole_crypto::nonce_size() {
    return crypto_aead_xchacha20poly1305_ietf_npubbytes();
}

size_t mole_crypto::extra_size() {
    return crypto_aead_xchacha20poly1305_ietf_abytes();
}

std::vector<uint8_t> mole_crypto::make_key(const std::string &key) {
    std::vector<uint8_t> key_(crypto_hash_sha256_bytes());
    auto *dd = reinterpret_cast<const uint8_t*>(key.data());
    crypto_hash_sha256(key_.data(), dd, key.size());
    key_.resize(crypto_aead_xchacha20poly1305_ietf_keybytes());
    return key_;
}


}

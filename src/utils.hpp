#pragma once

#define ASIO_STANDALONE
#include "asio.hpp"
#include "spdlog/spdlog.h"

#ifdef __declare_ref__
#undef __declare_ref__
#endif

#define __declare_ref__(TYPE, NAME) \
public:\
    const TYPE& NAME() const {\
        return NAME##_;\
    }\
    auto& NAME(const TYPE& val) {\
        NAME##_ = val;\
        return *this;\
    }\
private:\
    TYPE NAME##_;


#ifdef __declare_val__
#undef __declare_val__
#endif

#define __declare_val__(TYPE, NAME) \
public:\
    TYPE NAME() const {\
        return NAME##_;\
    }\
    auto& NAME(TYPE val) {\
        NAME##_ = val;\
        return *this;\
    }\
private:\
    TYPE NAME##_;


namespace mole {

void logging_init(const char *name, bool dev = false);

std::vector<std::string> split(const std::string& ss, char c);
inline std::vector<std::string> split(const std::string& ss) {
    return split(ss, ' ');
}

struct mole_cfg {
    static mole_cfg& self();

    mole_cfg(const mole_cfg&) = delete;
    mole_cfg(mole_cfg&&) = delete;

    __declare_ref__(std::string, key)
    __declare_ref__(asio::ip::tcp::endpoint, remote_endpoint)

    __declare_val__(uint16_t, port)
    __declare_val__(bool, dev)

private:
    explicit mole_cfg();
};

}

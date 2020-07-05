#pragma once

#include <string>

#define ASIO_STANDALONE
#include "asio.hpp"
#include "spdlog/spdlog.h"

#define __declare_ref__(TYPE, NAME) \
public:\
    const TYPE& NAME() const {\
        return _##NAME;\
    }\
    auto& NAME(const TYPE& val) {\
        _##NAME = val;\
        return *this;\
    }\
private:\
    TYPE _##NAME;

#define __declare_val__(TYPE, NAME) \
public:\
    TYPE NAME() const {\
        return _##NAME;\
    }\
    auto& NAME(TYPE val) {\
        _##NAME = val;\
        return *this;\
    }\
private:\
    TYPE _##NAME;

namespace mole {

void logging_init(const char *name, bool dev = false);

class mole_cfg {
public:
    static mole_cfg& self();

    mole_cfg(const mole_cfg&) = delete;
    mole_cfg(mole_cfg&&) = delete;

private:
    explicit mole_cfg() = default;

__declare_ref__(std::string, key)

__declare_val__(bool, dev)

};

}

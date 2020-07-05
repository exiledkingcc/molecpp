#include "spdlog/sinks/stdout_color_sinks.h"

#include "utils.hpp"

namespace mole {

void logging_init(const char *name, bool dev) {
    auto logger = spdlog::stdout_color_mt(name, spdlog::color_mode::always);
    logger->set_pattern("%^%Y-%m-%d %H:%M:%S.%e [%P] [%t] [%n] [%l] %v%$");
    if (dev) {
        logger->set_level(spdlog::level::debug);
    } else {
        logger->set_level(spdlog::level::info);
    }
    spdlog::set_default_logger(logger);
}

std::vector<std::string> split(const std::string& ss, char c) {
    std::vector<std::string> rr;
    if (ss.empty()) {
        return rr;
    }
    auto p = ss.find_first_not_of(c, 0);
    auto q = ss.find_first_of(c, p);
    while (p != std::string::npos || q != std::string::npos) {
        rr.emplace_back(ss.substr(p, q));
        p = ss.find_first_not_of(c, q);
        q = ss.find_first_of(c, p);
    }
    return rr;
}

mole_cfg& mole_cfg::self() {
    static mole_cfg cfg;
    return cfg;
}


}
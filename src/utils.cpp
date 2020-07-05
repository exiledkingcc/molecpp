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

mole_cfg& mole_cfg::self() {
    static mole_cfg c;
    return c;
}


}
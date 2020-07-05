#include "local_session.hpp"
#include "remote_session.hpp"
#include "tcp_srv.hpp"
#include "utils.hpp"

#include "cxxopts.hpp"
//#pragma GCC diagnostic push
//#pragma GCC diagnostic ignored "-Wmismatched-tags"
//#include "nlohmann/json.hpp"
//#pragma GCC diagnostic pop

constexpr const char* name = "mole";

int main(int argc, char** argv) {
    cxxopts::Options options(name, "a simple proxy");
    options.allow_unrecognised_options();
    options.add_options()("help", "show help");
    options.add_options()("d,dev", "dev mode");
    options.add_options()("m,mode", "mode", cxxopts::value<std::string>(), "local/remote");
    options.add_options()("k,key", "key for crypto", cxxopts::value<std::string>(), "key");
    options.add_options()("c,mole_cfg", "mole_cfg file", cxxopts::value<std::string>(), "mole_cfg.json");
    auto args = options.parse(argc, argv);

    if (args.count("help") > 0) {
        std::cout << options.help({}) << std::endl;
        return 0;
    }

    std::string mode;
    if (args.count("mode") > 0) {
        mode = args["mode"].as<std::string>();
    }
    if (mode != "local" && mode != "remote") {
        std::cout << options.help({}) << std::endl;
        return 0;
    }

    auto& cfg = mole::mole_cfg::self();
    cfg.dev(args.count("dev") > 0);
    if (args.count("key") > 0) {
        cfg.key(args["key"].as<std::string>());
    }

    if (cfg.key().empty()) {
        std::cerr << "key is required!" << std::endl;
        return 0;
    }

    mole::logging_init(name, cfg.dev());
    spdlog::info("start {}, mode: {}, dev={}", name, mode, cfg.dev());

    cfg.key();

    if (mode == "local") {
        auto srv = mole::tcp_srv<mole::local_session>(6666u);
        srv.run();
    } else {
        auto srv = mole::tcp_srv<mole::remote_session>(7777u);
        srv.run();
    }

    return 0;
}

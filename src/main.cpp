#include "local_session.hpp"
#include "remote_session.hpp"
#include "tcp_srv.hpp"
#include "utils.hpp"

#include "cxxopts.hpp"

constexpr const char* name = "mole";

asio::ip::tcp::endpoint parse_remote(const std::string& remote);

int main(int argc, char** argv) {
    cxxopts::Options options(name, "a simple proxy");
    options.allow_unrecognised_options();
    options.add_options()("help", "show help");
    options.add_options()("d,dev", "dev mode");
    options.add_options()("m,mode", "mode", cxxopts::value<std::string>(), "local/remote");
    options.add_options()("r,remote", "remote address", cxxopts::value<std::string>(), "remote");
    options.add_options()("k,key", "key for crypto", cxxopts::value<std::string>(), "key");
    options.add_options()("p,port", "listen port", cxxopts::value<uint16_t>()->default_value("20903"), "port");
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
    cfg.port(args["port"].as<uint16_t>());
    if (args.count("key") > 0) {
        cfg.key(args["key"].as<std::string>());
    }
    if (args.count("remote") > 0) {
        auto ep = parse_remote(args["remote"].as<std::string>());
        cfg.remote_endpoint(ep);
    }

    if (cfg.key().empty()) {
        std::cerr << "ERROR: key is required!"<< std::endl;
        return 0;
    }
    if (mode == "local" && cfg.remote_endpoint().address().is_unspecified()) {
        std::cerr << "ERROR: remote is required!"<< std::endl;
        return 0;
    }

    mole::logging_init(name, cfg.dev());
    spdlog::info("start {}, mode: {}, dev={}", name, mode, cfg.dev());

    if (mode == "local") {
        auto&& ep = cfg.remote_endpoint();
        spdlog::info("remote: {}:{}", ep.address().to_string(), ep.port());
        auto srv = mole::tcp_srv<mole::local_session>(cfg.port());
        srv.run();
    } else {
        auto srv = mole::tcp_srv<mole::remote_session>(cfg.port());
        srv.run();
    }

    return 0;
}


asio::ip::tcp::endpoint parse_remote(const std::string& remote) {
    auto ss = mole::split(remote, ':');
    if (ss.size() != 2) {
        return {};
    }
    auto ip = asio::ip::make_address(ss[0]);
    auto port = static_cast<uint16_t>(std::stoul(ss[1]));
    return {ip, port};
}
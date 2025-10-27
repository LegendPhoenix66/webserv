#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <vector>
#include <sstream>
#include "../inc/ConfigParser.hpp"
#include "../inc/Config.hpp"
#include "../inc/Listener.hpp"
#include "../inc/EventLoop.hpp"
#include "../inc/Logger.hpp"
#include "../inc/SignalHandler.hpp"

static void print_usage() {
    std::cout << "Usage: webserv [options] [config_file]\n"
                 "Options:\n"
                 "  --help       Show this help and exit\n"
                 "  --version    Show version and exit\n";
}

int main(int argc, char **argv) {
    const std::string defaultConfig = "conf_files/v0_min.conf";
    std::string configPath;

    if (argc == 1) {
        configPath = defaultConfig;
    } else if (argc == 2) {
        std::string arg = argv[1];
        if (arg == std::string("--help")) {
            print_usage();
            return 0;
        } else if (arg == std::string("--version")) {
            std::cout << "webserv " << WEBSERV_VERSION << "\n";
            return 0;
        } else {
            configPath = arg;
        }
    } else {
        std::cerr << "error: too many arguments\n";
        print_usage();
        return 2; // CLI error
    }

    try {
        // Initialise logging early
        Logger::init("logs/access.log", "logs/error.log");
        Logger::setLevel(LOG_INFO);
        (void)SignalHandler::install();
        std::cout << "webserv " << WEBSERV_VERSION << " — running (multi-listen event loop)\n";
        std::cout << "Loading config: " << configPath << "\n";
        ConfigParser parser;
        Config cfg = parser.parseFile(configPath);
        if (cfg.servers.empty()) {
            std::cerr << "error: no servers parsed\n";
            return 3;
        }

        // Build bind groups: key -> indices of servers (order preserved; first is default)
        std::map<std::string, std::vector<size_t> > binds;
        for (size_t i = 0; i < cfg.servers.size(); ++i) {
            const ServerConfig &sc = cfg.servers[i];
            std::string key = bindKeyOf(sc);
            binds[key].push_back(i);
        }

        // Start listeners
        std::vector<Listener*> listeners;
        listeners.reserve(binds.size());
        EventLoop loop;
        std::string emsg;

        for (std::map<std::string, std::vector<size_t> >::const_iterator it = binds.begin(); it != binds.end(); ++it) {
            const std::string &key = it->first;
            const std::vector<size_t> &idxs = it->second;
            if (idxs.empty()) continue;

            Listener *lst = new Listener();
            const ServerConfig &sc0 = cfg.servers[idxs[0]]; // default for this bind
            if (!lst->start(sc0, &emsg)) {
                std::cerr << "startup error on " << key << ": " << emsg << "\n";
                // cleanup
                for (size_t k = 0; k < listeners.size(); ++k) { delete listeners[k]; }
                return 2;
            }
            listeners.push_back(lst);

            // Build group of pointers for this bind
            std::vector<const ServerConfig*> group;
            for (size_t j = 0; j < idxs.size(); ++j) {
                group.push_back(&cfg.servers[idxs[j]]);
            }

            if (!loop.addListen(lst->fd(), key, group, &emsg)) {
                std::cerr << "eventloop setup error for " << key << ": " << emsg << "\n";
                for (size_t k = 0; k < listeners.size(); ++k) { delete listeners[k]; }
                return 2;
            }
            std::cout << "listening at " << lst->boundAddress() << " (group size=" << group.size() << ")\n";
        }

        int rc = loop.run();
        for (size_t k = 0; k < listeners.size(); ++k) { delete listeners[k]; }
        SignalHandler::uninstall();
        Logger::shutdown();
        return rc; // 0 on clean stop, 2 on fatal
    } catch (const ConfigError &e) {
        if (e.kind() == IO_ERROR) {
            std::cerr << e.file() << ": error: " << e.what() << "\n";
            return 2; // CLI / IO error
        }
        std::cerr << e.file() << ":" << e.line() << ":" << e.col() << ": error: " << e.what() << "\n";
        if (e.kind() == SYNTAX_ERROR) return 3;
        if (e.kind() == VALIDATION_ERROR) return 4;
        return 2;
    } catch (const std::exception &e) {
        std::cerr << "fatal: " << e.what() << "\n";
        return 2;
    }
}

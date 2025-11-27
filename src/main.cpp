#include "../inc/WebServ.hpp"
#include "../inc/ParseConfig.hpp"
#include "../inc/Logger.hpp"
#include "../inc/SignalHandler.hpp"
#include "../inc/Listener.hpp"
#include "../inc/EventLoop.hpp"

static void print_usage() {
	std::cout << "Usage: webserv [options] [config_file]\n"
				 "Options:\n"
				 "  --help       Show this help and exit\n";
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
		} else {
			configPath = arg;
		}
	} else {
		std::cerr << "error: too many arguments\n";
		print_usage();
		return 2; // CLI error
	}

	try {
		Logger::init("logs/access.log", "logs/error.log");
		Logger::setLevel(LOG_INFO);
		SignalHandler	signal;
		std::cout << "webserv — running (multi-listen event loop)" << std::endl;
		std::cout << "Loading config: " << configPath << std::endl;
		ParseConfig					parser(configPath);
		std::vector<ServerConfig>	configs = parser.getConfigs();
		if (configs.empty()) {
			std::cerr << "error: no servers parsed" << std::endl;
			return 3;
		}

		// Build bind groups: key -> indices of servers (order preserved; first is default)
		std::map<std::string, std::vector<size_t> >	binds;
		for (size_t i = 0; i < configs.size(); i++) {
			ServerConfig	&sc = configs[i];
			std::string		key = sc.bindKey();
			binds[key].push_back(i);
		}

		// Start listeners
		std::vector<Listener*>	listeners;
		listeners.reserve(binds.size());
		EventLoop	loop;
		std::string	emsg;

		for (std::map<std::string, std::vector<size_t> >::const_iterator it = binds.begin(); it != binds.end(); ++it) {
			const std::string &key = it->first;
			const std::vector<size_t> &idxs = it->second;
			if (idxs.empty()) continue;

			Listener *lst = new Listener();
			const ServerConfig &sc0 = configs[idxs[0]]; // default for this bind
			if (!lst->start(sc0, &emsg)) {
				std::cerr << "startup error on " << key << ": " << emsg << "\n";
				// cleanup
				for (size_t k = 0; k < listeners.size(); ++k) { delete listeners[k]; }
				delete lst;
				return 2;
			}
			listeners.push_back(lst);

			// Build group of pointers for this bind
			std::vector<const ServerConfig*> group;
			for (size_t j = 0; j < idxs.size(); ++j) {
				group.push_back(&configs[idxs[j]]);
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
		Logger::shutdown();
		return rc;
	}
	catch (std::exception &e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return 2;
	}

	return (0);
}

#include "../inc/WebServ.hpp"
#include "../inc/ParseConfig.hpp"
#include "../inc/Server.hpp"

static void print_usage() {
	std::cout << "Usage: webserv [options] [config_file]\n"
				 "Options:\n"
				 "  --help       Show this help and exit\n"
				 "  --version    Show version and exit\n";
}

std::vector<ServerConfig>	init_config(char *path) {
	return ParseConfig(path).getConfigs();
}

void start_server(const std::vector<ServerConfig> &configs) {
	std::vector <Server> servers;
	servers.reserve(configs.size());
	for (size_t i = 0; i < configs.size(); ++i) {
		servers.push_back(Server(configs[i]));
	}
	for (size_t i = 0; i < servers.size(); ++i) {
		servers[i].start();
	}
	// TODO: Integrate with poll/select/epoll for event loop
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


		std::cout << "webserv — running (multi-listen event loop)" << std::endl;
		std::cout << "Loading config: " << configPath << std::endl;
		ParseConfig					parser(configPath);
		std::vector<ServerConfig>	configs = parser.getConfigs();
		start_server(configs);
	}
	catch (std::exception &e) {
		std::cerr << "Error: " << e.what() << std::endl;
	}

	return (0);
}

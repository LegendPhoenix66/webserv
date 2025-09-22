#include "../inc/WebServ.hpp"
#include "../inc/ParseConfig.hpp"
#include "../inc/Server.hpp"
#include "../inc/EventLoop.hpp"

static const char* DEFAULT_CONFIG_PATH = "conf_files/simple_static.conf";

int check_args(int argc) {
	// No error when no config is specified; we'll use a default path
	(void)argc;
	return 0;
}

std::vector<ServerConfig>	init_config(const char *path) {
	return ParseConfig(const_cast<char*>(path)).getConfigs();
}

int main(int argc, char **argv) {
	const char* config_path = (argc >= 2 && argv[1] && argv[1][0] != '\0') ? argv[1] : DEFAULT_CONFIG_PATH;

	try {
		std::vector<ServerConfig> configs = init_config(config_path);
		/*for (size_t i = 0; i < configs.size(); i++)
			print_conf(configs[i], i + 1);*/

		// Build servers and register their listeners into a unified EventLoop
		std::vector<Server> servers;
		servers.reserve(configs.size());
		for (size_t i = 0; i < configs.size(); ++i) {
			servers.push_back(Server(configs[i]));
		}

		// Create EventLoop and register listeners
		EventLoop loop;
		for (size_t i = 0; i < servers.size(); ++i) {
			int fd = servers[i].setupListenSocket();
			if (fd >= 0) {
				loop.addListener(fd, &servers[i].getConfig());
			} else {
				std::cerr << "Failed to set up listener on server index " << i << std::endl;
			}
		}
		loop.run();
	}
	catch (std::exception &e) {
		std::cerr << "Error: " << e.what() << std::endl;
	}

	return (0);
}

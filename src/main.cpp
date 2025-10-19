#include "../inc/WebServ.hpp"
#include "../inc/ParseConfig.hpp"
#include "../inc/Server.hpp"

int check_args(int argc) {
	if (argc < 2) {
		std::cerr << "Error: No config file specified.\nUsage: ./webserv <config_file>" << std::endl;
		return 1;
	}
	return 0;
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
	if (check_args(argc)) return 1;

	try {
		std::vector<ServerConfig> configs = init_config(argv[1]);
		start_server(configs);
	}
	catch (std::exception &e) {
		std::cerr << "Error: " << e.what() << std::endl;
	}

	return (0);
}

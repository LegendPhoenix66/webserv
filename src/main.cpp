#include "../inc/WebServ.hpp"
#include "../inc/ParseConfig.hpp"
#include "../inc/Server.hpp"

void print_conf(const ServerConfig &conf, size_t n) {
	std::cout << "Server Configuration " << n << ":" << std::endl;
	std::cout << "Port: " << conf.getPort() << std::endl;
	std::cout << "Server Name: " << conf.getServerName() << std::endl;
	std::cout << "Host: " << conf.getHost() << std::endl;
	std::cout << "Root: " << conf.getRoot() << std::endl;
	std::cout << "Index: ";
	for (size_t i = 0; i < conf.getIndex().size(); i++)
		std::cout << conf.getIndex()[i] << " ";
	std::cout << std::endl;
	std::cout << "Client Max Body Size: " << conf.getClientMaxBodySize() << " bytes" << std::endl;
	const std::vector<Location> locations = conf.getLocations();
	std::cout << "Locations: " << std::endl;
	for (size_t i = 0; i < locations.size(); i++) {
		const Location &loc = locations[i];
		std::cout << "  Path: " << loc.getPath() << std::endl;
		if (!loc.getRoot().empty())
			std::cout << "  Root: " << loc.getRoot() << std::endl;
		if (!loc.getCgiPass().empty())
			std::cout << "  CGI Pass: " << loc.getCgiPass() << std::endl;
		if (!loc.getCgiPath().empty())
			std::cout << "  CGI Path: " << loc.getCgiPath() << std::endl;
		std::cout << "  Autoindex: " << (loc.getAutoindex() ? "on" : "off") << std::endl;
		if (!loc.getIndex().empty()) {
			std::cout << "  Index: ";
			for (size_t j = 0; j < loc.getIndex().size(); j++)
				std::cout << loc.getIndex()[j] << " ";
			std::cout << std::endl;
		}
		if (!loc.getAllowedMethods().empty()) {
			std::cout << "  Allowed Methods: ";
			for (size_t j = 0; j < loc.getAllowedMethods().size(); j++)
				std::cout << loc.getAllowedMethods()[j] << " ";
			std::cout << std::endl;
		}
		if (loc.getReturnDir().first != 0 || !loc.getReturnDir().second.empty())
			std::cout << "  Return Directory: ";
		if (loc.getReturnDir().first != 0)
			std::cout << loc.getReturnDir().first << " ";
		if (!loc.getReturnDir().second.empty())
			std::cout << loc.getReturnDir().second;
		if (loc.getReturnDir().first != 0 || !loc.getReturnDir().second.empty())
			std::cout << std::endl;
		if (!loc.getUploadStore().empty())
			std::cout << "  Upload Store: " << loc.getUploadStore() << std::endl;
		if (loc.getClientMaxBodySize() != 0)
			std::cout << "  Client Max Body Size: " << loc.getClientMaxBodySize() << " bytes" << std::endl;
		std::cout << "----------------------------------------" << std::endl;
	}
	const std::map<int, std::string> error_pages = conf.getErrorPages();
	std::cout << "Error Pages: " << std::endl;
	for (std::map<int, std::string>::const_iterator it = error_pages.begin(); it != error_pages.end(); ++it) {
		if (!it->second.empty())
			std::cout << "  " << it->first << ": " << it->second << std::endl;
	}
	std::cout << "----------------------------------------" << std::endl;
}

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
		for (size_t i = 0; i < configs.size(); i++)
			print_conf(configs[i], i + 1);
		start_server(configs);
	}
	catch (std::exception &e) {
		std::cerr << "Error: " << e.what() << std::endl;
	}

	return (0);
}

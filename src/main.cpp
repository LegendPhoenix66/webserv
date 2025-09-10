#include "../inc/WebServ.hpp"
#include "../inc/ParseConfig.hpp"
#include "../inc/Server.hpp"
#include "../inc/EventLoop.hpp"

/*void print_conf(const ServerConfig &conf, size_t n) {
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
}*/

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

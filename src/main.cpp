#include "../inc/WebServ.hpp"
#include "../inc/ParseConf.hpp"

void	print_conf(const ServerConf &conf)
{
	std::cout << "Server Configuration:" << std::endl;
	std::cout << "Port: " << conf.getPort() << std::endl;
	std::cout << "Server Name: " << conf.getServerName() << std::endl;
	std::cout << "Host: " << conf.getHost() << std::endl;
	std::cout << "Root: " << conf.getRoot() << std::endl;
}

int	main(int argc, char **argv)
{
	(void)argc;

	try {
		ParseConf	parse(argv[1]);
		ServerConf	conf = parse.getConf();
		print_conf(conf);
	}
	catch (std::exception &e) {
		std::cerr << "Error: " << e.what() << std::endl;
	}

	return (0);
}

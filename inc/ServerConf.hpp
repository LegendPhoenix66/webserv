#ifndef SERVERCONF_HPP
#define SERVERCONF_HPP

#include "WebServ.hpp"
#include "Location.hpp"

class ParseConf;

class ServerConf
{
private:
	uint16_t					port;
	std::string					server_name;
	std::string					host;
	std::string					root;
	std::vector<std::string>	index;
	std::vector<Location>		locations;
	std::map<int, std::string>	error_pages;
	size_t						client_max_body_size;

	void    swap(ServerConf &other);
public:
	ServerConf();
	ServerConf(const ServerConf &copy);
	ServerConf  &operator=(const ServerConf &copy);
	~ServerConf();

	void	setPort(uint16_t port);
	void	setServerName(std::string name);
	void	setHost(std::string host);
	void	setRoot(std::string root);
	void	addIndexBack(std::string index);
	void	addLocationBack(Location loc);
	void	addErrorPageBack(int code, std::string url);

	uint16_t					getPort() const { return port; }
	std::string					getServerName() const { return server_name; }
	std::string					getHost() const { return host; }
	std::string					getRoot() const { return root; }
	std::vector<std::string>	getIndex() const { return index; }
	std::vector<Location>		getLocations() const { return locations; }
	std::map<int, std::string>	getErrorPages() const { return error_pages; }
	size_t						getClientMaxBodySize() const { return client_max_body_size; }
};


#endif

#ifndef SERVERCONFIG_HPP
#define SERVERCONFIG_HPP

#include "WebServ.hpp"
#include "Location.hpp"

class ServerConfig {
private:
	uint16_t	port;
	uint32_t	host;
	std::string	root;
	std::vector<std::string>	index;
	std::vector<Location>		locations;
	std::vector<std::string>	server_name;
	std::map<int, std::string> error_pages;
	long long	client_max_body_size;
	long long	max_headers_size;
	long long	max_request_size;

	void swap(ServerConfig &other);

public:
	ServerConfig();
	ServerConfig(const ServerConfig &copy);
	ServerConfig &operator=(ServerConfig copy);
	~ServerConfig();
	void	setPort(uint16_t port);
	void	setHost(uint32_t host);
	void	setRoot(std::string root);
	void	setClientMaxBodySize(long long size);
	void	setMaxHeaderSize(long long size);
	void	setMaxRequestSize(long long size);
	void	addIndexBack(const std::string &index);
	void	setIndex(const std::vector<std::string> &index);
	void	addLocationBack(const Location &loc);
	void	addErrorPageBack(int code, std::string url);
	void	setServerName(const std::vector<std::string> names);
	uint16_t	getPort() const;
	uint32_t	getHost() const;
	std::string	getRoot() const;
	std::vector<std::string>	getIndex() const;
	std::vector<Location>		getLocations() const;
	const std::vector<Location>	&getLocationsRef() const;
	std::map<int, std::string>	getErrorPages() const;
	std::vector<std::string>	getServerName() const;
	long long		getClientMaxBodySize() const;
	long long		getMaxHeaderSize() const;
	long long		getMaxRequestSize() const;
	Location	findLocationForPath(std::string path) const;

	std::string	bindKey();
};

#endif

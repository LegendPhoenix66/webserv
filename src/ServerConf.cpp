#include "../inc/ServerConf.hpp"

ServerConf::ServerConf()
{
}

ServerConf::ServerConf(const ServerConf &copy)
:	port(copy.port),
	server_name(copy.server_name),
	host(copy.host),
	root(copy.root),
	index(copy.index),
	locations(copy.locations),
	error_pages(copy.error_pages),
	client_max_body_size(copy.client_max_body_size)
{
}

ServerConf	ServerConf::operator=(const ServerConf &copy)
{
	if (this != &copy) {
		*this = copy;
	}
	return *this;
}

ServerConf::~ServerConf()
{
}

void	ServerConf::setPort(uint16_t port)
{
	this->port = port;
}

void	ServerConf::setServerName(std::string name)
{
	this->server_name = name;
}

void	ServerConf::setHost(std::string host)
{
	this->host = host;
}

void	ServerConf::setRoot(std::string root)
{
	this->root = root;
}

void	ServerConf::addIndexBack(std::string index)
{
	this->index.push_back(index);
}

void	ServerConf::addLocationBack(Location loc)
{
	this->locations.push_back(loc);
}

void	ServerConf::addErrorPageBack(int code, std::string url)
{
	this->error_pages[code] = url;
}

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

ServerConf &ServerConf::operator=(const ServerConf &copy)
{
	ServerConf temp(copy);
	this->swap(temp);
	return *this;
}

ServerConf::~ServerConf()
{
}

void ServerConf::swap(ServerConf& other)
{
	// Using std::swap for all members
	std::swap(port, other.port);
	std::swap(server_name, other.server_name);
	std::swap(host, other.host);
	std::swap(root, other.root);
	std::swap(index, other.index);
	std::swap(locations, other.locations);
	std::swap(error_pages, other.error_pages);
	std::swap(client_max_body_size, other.client_max_body_size);
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

#include <utility>

#include "../inc/ServerConfig.hpp"

ServerConfig::ServerConfig() :
		port(0), host(0),
		client_max_body_size(-1),
		max_headers_size(-1),
		max_request_size(-1) {
}

ServerConfig::ServerConfig(const ServerConfig &copy)
		: port(copy.port),
		  host(copy.host),
		  root(copy.root),
		  index(copy.index),
		  locations(copy.locations),
		  error_pages(copy.error_pages),
		  client_max_body_size(copy.client_max_body_size),
		  max_headers_size(copy.max_request_size),
		  max_request_size(copy.max_request_size) {
}

ServerConfig &ServerConfig::operator=(ServerConfig copy) {
	this->swap(copy);
	return *this;
}

ServerConfig::~ServerConfig() {
}

void ServerConfig::swap(ServerConfig &other) {
	std::swap(this->port, other.port);
	std::swap(this->host, other.host);
	std::swap(this->root, other.root);
	std::swap(this->index, other.index);
	std::swap(this->locations, other.locations);
	std::swap(this->error_pages, other.error_pages);
	std::swap(this->client_max_body_size, other.client_max_body_size);
}


void ServerConfig::setPort(uint16_t port) {
	this->port = port;
}

void ServerConfig::setHost(uint32_t host) {
	this->host = host;
}

void ServerConfig::setRoot(std::string root) {
	this->root = root;
}

void ServerConfig::setClientMaxBodySize(long long size) {
	this->client_max_body_size = size;
}

void	ServerConfig::setMaxHeaderSize(long long size) {
	this->max_headers_size = size;
}

void	ServerConfig::setMaxRequestSize(long long size) {
	this->max_request_size = size;
}

void ServerConfig::addIndexBack(const std::string &index) {
	this->index.push_back(index);
}

void	ServerConfig::setIndex(const std::vector <std::string> &index) {
	this->index = index;
}

void ServerConfig::addLocationBack(const Location &loc) {
	this->locations.push_back(loc);
}

void ServerConfig::addErrorPageBack(int code, std::string url) {
	this->error_pages[code] = url;
}

void	ServerConfig::setServerName(const std::vector<std::string> names) {
	server_name = names;
}

uint16_t ServerConfig::getPort() const {
	return port;
}

uint32_t ServerConfig::getHost() const {
	return host;
}

std::string ServerConfig::getRoot() const {
	return root;
}

std::vector<std::string> ServerConfig::getIndex() const {
	return index;
}

std::vector<Location> ServerConfig::getLocations() const {
	return locations;
}

const std::vector<Location>	&ServerConfig::getLocationsRef() const {
	return locations;
}

std::map<int, std::string> ServerConfig::getErrorPages() const {
	return error_pages;
}

std::vector<std::string>	ServerConfig::getServerName() const {
	return server_name;
}

long long	ServerConfig::getClientMaxBodySize() const {
	return this->client_max_body_size;
}

long long	ServerConfig::getMaxHeaderSize() const {
	return this->max_headers_size;
}

long long	ServerConfig::getMaxRequestSize() const {
	return this->max_request_size;
}

Location	ServerConfig::findLocationForPath(std::string path) const {
	for (size_t i = 0; i < this->locations.size(); i++) {
		if (this->locations[i].getPath() == path)
			return this->locations[i];
	}
	return Location();
}

std::string	ServerConfig::bindKey() {
	std::ostringstream	oss;
	oss << host << ":" << port;
	return oss.str();
}

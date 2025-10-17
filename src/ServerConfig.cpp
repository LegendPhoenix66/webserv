#include <utility>

#include "../inc/ServerConfig.hpp"

ServerConfig::ServerConfig() :
		port(0), host(0),
		client_max_body_size(0) {
}

ServerConfig::ServerConfig(const ServerConfig &copy)
		: port(copy.port),
		  host(copy.host),
		  root(copy.root),
		  index(copy.index),
		  locations(copy.locations),
		  error_pages(copy.error_pages),
		  client_max_body_size(copy.client_max_body_size) {
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

void ServerConfig::setClientMaxBodySize(size_t size) {
	this->client_max_body_size = size;
}

void ServerConfig::addIndexBack(const std::string &index) {
	this->index.push_back(index);
}

void ServerConfig::addLocationBack(const Location &loc) {
	this->locations.push_back(loc);
}

void ServerConfig::addErrorPageBack(int code, std::string url) {
	this->error_pages[code] = url;
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

std::map<int, std::string> ServerConfig::getErrorPages() const {
	return error_pages;
}

size_t ServerConfig::getClientMaxBodySize() const {
	return this->client_max_body_size;
}

Location	ServerConfig::findLocationForPath(std::string path) const {
	for (size_t i = 0; i < this->locations.size(); i++) {
		if (this->locations[i].getPath() == path)
			return this->locations[i];
	}
	return Location();
}

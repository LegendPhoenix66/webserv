#include <utility>

#include "../inc/ServerConfig.hpp"

ServerConfig::ServerConfig() {
}

ServerConfig::ServerConfig(const ServerConfig &copy)
        : port(copy.port),
          server_name(copy.server_name),
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
    std::swap(port, other.port);
    std::swap(server_name, other.server_name);
    std::swap(host, other.host);
    std::swap(root, other.root);
    std::swap(index, other.index);
    std::swap(locations, other.locations);
    std::swap(error_pages, other.error_pages);
    std::swap(client_max_body_size, other.client_max_body_size);
}


void ServerConfig::setPort(uint16_t port) {
    this->port = port;
}

void ServerConfig::setServerName(std::string name) {
    this->server_name = std::move(name);
}

void ServerConfig::setHost(std::string host) {
    this->host = std::move(host);
}

void ServerConfig::setRoot(std::string root) {
    this->root = std::move(root);
}

void ServerConfig::addIndexBack(const std::string &index) {
    this->index.push_back(index);
}

void ServerConfig::addLocationBack(const Location &loc) {
    this->locations.push_back(loc);
}

void ServerConfig::addErrorPageBack(int code, std::string url) {
    this->error_pages[code] = std::move(url);
}

uint16_t ServerConfig::getPort() const {
    return port;
}

std::string ServerConfig::getServerName() const {
    return server_name;
}

std::string ServerConfig::getHost() const {
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
    return client_max_body_size;
}

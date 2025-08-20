#include "../inc/Server.hpp"

Server::Server()
        : listen_fd(-1) {
}

Server::Server(const Server &other)
        : listen_fd(other.listen_fd),
          config(other.config) {
}

Server &Server::operator=(Server other) {
    this->swap(other);
    return *this;
}

Server::~Server() {
}

Server::Server(const ServerConfig &config)
        : listen_fd(-1),
          config(config) {
}

void Server::swap(Server &other) {
    std::swap(this->config, other.config);
    std::swap(this->listen_fd, other.listen_fd);
}

void Server::start() {
    std::cout << "Starting server on port " << config.getPort() << std::endl;
}
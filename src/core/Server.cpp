#include "../../inc/Server.hpp"
#include "../../inc/Config.hpp"
#include <iostream>

Server::Server(const Config &cfg) : _cfg(cfg) {}
Server::~Server() throw() {}

bool Server::startOnce(std::string *err) {
    if (_cfg.servers.empty()) {
        if (err) *err = "no servers configured";
        return false;
    }
    const ServerConfig &sc = _cfg.servers[0];
    if (!_listener.start(sc, err)) {
        if (err && err->empty()) *err = "failed to start listener";
        return false;
    }
    std::cout << "listening on " << sc.host << ":" << sc.port << " (non-blocking)\n";
    return true;
}

#ifndef ROUTER_HPP
#define ROUTER_HPP

#include <vector>
#include <string>
#include "Config.hpp"

struct RouteMatch {
    const LocationConfig *loc; // null if no match
    RouteMatch() : loc(0) {}
};

class Router {
public:
    Router();
    void build(const ServerConfig &srv);                 // capture pointers to locations, sorted by longest prefix first
    RouteMatch match(const std::string &target) const;   // normalize and longest-prefix match
private:
    std::vector<const LocationConfig*> _locs;            // sorted by descending path length
};

#endif // ROUTER_HPP

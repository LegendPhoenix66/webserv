#ifndef ROUTER_HPP
#define ROUTER_HPP

#include <vector>
#include <string>
#include <algorithm>
#include "ServerConfig.hpp"

struct RouteMatch {
	const Location *loc; // null if no match
	RouteMatch() : loc(NULL) {}
};

class Router {
public:
	Router();
	void build(const ServerConfig &srv);                 // capture pointers to locations, sorted by longest prefix first
	RouteMatch match(const std::string &target) const;   // normalize and longest-prefix match
private:
	std::vector<const Location *> _locs;            // sorted by descending path length
	std::string	normalize_target(const std::string &t) const;
};

#endif // ROUTER_HPP

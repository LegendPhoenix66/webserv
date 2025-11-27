#include "../inc/Router.hpp"

static bool	loc_cmp(const Location *a, const Location *b) {
	if (!a || !b) return a && !b;
	if (a->getPath().size() != b->getPath().size()) return a->getPath().size() > b->getPath().size();
	return a->getPath() < b->getPath(); // tie-breaker
}

Router::Router() : _locs() {}

void Router::build(const ServerConfig &srv) {
	_locs.clear();
	const std::vector<Location>	&locs = srv.getLocationsRef();
	_locs.reserve(locs.size());
	for (size_t i = 0; i < locs.size(); ++i) {
		_locs.push_back(&locs[i]);
	}
	std::sort(_locs.begin(), _locs.end(), loc_cmp);
}

RouteMatch Router::match(const std::string &target) const {
	RouteMatch m;
	if (_locs.empty()) return m;
	std::string norm = normalize_target(target);
	for (size_t i = 0; i < _locs.size(); ++i) {
		const Location	*loc = _locs[i];
		if (!loc) continue;
		const std::string &p = loc->getPath();
		if (p.empty()) continue;
		if (norm.size() >= p.size() && norm.compare(0, p.size(), p) == 0) {
			m.loc = loc;
			return m;
		}
	}
	return m;
}

std::string	Router::normalize_target(const std::string &t) const {
	if (t.empty()) return std::string("/");
	std::string out; out.reserve(t.size()+1);
	if (t[0] != '/') out.push_back('/');
	for (size_t i = 0; i < t.size(); ++i) {
		char c = t[i];
		if (c == '\\') c = '/';
		out.push_back(c);
	}
	return out;
}

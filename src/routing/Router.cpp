#include "../../inc/Router.hpp"

#include <algorithm>

static std::string normalize_target(const std::string &t) {
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

static bool loc_cmp(const LocationConfig* a, const LocationConfig* b) {
    if (!a || !b) return a && !b;
    if (a->path.size() != b->path.size()) return a->path.size() > b->path.size();
    return a->path < b->path; // tie-breaker
}

Router::Router() : _locs() {}

void Router::build(const ServerConfig &srv) {
    _locs.clear();
    for (size_t i = 0; i < srv.locations.size(); ++i) {
        _locs.push_back(&srv.locations[i]);
    }
    std::sort(_locs.begin(), _locs.end(), loc_cmp);
}

RouteMatch Router::match(const std::string &target) const {
    RouteMatch m;
    if (_locs.empty()) return m;
    std::string norm = normalize_target(target);
    for (size_t i = 0; i < _locs.size(); ++i) {
        const LocationConfig *loc = _locs[i];
        if (!loc) continue;
        const std::string &p = loc->path;
        if (p.empty()) continue;
        if (norm.size() >= p.size() && norm.compare(0, p.size(), p) == 0) {
            m.loc = loc;
            return m;
        }
    }
    return m;
}

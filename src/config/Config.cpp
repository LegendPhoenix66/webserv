#include "../../inc/Config.hpp"
#include <sstream>

const char *WEBSERV_VERSION = "0.1.0";

static std::string join(const std::vector<std::string>& v, const char* sep) {
    std::ostringstream oss;
    for (std::vector<std::string>::const_iterator it = v.begin(); it != v.end(); ++it) {
        if (it != v.begin()) oss << sep;
        oss << *it;
    }
    return oss.str();
}

std::string toString(const LocationConfig &loc) {
    std::ostringstream oss;
    oss << loc.path;
    if (loc.has_methods) oss << " methods_mask=" << loc.methods_mask;
    if (loc.has_root) oss << " root=" << loc.root;
    if (loc.has_index) oss << " index=[" << join(loc.index, " ") << "]";
    if (loc.has_autoindex) oss << " autoindex=" << (loc.autoindex ? "on" : "off");
    if (loc.has_redirect) oss << " return=" << loc.redirect_code << " " << loc.redirect_location;
    return oss.str();
}

std::string toString(const ServerConfig &srv) {
    std::ostringstream oss;
    oss << srv.host << ":" << (srv.port);
    oss << " root=" << srv.root;
    oss << " index=[" << join(srv.index, " ") << "]";
    oss << " errors=[";
    for (std::map<int, std::string>::const_iterator it = srv.error_pages.begin(); it != srv.error_pages.end(); ++it) {
        if (it != srv.error_pages.begin()) oss << " ";
        oss << it->first << "->" << it->second;
    }
    oss << "]";
    if (!srv.server_names.empty()) {
        oss << " names=[" << join(srv.server_names, " ") << "]";
    }
    if (!srv.locations.empty()) {
        oss << " locs=" << srv.locations.size() << "(first=" << toString(srv.locations[0]) << ")";
    }
    return oss.str();
}

std::string toString(const Config &cfg) {
    std::ostringstream oss;
    oss << "servers=" << cfg.servers.size();
    if (!cfg.servers.empty()) {
        oss << " first={" << toString(cfg.servers[0]) << "}";
    }
    return oss.str();
}

std::string bindKeyOf(const ServerConfig &srv) {
    std::ostringstream oss;
    oss << srv.host << ":" << srv.port;
    return oss.str();
}

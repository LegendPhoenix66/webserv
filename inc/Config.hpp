#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <string>
#include <vector>
#include <map>

// Router/location configuration (Phase 1)
struct LocationConfig {
    std::string path;                  // prefix, must start with '/'

    // Methods bitmask
    unsigned methods_mask;             // bit 0=GET, 1=HEAD, 2=POST, 3=DELETE
    bool has_methods;

    // Overrides
    std::string root;                  // optional override of server root
    bool has_root;

    std::vector<std::string> index;    // optional override of server index list
    bool has_index;

    bool autoindex;                    // default false
    bool has_autoindex;

    // Redirect support
    int redirect_code;                 // 301 or 302 supported in Phase 1
    std::string redirect_location;     // absolute or absolute-path URL
    bool has_redirect;

    // Placeholders for later phases
    std::string cgi_pass;              // interpreter path
    std::string cgi_path;              // script path
    std::string upload_store;          // directory path
    long client_max_body_size_override; // -1 if not set

    LocationConfig()
    : path("/"), methods_mask(0), has_methods(false),
      root(), has_root(false), index(), has_index(false),
      autoindex(false), has_autoindex(false),
      redirect_code(0), redirect_location(), has_redirect(false),
      cgi_pass(), cgi_path(), upload_store(), client_max_body_size_override(-1) {}
};

// Server-level configuration
struct ServerConfig {
    std::string host;              // required
    int         port;              // required (1..65535)
    std::string root;              // required
    std::vector<std::string> index;        // optional, may be empty
    std::map<int, std::string> error_pages; // optional
    std::vector<std::string> server_names;  // optional

    // Router: locations
    std::vector<LocationConfig> locations;  // optional

    // Body size default (enforced when uploading/POST)
    long client_max_body_size;              // -1 if not set

    // Limits & timeouts (server-level). -1 => use internal defaults
    long max_header_size;                   // bytes; default 16384
    int  max_header_lines;                  // default 100
    long max_request_line;                  // bytes; default 4096
    int  header_timeout_ms;                 // default 5000

    // Presence flags for validation
    bool has_host;
    bool has_port;
    bool has_root;

    ServerConfig()
    : port(-1),
      client_max_body_size(-1),
      max_header_size(-1),
      max_header_lines(-1),
      max_request_line(-1),
      header_timeout_ms(-1),
      has_host(false), has_port(false), has_root(false) {}
};

struct Config {
    std::vector<ServerConfig> servers;
};

// Helpers to print summaries
std::string toString(const LocationConfig &loc);
std::string toString(const ServerConfig &srv);
std::string toString(const Config &cfg);

// Build a canonical bind key like "127.0.0.1:8080"
std::string bindKeyOf(const ServerConfig &srv);

// Version string for banner
extern const char *WEBSERV_VERSION;

#endif // CONFIG_HPP

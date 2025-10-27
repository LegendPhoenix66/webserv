#include "../../inc/MimeTypes.hpp"

#include <string>
#include <cctype>

static std::string to_lower(const std::string &s) {
    std::string out = s;
    for (size_t i = 0; i < out.size(); ++i) out[i] = static_cast<char>(std::tolower(out[i]));
    return out;
}

static std::string ext_of(const std::string &path) {
    std::string::size_type dot = path.find_last_of('.');
    if (dot == std::string::npos) return "";
    return to_lower(path.substr(dot + 1));
}

std::string MimeTypes::of(const std::string &path) {
    std::string ext = ext_of(path);
    if (ext == "html" || ext == "htm") return "text/html; charset=utf-8";
    if (ext == "css") return "text/css; charset=utf-8";
    if (ext == "js") return "application/javascript";
    if (ext == "json") return "application/json";
    if (ext == "png") return "image/png";
    if (ext == "jpg" || ext == "jpeg") return "image/jpeg";
    if (ext == "gif") return "image/gif";
    if (ext == "svg") return "image/svg+xml";
    if (ext == "ico") return "image/x-icon";
    if (ext == "txt") return "text/plain; charset=utf-8";
    return "application/octet-stream";
}

#ifndef STATIC_FILE_HANDLER_HPP
#define STATIC_FILE_HANDLER_HPP

#include <string>
#include <vector>

struct ServerConfig;
struct HttpRequest;
class HttpResponse;

namespace StaticFileHandler {
    // Resolve a request target under the given root and optional index list.
    // Returns true on success (200) and fills response; false if not found (caller should send 404).
    // If method is HEAD, it only sets headers with correct Content-Length and no body.
    // If autoindex is true and a directory without index is requested, generate a simple listing.
    bool handle(const std::string &root,
                const std::vector<std::string> &indexList,
                const HttpRequest &req,
                bool isHead,
                bool autoindex,
                HttpResponse &outResp,
                std::string &err);
}

#endif // STATIC_FILE_HANDLER_HPP

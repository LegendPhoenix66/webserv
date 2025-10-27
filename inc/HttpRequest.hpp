#ifndef HTTP_REQUEST_HPP
#define HTTP_REQUEST_HPP

#include <string>
#include <map>

struct HttpRequest {
    std::string method;
    std::string target; // request-target (path or absolute-form)
    std::string version; // e.g., HTTP/1.1
    std::map<std::string, std::string> headers;
};

#endif // HTTP_REQUEST_HPP

#include "../inc/Response.hpp"
#include <sstream>

std::string Response::buildStatusLine(int code, const std::string &reason) {
    std::ostringstream oss;
    oss << "HTTP/1.1 " << code << " " << reason << "\r\n";
    return oss.str();
}

std::string Response::buildSimpleHtml(int code, const std::string &reason, const std::string &body) {
    std::ostringstream oss;
    oss << buildStatusLine(code, reason);
    oss << "Content-Type: text/html; charset=UTF-8\r\n";
    oss << "Content-Length: " << body.size() << "\r\n";
    oss << "Connection: close\r\n\r\n";
    oss << body;
    return oss.str();
}

std::string Response::buildErrorHtml(int code, const std::string &reason) {
    std::ostringstream body;
    body << "<!doctype html><html><head><title>" << code << " " << reason
         << "</title></head><body><h1>" << code << " " << reason << "</h1></body></html>";
    return buildSimpleHtml(code, reason, body.str());
}

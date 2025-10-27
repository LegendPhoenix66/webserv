#include "../../inc/HttpResponse.hpp"

#include <sstream>
#include <ctime>
#include <cstdio>

namespace {
static std::string httpDateNow() {
    char buf[64];
    std::time_t t = std::time(0);
    std::tm gmt;
#if defined(_WIN32)
    gmtime_s(&gmt, &t);
#else
    gmt = *std::gmtime(&t);
#endif
    // RFC 1123 format: Sun, 06 Nov 1994 08:49:37 GMT
    std::strftime(buf, sizeof(buf), "%a, %d %b %Y %H:%M:%S GMT", &gmt);
    return std::string(buf);
}
}

HttpResponse::HttpResponse() : _status(200), _reason("OK") {}

void HttpResponse::setStatus(int code, const std::string &reason) {
    _status = code;
    _reason = reason;
}

void HttpResponse::setHeader(const std::string &name, const std::string &value) {
    _headers[name] = value;
}

void HttpResponse::setBody(const std::string &body) {
    _body = body;
}

std::vector<char> HttpResponse::serialize() const {
    // Start with a local copy so we can inject safe defaults without mutating state
    std::map<std::string, std::string> hdrs = _headers;

    if (hdrs.find("Date") == hdrs.end()) hdrs["Date"] = httpDateNow();
    if (hdrs.find("Server") == hdrs.end()) hdrs["Server"] = "webserv";
    if (hdrs.find("Connection") == hdrs.end()) hdrs["Connection"] = "close"; // conservative default
    if (hdrs.find("Transfer-Encoding") == hdrs.end() && hdrs.find("Content-Length") == hdrs.end()) {
        std::ostringstream cl; cl << _body.size();
        hdrs["Content-Length"] = cl.str();
    }

    std::ostringstream oss;
    oss << "HTTP/1.1 " << _status << " " << _reason << "\r\n";
    for (std::map<std::string, std::string>::const_iterator it = hdrs.begin(); it != hdrs.end(); ++it) {
        oss << it->first << ": " << it->second << "\r\n";
    }
    oss << "\r\n";

    const std::string head = oss.str();
    std::vector<char> out;
    out.reserve(head.size() + _body.size());
    out.insert(out.end(), head.begin(), head.end());
    out.insert(out.end(), _body.begin(), _body.end());
    return out;
}

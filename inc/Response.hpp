#ifndef WEBSERV_RESPONSE_HPP
#define WEBSERV_RESPONSE_HPP

#include <string>

namespace Response {
    std::string buildStatusLine(int code, const std::string &reason);
    std::string buildSimpleHtml(int code, const std::string &reason, const std::string &body);
    std::string buildErrorHtml(int code, const std::string &reason);
}

#endif // WEBSERV_RESPONSE_HPP

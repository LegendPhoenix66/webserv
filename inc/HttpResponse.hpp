#ifndef HTTP_RESPONSE_HPP
#define HTTP_RESPONSE_HPP

#include <string>
#include <map>
#include <vector>

class HttpResponse {
public:
    HttpResponse();

    void setStatus(int code, const std::string &reason);
    void setHeader(const std::string &name, const std::string &value);
    void setBody(const std::string &body);

    // Serialize to a byte vector.
    std::vector<char> serialize() const;

private:
    int _status;
    std::string _reason;
    std::map<std::string, std::string> _headers;
    std::string _body;
};

#endif // HTTP_RESPONSE_HPP

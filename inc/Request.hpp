#ifndef WEBSERV_REQUEST_HPP
#define WEBSERV_REQUEST_HPP

#include <string>
#include <map>

class Request {
public:
    enum State { ReadingHeaders, ReadingBody, Ready, Error };

    Request();

    // Feed data and progress the parser. Returns current state.
    State feed(const std::string &data);

    // Accessors
    const std::string& method() const { return _method; }
    const std::string& target() const { return _target; }
    const std::string& version() const { return _version; }
    const std::map<std::string,std::string>& headers() const { return _headers; }
    size_t contentLength() const { return _content_length; }
    bool isChunked() const { return _chunked; }
    bool keepAlive() const { return _keep_alive; }
    const std::string& body() const { return _body; }

private:
    State _state;
    std::string _buf;
    std::string _method, _target, _version;
    std::map<std::string,std::string> _headers;
    size_t _content_length;
    bool _chunked;
    bool _keep_alive;
    std::string _body;

    bool parseStartLineAndHeaders();
    static std::string trim(const std::string &s);
};

#endif // WEBSERV_REQUEST_HPP

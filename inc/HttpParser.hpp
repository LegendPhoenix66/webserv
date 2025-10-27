#ifndef HTTP_PARSER_HPP
#define HTTP_PARSER_HPP

#include <string>
#include <map>
#include "HttpRequest.hpp"

class HttpParser {
public:
    enum Result { NEED_MORE = 0, OK = 1, ERROR = 2 };

    // Parser error classification for mapping to HTTP status
    enum ErrorKind {
        ERR_NONE = 0,
        ERR_REQUEST_LINE_TOO_LONG,
        ERR_HEADER_LINE_TOO_LONG,
        ERR_TOO_MANY_HEADERS,
        ERR_MALFORMED_REQUEST,
        ERR_BAD_METHOD,
        ERR_BAD_VERSION,
        ERR_EMPTY_HEADER_NAME,
        ERR_MALFORMED_HEADER
    };

    HttpParser();

    // Configure limits (bytes/lines) before feeding data
    void setLimits(size_t maxStartLine, size_t maxHeaderLine, size_t maxHeaders);

    // Feed new data chunk; returns parser state.
    Result feed(const char *data, size_t len);

    // If OK: access parsed request. If ERROR: access error message and kind.
    const HttpRequest &request() const { return _req; }
    const std::string &error() const { return _err; }
    ErrorKind errorKind() const { return _errKind; }

    // After headers are parsed (OK), any extra bytes already read are kept here.
    // Use these helpers to access/consume them when starting to read the body.
    size_t remainingSize() const { return _buf.size(); }
    void takeRemaining(std::string &out) { out.swap(_buf); }

private:
    std::string _buf;
    HttpRequest _req;
    std::string _err;
    ErrorKind _errKind;
    bool _haveStartLine;
    bool _done;

    // Limits
    size_t _maxStartLine;
    size_t _maxHeaderLine;
    size_t _maxHeaders;

    bool parseStartLine();
    bool parseHeaders();
};

#endif // HTTP_PARSER_HPP

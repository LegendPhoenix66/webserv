#ifndef CONNECTION_HPP
#define CONNECTION_HPP

#include <string>
#include <vector>
#include <map>
#include <stdint.h>

#include <sys/types.h>

#include "HttpParser.hpp"
#include "HttpResponse.hpp"
#include "Config.hpp"
#include "Router.hpp"

class EventLoop; // forward decl

class Connection {
public:
    // Construct a connection associated with a listener's vhost group and bind key.
    explicit Connection(int fd, const std::vector<const ServerConfig*> &group, const std::string &bindKey, EventLoop* loop);
    ~Connection();

    int fd() const { return _fd; }

    // Desired poll events
    bool wantRead() const;
    bool wantWrite() const;

    // Event handlers return false on fatal/close
    bool onReadable();
    bool onWritable();

    // Auxiliary (CGI) fds readiness; return false to close client
    bool onAuxEvent(int fd, short revents);

    // Timeout sweep hook; returns true to keep, false to remove/close
    bool checkTimeouts(uint64_t now_ms);

    bool isClosed() const { return _closed; }

private:
    enum BodyState { BODY_NONE = 0, BODY_FIXED = 1, BODY_CHUNKED = 2, BODY_DONE = 3 };

    // Load mapped error page body for the given status code using current server's error_pages.
    // The mapped path is resolved relative to the server root (absolute or starting with '/' is treated as under root).
    bool loadMappedErrorBody(int code, std::string &out);

    int _fd;
    bool _closed;

    // vhost context
    std::vector<const ServerConfig*> _group; // servers for this bind
    const ServerConfig* _srv;               // currently selected server (default until Host parsed)
    std::string _bindKey;                   // e.g., 127.0.0.1:8080
    std::string _vhostName;                 // for logging

    // static serving context derived from _srv
    std::string _root;
    std::vector<std::string> _index;

    // Router cache for current server
    Router _router;
    const ServerConfig* _routerSrv; // last server used to build _router

    HttpParser _parser;
    std::string _rbuf; // read buffer
    std::vector<char> _wbuf; // write buffer

    // Request lifecycle
    bool _headersDone;          // headers parsing completed
    HttpRequest _req;           // snapshot of parsed request

    // Body handling
    BodyState _bodyState;
    std::string _bodyBuf;       // aggregated body (unchunked)
    long _bodyLimit;            // effective client_max_body_size (-1 means unlimited)
    long _clRemaining;          // bytes remaining for fixed-length mode
    // Chunked decoding state
    long _chunkRemaining;       // -1 when expecting size line; >=0 when reading chunk data; 0 means final-chunk seen
    bool _chunkReadingTrailers; // true after 0-chunk, consume trailers until CRLF CRLF

    bool processChunkedBuffered();

    // timing & stats
    uint64_t _t_start;
    uint64_t _t_last_active;
    uint64_t _t_headers_start;
    uint64_t _t_write_start;
    size_t   _bytes_sent;
    int      _status_code; // 0 until set
    bool     _logged;
    std::string _reqLine;
    std::string _peer;

    // Back-pointer to event loop for aux fd registration
    EventLoop* _loop;

    // --- CGI state ---
    enum CgiState { CGI_NONE = 0, CGI_SPAWNING = 1, CGI_STREAMING = 2, CGI_DONE = 3 };
    CgiState _cgiState;
    int _cgiPid;
    int _cgiIn;   // write end to child stdin
    int _cgiOut;  // read end from child stdout
    uint64_t _t_cgi_start;
    std::string _cgiHdrBuf;    // header buffer until CRLFCRLF
    bool _cgiHeadersDone;
    int _cgiStatusFromCGI;
    std::map<std::string,std::string> _cgiHdrs;
    size_t _cgiOutputSent;
    static const size_t CGI_OUTPUT_MAX = 8 * 1024 * 1024; // safety cap

    // For routing across callbacks
    bool _cgiEnabled;
    std::string _locCgiPass;
    std::string _locCgiPath;
    std::string _effRootForRequest;

    bool startCgiCurrent();
    bool startCgiWith(const std::string &cgiPass, const std::string &cgiPath,
                      const std::string &effRoot, const HttpRequest &req);
    void abortCgiWithStatus(int code, const char *reason);
    void closeCgiPipes();

    void closeFd();

    void makeError400();
    void makeError404();
    void makeError408();
    void makeError411();
    void makeError413();
    void makeError414();
    void makeError431();
    void makeNotImplemented501();
    void makeMethodNotAllowed();
    void makeMethodNotAllowedAllow(const std::string &allow);
    void makeOkResponse(const HttpRequest &req, bool headOnly);
    void makeRedirect(int code, const std::string &location);

    void logAccess();

    static bool readFileToString(const std::string &path, std::string &out);

    // New helpers/responses for uploads & DELETE
    void makeError403();
    void makeError500();
    void makeCreated201(const std::string &location, size_t sizeBytes);
    void makeNoContent204();

    // Matched location context for POST/DELETE handling
    std::string _uploadStore;      // from matched location (empty if none)
    std::string _matchedLocPath;   // matched location path (e.g., "/upload/")
};

#endif // CONNECTION_HPP

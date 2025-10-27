#include "../../inc/Connection.hpp"

#include <unistd.h>
#include <fcntl.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <cstdio>
#include <sstream>
#include <algorithm>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>
#include <vector>
#include <poll.h>
#include "../../inc/EventLoop.hpp"
#include "../../inc/StaticFileHandler.hpp"
#include "../../inc/HttpRequest.hpp"
#include "../../inc/Logger.hpp"
#include "../../inc/Config.hpp"

static uint64_t now_ms_conn() {
    struct timeval tv; gettimeofday(&tv, 0);
    return (uint64_t)tv.tv_sec * 1000ULL + (uint64_t)(tv.tv_usec / 1000ULL);
}

// Timeouts (ms)
static const uint64_t IDLE_TIMEOUT_MS = 15000ULL;
static const uint64_t HEADERS_TIMEOUT_MS = 5000ULL;
static const uint64_t WRITE_DRAIN_TIMEOUT_MS = 10000ULL;

static std::string peer_of(int fd) {
    struct sockaddr_in sa; socklen_t sl = sizeof(sa);
    char buf[64];
    if (getpeername(fd, (struct sockaddr*)&sa, &sl) == 0) {
        const char *ip = inet_ntop(AF_INET, &sa.sin_addr, buf, sizeof(buf));
        std::ostringstream oss; oss << (ip ? ip : "?") << ":" << ntohs(sa.sin_port);
        return oss.str();
    }
    return std::string("-");
}

static std::string to_lower_copy(const std::string &s) {
    std::string out = s;
    for (size_t i = 0; i < out.size(); ++i) {
        char c = out[i];
        if (c >= 'A' && c <= 'Z') out[i] = static_cast<char>(c - 'A' + 'a');
    }
    return out;
}

static std::string strip_port(const std::string &host) {
    std::string::size_type pos = host.find(':');
    if (pos == std::string::npos) return host;
    return host.substr(0, pos);
}

static std::string find_header_icase(const std::map<std::string, std::string> &hdrs, const std::string &name) {
    std::string lname = to_lower_copy(name);
    for (std::map<std::string, std::string>::const_iterator it = hdrs.begin(); it != hdrs.end(); ++it) {
        if (to_lower_copy(it->first) == lname) return it->second;
    }
    return std::string();
}

// --- Path utilities for uploads/DELETE ---
static std::string normalize_target_simple(const std::string &t) {
    if (t.empty()) return std::string("/");
    std::string out; out.reserve(t.size()+1);
    if (t[0] != '/') out.push_back('/');
    for (size_t i = 0; i < t.size(); ++i) {
        char c = t[i];
        if (c == '\\') c = '/';
        out.push_back(c);
    }
    return out;
}

static std::string join_path_simple(const std::string &a, const std::string &b) {
    if (a.empty()) return b;
    if (a[a.size()-1] == '/') return a + (b.size() && b[0] == '/' ? b.substr(1) : b);
    return a + "/" + (b.size() && b[0] == '/' ? b.substr(1) : b);
}

static std::string base_name_only(const std::string &p) {
    if (p.empty()) return p;
    std::string::size_type pos = p.find_last_of("/\\");
    if (pos == std::string::npos) return p;
    return p.substr(pos + 1);
}

static std::string safe_filename(const std::string &s) {
    if (s.empty()) return std::string("upload.bin");
    std::string out; out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        char c = s[i];
        bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-';
        out.push_back(ok ? c : '_');
    }
    if (out.empty()) out = "upload.bin";
    return out;
}

static std::string gen_unique_upload_name() {
    struct timeval tv; gettimeofday(&tv, 0);
    std::ostringstream oss; oss << "upload-" << (unsigned long)tv.tv_sec << "-" << (unsigned long)tv.tv_usec << ".bin";
    return oss.str();
}

Connection::Connection(int fd, const std::vector<const ServerConfig*> &group, const std::string &bindKey, EventLoop* loop)
: _fd(fd), _closed(false), _group(group), _srv(0), _bindKey(bindKey), _vhostName("-"), _root(), _index(),
  _router(), _routerSrv(0), _rbuf(), _wbuf(),
  _headersDone(false), _req(),
  _bodyState(BODY_NONE), _bodyBuf(), _bodyLimit(-1), _clRemaining(0),
  _chunkRemaining(-1), _chunkReadingTrailers(false),
  _t_start(now_ms_conn()), _t_last_active(_t_start), _t_headers_start(_t_start), _t_write_start(0),
  _bytes_sent(0), _status_code(0), _logged(false), _reqLine("-"), _peer(peer_of(fd)),
  _loop(loop), _cgiState(CGI_NONE), _cgiPid(-1), _cgiIn(-1), _cgiOut(-1), _t_cgi_start(0),
  _cgiHdrBuf(), _cgiHeadersDone(false), _cgiStatusFromCGI(0), _cgiHdrs(), _cgiOutputSent(0),
  _cgiEnabled(false), _locCgiPass(), _locCgiPath(), _effRootForRequest(),
  _uploadStore(), _matchedLocPath() {
    if (!_group.empty() && _group[0]) {
        _srv = _group[0];
        _root = _srv->root;
        _index = _srv->index;
        _router.build(*_srv);
        _routerSrv = _srv;
        if (!_srv->server_names.empty()) _vhostName = _srv->server_names[0];
        // Apply parser limits from server config (with sane defaults)
        size_t maxRL = (_srv->max_request_line >= 0) ? (size_t)_srv->max_request_line : 4096u;
        size_t maxHL = (_srv->max_header_size >= 0) ? (size_t)_srv->max_header_size : 16384u;
        size_t maxHC = (_srv->max_header_lines >= 0) ? (size_t)_srv->max_header_lines : 100u;
        _parser.setLimits(maxRL, maxHL, maxHC);
    } else {
        // Default limits if no server configured (shouldn't happen)
        _parser.setLimits(4096u, 16384u, 100u);
    }
}

Connection::~Connection() {
    closeCgiPipes();
    if (_cgiPid > 0) { (void)::kill(_cgiPid, SIGKILL); (void)::waitpid(_cgiPid, 0, WNOHANG); _cgiPid = -1; }
    closeFd();
}

bool Connection::loadMappedErrorBody(int code, std::string &out) {
    if (!_srv) return false;
    std::map<int, std::string>::const_iterator it = _srv->error_pages.find(code);
    if (it == _srv->error_pages.end()) return false;
    // Resolve path relative to server root (treat leading '/' as under root)
    std::string mapped = it->second;
    std::string full = join_path_simple(_root, mapped);
    std::string body;
    if (!readFileToString(full, body)) return false;
    out.swap(body);
    return true;
}

bool Connection::wantRead() const {
    return !_closed && _wbuf.empty();
}

bool Connection::wantWrite() const {
    return !_closed && !_wbuf.empty();
}

void Connection::logAccess() {
    if (_logged) return;
    uint64_t dur = now_ms_conn() - _t_start;
    Logger::accessf("%s [%s] vhost=%s \"%s\" %d %zu dur_ms=%llu",
                    _peer.c_str(), _bindKey.c_str(), _vhostName.c_str(), _reqLine.c_str(),
                    _status_code, (size_t)_bytes_sent,
                    (unsigned long long)dur);
    _logged = true;
}

void Connection::closeFd() {
    if (!_closed && _fd >= 0) {
        if (_status_code != 0) logAccess();
        ::close(_fd);
        _fd = -1;
        _closed = true;
    }
}

bool Connection::readFileToString(const std::string &path, std::string &out) {
    FILE *f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    std::string buf;
    char tmp[4096];
    size_t n;
    while ((n = std::fread(tmp, 1, sizeof(tmp), f)) > 0) {
        buf.append(tmp, n);
    }
    std::fclose(f);
    out.swap(buf);
    return true;
}

void Connection::makeError400() {
    std::string body;
    if (!loadMappedErrorBody(400, body)) {
        if (!readFileToString("www/error/400.html", body)) {
            body = "<html><body><h1>400 Bad Request</h1></body></html>";
        }
    }
    HttpResponse resp;
    resp.setStatus(400, "Bad Request");
    resp.setHeader("Content-Type", "text/html; charset=utf-8");
    { std::ostringstream oss; oss << body.size(); resp.setHeader("Content-Length", oss.str()); }
    resp.setHeader("Connection", "close");
    resp.setBody(body);
    _wbuf = resp.serialize();
    _status_code = 400;
    _t_write_start = now_ms_conn();
}

void Connection::makeError408() {
    std::string body;
    if (!loadMappedErrorBody(408, body)) {
        if (!readFileToString("www/error/408.html", body)) {
            body = "<html><body><h1>408 Request Timeout</h1></body></html>";
        }
    }
    HttpResponse resp;
    resp.setStatus(408, "Request Timeout");
    resp.setHeader("Content-Type", "text/html; charset=utf-8");
    { std::ostringstream oss; oss << body.size(); resp.setHeader("Content-Length", oss.str()); }
    resp.setHeader("Connection", "close");
    resp.setBody(body);
    _wbuf = resp.serialize();
    _status_code = 408;
    _t_write_start = now_ms_conn();
}

void Connection::makeError411() {
    std::string body;
    if (!loadMappedErrorBody(411, body)) {
        if (!readFileToString("www/error/411.html", body)) {
            body = "<html><body><h1>411 Length Required</h1></body></html>";
        }
    }
    HttpResponse resp;
    resp.setStatus(411, "Length Required");
    resp.setHeader("Content-Type", "text/html; charset=utf-8");
    { std::ostringstream oss; oss << body.size(); resp.setHeader("Content-Length", oss.str()); }
    resp.setHeader("Connection", "close");
    resp.setBody(body);
    _wbuf = resp.serialize();
    _status_code = 411;
    _t_write_start = now_ms_conn();
}

void Connection::makeError413() {
    std::string body;
    if (!loadMappedErrorBody(413, body)) {
        if (!readFileToString("www/error/413.html", body)) {
            body = "<html><body><h1>413 Payload Too Large</h1></body></html>";
        }
    }
    HttpResponse resp;
    resp.setStatus(413, "Payload Too Large");
    resp.setHeader("Content-Type", "text/html; charset=utf-8");
    { std::ostringstream oss; oss << body.size(); resp.setHeader("Content-Length", oss.str()); }
    resp.setHeader("Connection", "close");
    resp.setBody(body);
    _wbuf = resp.serialize();
    _status_code = 413;
    _t_write_start = now_ms_conn();
}

void Connection::makeError414() {
    std::string body;
    if (!loadMappedErrorBody(414, body)) {
        if (!readFileToString("www/error/414.html", body)) {
            body = "<html><body><h1>414 URI Too Long</h1></body></html>";
        }
    }
    HttpResponse resp;
    resp.setStatus(414, "URI Too Long");
    resp.setHeader("Content-Type", "text/html; charset=utf-8");
    { std::ostringstream oss; oss << body.size(); resp.setHeader("Content-Length", oss.str()); }
    resp.setHeader("Connection", "close");
    resp.setBody(body);
    _wbuf = resp.serialize();
    _status_code = 414;
    _t_write_start = now_ms_conn();
}

void Connection::makeError431() {
    std::string body;
    if (!loadMappedErrorBody(431, body)) {
        if (!readFileToString("www/error/431.html", body)) {
            body = "<html><body><h1>431 Request Header Fields Too Large</h1></body></html>";
        }
    }
    HttpResponse resp;
    resp.setStatus(431, "Request Header Fields Too Large");
    resp.setHeader("Content-Type", "text/html; charset=utf-8");
    { std::ostringstream oss; oss << body.size(); resp.setHeader("Content-Length", oss.str()); }
    resp.setHeader("Connection", "close");
    resp.setBody(body);
    _wbuf = resp.serialize();
    _status_code = 431;
    _t_write_start = now_ms_conn();
}

void Connection::makeNotImplemented501() {
    std::string body;
    if (!loadMappedErrorBody(501, body)) {
        body = "<html><body><h1>501 Not Implemented</h1></body></html>";
    }
    HttpResponse resp;
    resp.setStatus(501, "Not Implemented");
    resp.setHeader("Content-Type", "text/html; charset=utf-8");
    { std::ostringstream oss; oss << body.size(); resp.setHeader("Content-Length", oss.str()); }
    resp.setHeader("Connection", "close");
    resp.setBody(body);
    _wbuf = resp.serialize();
    _status_code = 501;
    _t_write_start = now_ms_conn();
}

void Connection::makeError403() {
    std::string body;
    if (!loadMappedErrorBody(403, body)) {
        if (!readFileToString("www/error/403.html", body)) {
            body = "<html><body><h1>403 Forbidden</h1></body></html>";
        }
    }
    HttpResponse resp;
    resp.setStatus(403, "Forbidden");
    resp.setHeader("Content-Type", "text/html; charset=utf-8");
    { std::ostringstream oss; oss << body.size(); resp.setHeader("Content-Length", oss.str()); }
    resp.setHeader("Connection", "close");
    resp.setBody(body);
    _wbuf = resp.serialize();
    _status_code = 403;
    _t_write_start = now_ms_conn();
}

void Connection::makeError500() {
    std::string body;
    if (!loadMappedErrorBody(500, body)) {
        if (!readFileToString("www/error/500.html", body)) {
            body = "<html><body><h1>500 Internal Server Error</h1></body></html>";
        }
    }
    HttpResponse resp;
    resp.setStatus(500, "Internal Server Error");
    resp.setHeader("Content-Type", "text/html; charset=utf-8");
    { std::ostringstream oss; oss << body.size(); resp.setHeader("Content-Length", oss.str()); }
    resp.setHeader("Connection", "close");
    resp.setBody(body);
    _wbuf = resp.serialize();
    _status_code = 500;
    _t_write_start = now_ms_conn();
}

void Connection::makeCreated201(const std::string &location, size_t sizeBytes) {
    std::ostringstream body;
    body << "Uploaded " << sizeBytes << " bytes to " << location << "\n";
    HttpResponse resp;
    resp.setStatus(201, "Created");
    resp.setHeader("Connection", "close");
    resp.setHeader("Content-Type", "text/plain; charset=utf-8");
    if (!location.empty()) resp.setHeader("Location", location);
    { std::ostringstream oss; oss << body.str().size(); resp.setHeader("Content-Length", oss.str()); }
    resp.setBody(body.str());
    _wbuf = resp.serialize();
    _status_code = 201;
    _t_write_start = now_ms_conn();
}

void Connection::makeNoContent204() {
    HttpResponse resp;
    resp.setStatus(204, "No Content");
    resp.setHeader("Connection", "close");
    resp.setHeader("Content-Length", "0");
    _wbuf = resp.serialize();
    _status_code = 204;
    _t_write_start = now_ms_conn();
}

// Process any bytes in _rbuf as chunked-encoding data; return false to close
bool Connection::processChunkedBuffered() {
    const size_t CHUNK_LINE_MAX = 8192; // safety cap for chunk-size line and trailer lines
    for (;;) {
        // If reading trailers after 0-chunk, consume until CRLF CRLF
        if (_chunkReadingTrailers) {
            // Accept empty trailers (single CRLF) or non-empty trailers ending with CRLFCRLF
            if (_rbuf.size() >= 2 && _rbuf[0] == '\r' && _rbuf[1] == '\n') {
                // Empty trailer section
                _rbuf.erase(0, 2);
            } else {
                std::string::size_type pos2 = _rbuf.find("\r\n\r\n");
                if (pos2 == std::string::npos) {
                    // Need more bytes
                    return true;
                }
                // Discard trailers
                _rbuf.erase(0, pos2 + 4);
            }
            // Body complete — launch CGI if configured; else same finalize path as fixed length
            if (_cgiEnabled) {
                startCgiCurrent();
                return true;
            }
            if (!_uploadStore.empty()) {
                std::string target = normalize_target_simple(_req.target);
                std::string suffix;
                if (!_matchedLocPath.empty() && target.size() >= _matchedLocPath.size() && target.compare(0, _matchedLocPath.size(), _matchedLocPath) == 0) {
                    suffix = target.substr(_matchedLocPath.size());
                } else {
                    suffix = target;
                }
                std::string name = base_name_only(suffix);
                if (name.empty() || (!suffix.empty() && suffix[suffix.size()-1] == '/')) {
                    name = gen_unique_upload_name();
                } else {
                    name = safe_filename(name);
                }
                std::string full = join_path_simple(_uploadStore, name);
                bool existed = false; struct stat st; if (::stat(full.c_str(), &st) == 0) existed = S_ISREG(st.st_mode);
                FILE *f = std::fopen(full.c_str(), "wb");
                if (!f) { makeError500(); _t_write_start = now_ms_conn(); return true; }
                size_t total = 0; const char *p = _bodyBuf.data(); size_t left = _bodyBuf.size();
                while (left > 0) { size_t w = std::fwrite(p + total, 1, left, f); if (w == 0) break; total += w; left -= w; }
                std::fclose(f);
                if (total != _bodyBuf.size()) { makeError500(); _t_write_start = now_ms_conn(); return true; }
                std::string url = _matchedLocPath; if (url.empty()) url = "/"; if (url[url.size()-1] != '/') url += "/"; url += name;
                if (existed) {
                    std::ostringstream body; body << "Uploaded " << total << " bytes to " << url << " (overwritten)\n";
                    HttpResponse resp; resp.setStatus(200, "OK"); resp.setHeader("Connection", "close"); resp.setHeader("Content-Type", "text/plain; charset=utf-8"); { std::ostringstream oss; oss << body.str().size(); resp.setHeader("Content-Length", oss.str()); } resp.setBody(body.str());
                    _wbuf = resp.serialize(); _status_code = 200; _t_write_start = now_ms_conn();
                } else {
                    makeCreated201(url, total);
                }
                return true;
            } else {
                // Placeholder 200 when no upload_store configured
                std::ostringstream body; body << "Received " << _bodyBuf.size() << " bytes\n";
                HttpResponse resp; resp.setStatus(200, "OK"); resp.setHeader("Connection", "close"); resp.setHeader("Content-Type", "text/plain; charset=utf-8"); { std::ostringstream oss; oss << body.str().size(); resp.setHeader("Content-Length", oss.str()); } resp.setBody(body.str());
                _wbuf = resp.serialize(); _status_code = 200; _t_write_start = now_ms_conn();
                return true;
            }
        }

        // Expecting size line?
        if (_chunkRemaining < 0) {
            // Protect against pathological growth without CRLF
            if (_rbuf.size() > CHUNK_LINE_MAX && _rbuf.find("\r\n") == std::string::npos) { makeError400(); return true; }
            std::string::size_type crlf = _rbuf.find("\r\n");
            if (crlf == std::string::npos) {
                // need more data
                return true;
            }
            std::string line = _rbuf.substr(0, crlf);
            _rbuf.erase(0, crlf + 2);
            if (line.size() > CHUNK_LINE_MAX) { makeError400(); return true; }
            // Strip chunk extensions
            std::string::size_type sc = line.find(';');
            if (sc != std::string::npos) line.erase(sc);
            // Trim spaces
            size_t b=0; while (b < line.size() && (line[b]==' '||line[b]=='\t')) ++b;
            size_t e=line.size(); while (e>b && (line[e-1]==' '||line[e-1]=='\t')) --e;
            std::string hex = line.substr(b, e-b);
            if (hex.empty()) { makeError400(); return true; }
            // Parse hex
            long sz = 0; for (size_t i=0; i<hex.size(); ++i) {
                char c = hex[i]; int v;
                if (c>='0'&&c<='9') v = c - '0';
                else if (c>='a'&&c<='f') v = 10 + (c - 'a');
                else if (c>='A'&&c<='F') v = 10 + (c - 'A');
                else { makeError400(); return true; }
                // avoid overflow
                if (sz > 0x7FFFFFF / 16) { makeError413(); return true; }
                sz = sz * 16 + v;
            }
            if (sz < 0) { makeError400(); return true; }
            _chunkRemaining = sz;
            if (_chunkRemaining == 0) {
                // Last chunk: switch to trailers
                _chunkReadingTrailers = true;
                continue; // next loop to look for CRLF CRLF
            }
            // Otherwise proceed to read chunk data
            continue;
        }

        // We have a chunk length; wait for data + trailing CRLF
        if ((long)_rbuf.size() < _chunkRemaining + 2) {
            // not enough yet
            return true;
        }
        // Enforce body size limit pre-append
        if (_bodyLimit >= 0 && (long)(_bodyBuf.size() + _chunkRemaining) > _bodyLimit) { makeError413(); return true; }
        // Append chunk data
        _bodyBuf.append(_rbuf.data(), (size_t)_chunkRemaining);
        _rbuf.erase(0, (size_t)_chunkRemaining);
        // Verify CRLF
        if (!(_rbuf.size() >= 2 && _rbuf[0] == '\r' && _rbuf[1] == '\n')) { makeError400(); return true; }
        _rbuf.erase(0, 2);
        // Ready for next size line
        _chunkRemaining = -1;
    }
}

bool Connection::onReadable() {
    if (_closed) return false;
    char buf[4096];
    for (;;) {
        ssize_t n = ::recv(_fd, buf, sizeof(buf), 0);
        if (n == 0) { // peer closed
            closeFd();
            return false;
        }
        if (n < 0) {
            // Subject compliance: do not consult errno after recv; treat as no progress now.
            // Break read loop and wait for next poll() readiness or error/timeout handling.
            break;
        }
        _t_last_active = now_ms_conn();

        // If we are in body reading mode, bypass header parser entirely
        if (_headersDone && _bodyState == BODY_FIXED && _clRemaining > 0) {
            size_t take = (n > _clRemaining) ? static_cast<size_t>(_clRemaining) : static_cast<size_t>(n);
            if (_bodyLimit >= 0 && (long)(_bodyBuf.size() + take) > _bodyLimit) {
                makeError413();
                return true;
            }
            _bodyBuf.append(buf, take);
            _clRemaining -= static_cast<long>(take);
            if (_clRemaining == 0) {
                if (_cgiEnabled) { startCgiCurrent(); return true; }
                // Body complete → if upload_store is configured, write to disk; else simple 200 placeholder
                if (!_uploadStore.empty()) {
                    std::string target = normalize_target_simple(_req.target);
                    std::string suffix;
                    if (!_matchedLocPath.empty() && target.size() >= _matchedLocPath.size() && target.compare(0, _matchedLocPath.size(), _matchedLocPath) == 0) {
                        suffix = target.substr(_matchedLocPath.size());
                    } else {
                        suffix = target;
                    }
                    std::string name = base_name_only(suffix);
                    if (name.empty() || (!suffix.empty() && suffix[suffix.size()-1] == '/')) {
                        name = gen_unique_upload_name();
                    } else {
                        name = safe_filename(name);
                    }
                    std::string full = join_path_simple(_uploadStore, name);
                    bool existed = false; struct stat st; if (::stat(full.c_str(), &st) == 0) existed = S_ISREG(st.st_mode);
                    FILE *f = std::fopen(full.c_str(), "wb");
                    if (!f) {
                        makeError500();
                        _t_write_start = now_ms_conn();
                        return true;
                    }
                    size_t total = 0;
                    const char *p = _bodyBuf.data(); size_t left = _bodyBuf.size();
                    while (left > 0) {
                        size_t w = std::fwrite(p + total, 1, left, f);
                        if (w == 0) { break; }
                        total += w; left -= w;
                    }
                    std::fclose(f);
                    if (total != _bodyBuf.size()) {
                        makeError500();
                        _t_write_start = now_ms_conn();
                        return true;
                    }
                    // Build Location URL under the matched location path
                    std::string url = _matchedLocPath;
                    if (url.empty()) url = "/";
                    if (url[url.size()-1] != '/') url += "/";
                    url += name;
                    if (existed) {
                        std::ostringstream body;
                        body << "Uploaded " << total << " bytes to " << url << " (overwritten)\n";
                        HttpResponse resp;
                        resp.setStatus(200, "OK");
                        resp.setHeader("Connection", "close");
                        resp.setHeader("Content-Type", "text/plain; charset=utf-8");
                        { std::ostringstream oss; oss << body.str().size(); resp.setHeader("Content-Length", oss.str()); }
                        resp.setBody(body.str());
                        _wbuf = resp.serialize(); _status_code = 200; _t_write_start = now_ms_conn();
                        return true;
                    } else {
                        makeCreated201(url, total);
                        return true;
                    }
                } else {
                    LOG_WARNF("post complete: upload_store is empty — returning placeholder 200 (no file write)");
                    std::cout << "[trace] post complete: upload_store is empty — returning placeholder 200 (no file write)" << std::endl;
                    std::ostringstream body;
                    body << "Received " << _bodyBuf.size() << " bytes\n";
                    HttpResponse resp;
                    resp.setStatus(200, "OK");
                    resp.setHeader("Connection", "close");
                    resp.setHeader("Content-Type", "text/plain; charset=utf-8");
                    { std::ostringstream oss; oss << body.str().size(); resp.setHeader("Content-Length", oss.str()); }
                    resp.setBody(body.str());
                    _wbuf = resp.serialize();
                    _status_code = 200;
                    _t_write_start = now_ms_conn();
                    return true;
                }
            }
            // If peer sent more than Content-Length, ignore the extra bytes for now
            continue;
        }

        // If reading chunked body, accumulate and process
        if (_headersDone && _bodyState == BODY_CHUNKED) {
            _rbuf.append(buf, n);
            if (!processChunkedBuffered()) return false; // closed
            // If a response was generated, return to write
            if (!_wbuf.empty()) return true;
            continue; // read more
        }

        // Otherwise, feed the header parser
        HttpParser::Result r = _parser.feed(buf, static_cast<size_t>(n));
        if (r == HttpParser::ERROR) {
            // Classify parse error to appropriate status
            HttpParser::ErrorKind ek = _parser.errorKind();
            if (ek == HttpParser::ERR_REQUEST_LINE_TOO_LONG) {
                makeError414();
            } else if (ek == HttpParser::ERR_HEADER_LINE_TOO_LONG || ek == HttpParser::ERR_TOO_MANY_HEADERS) {
                makeError431();
            } else {
                makeError400();
            }
            return true; // switch to write
        }
        if (r == HttpParser::OK) {
            // Snapshot request and mark headers done
            _req = _parser.request();
            const HttpRequest &req = _req;
            _headersDone = true;
            _reqLine = req.method + std::string(" ") + req.target + std::string(" ") + req.version;

            // Vhost selection based on Host header (case-insensitive, strip port)
            std::string host = find_header_icase(req.headers, "Host");
            if (!host.empty()) {
                std::string name = to_lower_copy(strip_port(host));
                for (size_t i = 0; i < _group.size(); ++i) {
                    const ServerConfig *sc = _group[i];
                    if (!sc) continue;
                    for (size_t j = 0; j < sc->server_names.size(); ++j) {
                        if (to_lower_copy(sc->server_names[j]) == name) {
                            if (sc != _srv) {
                                _srv = sc;
                                _root = _srv->root;
                                _index = _srv->index;
                                _vhostName = sc->server_names[j];
                            }
                            goto vhost_done;
                        }
                    }
                }
            }
            vhost_done:

            // (Re)build router if server changed
            if (_srv && _routerSrv != _srv) { _router.build(*_srv); _routerSrv = _srv; }

            // Match location
            RouteMatch match = _router.match(req.target);
            const LocationConfig *loc = match.loc;
            _matchedLocPath = loc ? loc->path : std::string();
            _uploadStore = (loc && !loc->upload_store.empty()) ? loc->upload_store : std::string();

            // Redirect takes precedence if configured
            if (loc && loc->has_redirect) {
                // Preserve suffix policy (normalize simplistic)
                std::string target = req.target;
                if (target.empty() || target[0] != '/') target = std::string("/") + target;
                for (size_t i = 0; i < target.size(); ++i) if (target[i] == '\\') target[i] = '/';
                std::string suffix;
                if (target.size() >= loc->path.size() && target.compare(0, loc->path.size(), loc->path) == 0) {
                    suffix = target.substr(loc->path.size());
                }
                std::string dest = loc->redirect_location + suffix;
                makeRedirect(loc->redirect_code, dest);
                _t_write_start = now_ms_conn();
                return true;
            }

            bool isHead = (req.method == "HEAD");
            bool isGet = (req.method == "GET");
            bool isPost = (req.method == "POST");
            bool isDelete = (req.method == "DELETE");

            // Method filtering
            if (loc && loc->has_methods) {
                bool getAllowed = (loc->methods_mask & 1u) != 0;
                bool postAllowed = (loc->methods_mask & 4u) != 0;
                bool deleteAllowed = (loc->methods_mask & 8u) != 0;
                // HEAD piggybacks on GET allowance
                if ((isGet || isHead) && !getAllowed) {
                    std::string allow;
                    if (loc->methods_mask & 1u) { allow += "GET, HEAD"; }
                    if (loc->methods_mask & 4u) { if (!allow.empty()) allow += ", "; allow += "POST"; }
                    if (loc->methods_mask & 8u) { if (!allow.empty()) allow += ", "; allow += "DELETE"; }
                    if (allow.empty()) allow = "GET, HEAD";
                    makeMethodNotAllowedAllow(allow);
                    _t_write_start = now_ms_conn();
                    return true;
                }
                if (isPost && !postAllowed) {
                    std::string allow;
                    if (loc->methods_mask & 1u) { allow += "GET, HEAD"; }
                    if (loc->methods_mask & 4u) { if (!allow.empty()) allow += ", "; allow += "POST"; }
                    if (loc->methods_mask & 8u) { if (!allow.empty()) allow += ", "; allow += "DELETE"; }
                    if (allow.empty()) allow = "GET, HEAD";
                    makeMethodNotAllowedAllow(allow);
                    _t_write_start = now_ms_conn();
                    return true;
                }
                if (isDelete && !deleteAllowed) {
                    std::string allow;
                    if (loc->methods_mask & 1u) { allow += "GET, HEAD"; }
                    if (loc->methods_mask & 4u) { if (!allow.empty()) allow += ", "; allow += "POST"; }
                    if (loc->methods_mask & 8u) { if (!allow.empty()) allow += ", "; allow += "DELETE"; }
                    if (allow.empty()) allow = "GET, HEAD";
                    makeMethodNotAllowedAllow(allow);
                    _t_write_start = now_ms_conn();
                    return true;
                }
            }

            // Compute effective root/index/autoindex
            std::string effRoot = _root;
            std::vector<std::string> effIndex = _index;
            bool effAutoindex = false;
            long effectiveLimit = -1;
            if (loc) {
                if (loc->has_root) effRoot = loc->root;
                if (loc->has_index) effIndex = loc->index;
                if (loc->has_autoindex) effAutoindex = loc->autoindex;
                if (loc->client_max_body_size_override >= 0) effectiveLimit = loc->client_max_body_size_override;
            }
            if (effectiveLimit < 0 && _srv && _srv->client_max_body_size >= 0) effectiveLimit = _srv->client_max_body_size;
            _cgiEnabled = (loc && !loc->cgi_pass.empty());
            _locCgiPass = _cgiEnabled ? loc->cgi_pass : std::string();
            _locCgiPath = (loc && !loc->cgi_path.empty()) ? loc->cgi_path : std::string();
            _effRootForRequest = effRoot;

            if (isDelete) {
                // Determine base directory for deletion
                std::string base = !_uploadStore.empty() ? _uploadStore : effRoot;
                std::string target = normalize_target_simple(req.target);
                std::string suffix;
                if (!_matchedLocPath.empty() && target.size() >= _matchedLocPath.size() && target.compare(0, _matchedLocPath.size(), _matchedLocPath) == 0) {
                    suffix = target.substr(_matchedLocPath.size());
                } else {
                    suffix = target;
                }
                std::string name = base_name_only(suffix);
                if (name.empty()) {
                    makeError404();
                    _t_write_start = now_ms_conn();
                    return true;
                }
                std::string full = join_path_simple(base, name);
                LOG_INFOF("delete: target path %s", full.c_str());
                std::cout << "[trace] delete: target path " << full << std::endl;
                struct stat st;
                if (::stat(full.c_str(), &st) != 0 || !S_ISREG(st.st_mode)) {
                    makeError404();
                    _t_write_start = now_ms_conn();
                    return true;
                }
                if (::unlink(full.c_str()) == 0) {
                    makeNoContent204();
                } else {
                    makeError500();
                }
                _t_write_start = now_ms_conn();
                return true;
            }

            if (_cgiEnabled && isGet) {
                startCgiCurrent();
                return true;
            }
            if (isGet || isHead) {
                HttpResponse resp;
                std::string err;
                // If a location overrides root, strip the matched prefix from the URL before resolving
                HttpRequest adj = req;
                if (loc && loc->has_root && !loc->path.empty()) {
                    std::string norm = normalize_target_simple(req.target);
                    if (norm.size() >= loc->path.size() && norm.compare(0, loc->path.size(), loc->path) == 0) {
                        std::string suffix = norm.substr(loc->path.size());
                        if (suffix.empty() || suffix[0] != '/') suffix = std::string("/") + suffix;
                        adj.target = suffix;
                        LOG_INFOF("static resolve: stripped prefix '%s' → '%s' under root '%s'", loc->path.c_str(), adj.target.c_str(), effRoot.c_str());
                        std::cout << "[trace] static resolve: strip '" << loc->path << "' → '" << adj.target << "' under root '" << effRoot << "'" << std::endl;
                    }
                }
                if (StaticFileHandler::handle(effRoot, effIndex, adj, isHead, effAutoindex, resp, err)) {
                    _wbuf = resp.serialize();
                    _status_code = 200;
                } else {
                    // Treat unexpected read/autoindex generation failures as 500; missing files as 404
                    if (err.find("read error:") == 0 || err == "autoindex generation failed") {
                        makeError500();
                    } else {
                        makeError404();
                    }
                }
                _t_write_start = now_ms_conn();
                return true; // ready to write
            }

            // POST path — initialize body machine (fixed-length only for now)
            if (isPost) {
                std::string te = find_header_icase(req.headers, "Transfer-Encoding");
                if (!te.empty() && to_lower_copy(te).find("chunked") != std::string::npos) {
                    long effectiveLimit2 = effectiveLimit;
                    _bodyLimit = effectiveLimit2;
                    _bodyState = BODY_CHUNKED;
                    _chunkRemaining = -1; // expect size line first
                    _chunkReadingTrailers = false;
                    _bodyBuf.clear();
                    // Consume any already‑read bytes after headers into _rbuf and process
                    std::string pref2; _parser.takeRemaining(pref2);
                    if (!pref2.empty()) { _rbuf.append(pref2); if (!processChunkedBuffered()) return false; if (!_wbuf.empty()) { _t_write_start = now_ms_conn(); return true; } }
                    // Continue reading more chunked data
                    continue;
                }
                std::string clh = find_header_icase(req.headers, "Content-Length");
                if (clh.empty()) { makeError411(); return true; }
                long cl = 0; { std::istringstream iss(clh); iss >> cl; }
                if (cl < 0) { makeError400(); return true; }
                if (effectiveLimit >= 0 && cl > effectiveLimit) { makeError413(); return true; }
                _bodyLimit = effectiveLimit;
                _bodyState = BODY_FIXED;
                _clRemaining = cl;
                _bodyBuf.clear();
                // Consume any bytes already buffered after headers
                std::string pref;
                _parser.takeRemaining(pref);
                if (!pref.empty()) {
                    size_t take = (pref.size() > (size_t)_clRemaining) ? (size_t)_clRemaining : pref.size();
                    if (_bodyLimit >= 0 && (long)(_bodyBuf.size() + take) > _bodyLimit) { makeError413(); return true; }
                    _bodyBuf.append(pref.data(), take);
                    _clRemaining -= static_cast<long>(take);
                    // ignore any extra bytes beyond Content-Length (no pipelining support in v0)
                }
                if (_clRemaining == 0) {
                    if (_cgiEnabled) { startCgiCurrent(); return true; }
                    // Entire body arrived with headers' remaining bytes; finalize like the normal completion path
                    if (!_uploadStore.empty()) {
                        std::string target = normalize_target_simple(_req.target);
                        std::string suffix;
                        if (!_matchedLocPath.empty() && target.size() >= _matchedLocPath.size() && target.compare(0, _matchedLocPath.size(), _matchedLocPath) == 0) {
                            suffix = target.substr(_matchedLocPath.size());
                        } else {
                            suffix = target;
                        }
                        std::string name = base_name_only(suffix);
                        if (name.empty() || (!suffix.empty() && suffix[suffix.size()-1] == '/')) {
                            name = gen_unique_upload_name();
                        } else {
                            name = safe_filename(name);
                        }
                        std::string full = join_path_simple(_uploadStore, name);
                        bool existed = false; struct stat st; if (::stat(full.c_str(), &st) == 0) existed = S_ISREG(st.st_mode);
                        FILE *f = std::fopen(full.c_str(), "wb");
                        if (!f) {
                            makeError500();
                            _t_write_start = now_ms_conn();
                            return true;
                        }
                        size_t total = 0; const char *p = _bodyBuf.data(); size_t left = _bodyBuf.size();
                        while (left > 0) { size_t w = std::fwrite(p + total, 1, left, f); if (w == 0) break; total += w; left -= w; }
                        std::fclose(f);
                        if (total != _bodyBuf.size()) {
                            makeError500(); _t_write_start = now_ms_conn(); return true;
                        }
                        std::string url = _matchedLocPath; if (url.empty()) url = "/"; if (url[url.size()-1] != '/') url += "/"; url += name;
                        if (existed) {
                            std::ostringstream body2; body2 << "Uploaded " << total << " bytes to " << url << " (overwritten)\n";
                            HttpResponse resp; resp.setStatus(200, "OK"); resp.setHeader("Connection", "close"); resp.setHeader("Content-Type", "text/plain; charset=utf-8"); { std::ostringstream oss; oss << body2.str().size(); resp.setHeader("Content-Length", oss.str()); } resp.setBody(body2.str());
                            _wbuf = resp.serialize(); _status_code = 200; _t_write_start = now_ms_conn(); return true;
                        } else {
                            makeCreated201(url, total); return true;
                        }
                    } else {
                        LOG_WARNF("post complete(pref): upload_store is empty — returning placeholder 200 (no file write)");
                        std::cout << "[trace] post complete(pref): upload_store is empty — returning placeholder 200 (no file write)" << std::endl;
                        std::ostringstream body; body << "Received " << _bodyBuf.size() << " bytes\n";
                        HttpResponse resp; resp.setStatus(200, "OK"); resp.setHeader("Connection", "close"); resp.setHeader("Content-Type", "text/plain; charset=utf-8"); { std::ostringstream oss; oss << body.str().size(); resp.setHeader("Content-Length", oss.str()); } resp.setBody(body.str());
                        _wbuf = resp.serialize(); _status_code = 200; _t_write_start = now_ms_conn(); return true;
                    }
                }
                // otherwise continue reading loop to collect the remaining body
                continue;
            }
        }
        // NEED_MORE: continue reading loop
    }
    return true;
}

bool Connection::onWritable() {
    if (_closed) return false;
    if (_wbuf.empty()) return true;
    for (;;) {
        ssize_t n = ::send(_fd, &_wbuf[0], _wbuf.size(), 0);
        if (n > 0) {
            _t_last_active = now_ms_conn();
            _bytes_sent += (size_t)n;
            _wbuf.erase(_wbuf.begin(), _wbuf.begin() + n);
            if (_wbuf.empty()) {
                closeFd();
                return false;
            }
            continue; // try to send more in this readiness
        }
        if (n == 0) {
            // No progress; wait for next POLLOUT
            return true;
        }
        // n < 0: Subject compliance — do not read errno after send; wait for next poll()/timeout
        return true;
    }
}

void Connection::makeError404() {
    std::string body;
    if (!loadMappedErrorBody(404, body)) {
        if (!readFileToString("www/error/404.html", body)) {
            body = "<html><body><h1>404 Not Found</h1></body></html>";
        }
    }
    HttpResponse resp;
    resp.setStatus(404, "Not Found");
    resp.setHeader("Content-Type", "text/html; charset=utf-8");
    { std::ostringstream oss; oss << body.size(); resp.setHeader("Content-Length", oss.str()); }
    resp.setHeader("Connection", "close");
    resp.setBody(body);
    _wbuf = resp.serialize();
    _status_code = 404;
    _t_write_start = now_ms_conn();
}

void Connection::makeMethodNotAllowed() {
    makeMethodNotAllowedAllow("GET, HEAD");
}

void Connection::makeMethodNotAllowedAllow(const std::string &allow) {
    std::string body;
    if (!loadMappedErrorBody(405, body)) {
        if (!readFileToString("www/error/405.html", body)) {
            body = "<html><body><h1>405 Method Not Allowed</h1></body></html>";
        }
    }
    HttpResponse resp;
    resp.setStatus(405, "Method Not Allowed");
    resp.setHeader("Allow", allow);
    resp.setHeader("Content-Type", "text/html; charset=utf-8");
    { std::ostringstream oss; oss << body.size(); resp.setHeader("Content-Length", oss.str()); }
    resp.setHeader("Connection", "close");
    resp.setBody(body);
    _wbuf = resp.serialize();
    _status_code = 405;
    _t_write_start = now_ms_conn();
}

void Connection::makeRedirect(int code, const std::string &location) {
    std::string body;
    {
        std::ostringstream oss;
        oss << "<html><body><h1>" << code << " Redirect</h1><a href=\"" << location << "\">" << location << "</a></body></html>";
        body = oss.str();
    }
    HttpResponse resp;
    std::string reason = (code == 301 ? "Moved Permanently" : (code == 302 ? "Found" : "Redirect"));
    resp.setStatus(code, reason);
    resp.setHeader("Location", location);
    resp.setHeader("Content-Type", "text/html; charset=utf-8");
    { std::ostringstream oss; oss << body.size(); resp.setHeader("Content-Length", oss.str()); }
    resp.setHeader("Connection", "close");
    resp.setBody(body);
    _wbuf = resp.serialize();
    _status_code = code;
    _t_write_start = now_ms_conn();
}

bool Connection::checkTimeouts(uint64_t now_ms) {
    if (_closed) return false;
    // Reading stage (headers or body)
    if (_wbuf.empty()) {
        bool headersStage = !_headersDone;
        if (headersStage) {
            // Apply both idle and headers timeout while waiting for headers
            uint64_t effHeadersTimeout = HEADERS_TIMEOUT_MS;
            if (_srv && _srv->header_timeout_ms >= 0) effHeadersTimeout = (uint64_t)_srv->header_timeout_ms;
            if ((now_ms - _t_last_active) > IDLE_TIMEOUT_MS || (now_ms - _t_headers_start) > effHeadersTimeout) {
                if (_status_code == 0) {
                    makeError408();
                    return true; // switch to write
                }
            }
        } else {
            // Body stage: only idle timeout applies
            if ((now_ms - _t_last_active) > IDLE_TIMEOUT_MS) {
                if (_status_code == 0) {
                    makeError408();
                    return true;
                }
            }
        }
        return true;
    }
    // CGI execution timeout (e.g., 5s)
    const uint64_t CGI_TIMEOUT_MS = 5000ULL;
    if (_cgiState != CGI_NONE && _cgiState != CGI_DONE && _t_cgi_start != 0 && (now_ms - _t_cgi_start) > CGI_TIMEOUT_MS) {
        LOG_WARNF("cgi timeout for fd=%d after %llu ms", _fd, (unsigned long long)(now_ms - _t_cgi_start));
        abortCgiWithStatus(504, "timeout");
        return true;
    }
    // Writing stage
    if (_t_write_start != 0 && (now_ms - _t_write_start) > WRITE_DRAIN_TIMEOUT_MS) {
        LOG_WARNF("write drain timeout for fd=%d after %llu ms", _fd, (unsigned long long)(now_ms - _t_write_start));
        closeFd();
        return false;
    }
    return true;
}


// --- CGI helpers appended ---
void Connection::closeCgiPipes() {
    if (_cgiIn != -1) { ::close(_cgiIn); _cgiIn = -1; }
    if (_cgiOut != -1) { ::close(_cgiOut); _cgiOut = -1; }
}

void Connection::abortCgiWithStatus(int code, const char *reason) {
    (void)reason;
    if (_cgiPid > 0) { (void)::kill(_cgiPid, SIGKILL); (void)::waitpid(_cgiPid, 0, WNOHANG); _cgiPid = -1; }
    closeCgiPipes();
    std::string body;
    int scode = (code == 504) ? 504 : 502;
    if (!loadMappedErrorBody(scode, body)) {
        if (scode == 504) {
            if (!readFileToString("www/error/504.html", body)) body = "<html><body><h1>504 Gateway Timeout</h1></body></html>";
        } else {
            if (!readFileToString("www/error/502.html", body)) body = "<html><body><h1>502 Bad Gateway</h1></body></html>";
        }
    }
    HttpResponse resp;
    if (scode == 504) resp.setStatus(504, "Gateway Timeout"); else resp.setStatus(502, "Bad Gateway");
    resp.setHeader("Connection", "close");
    resp.setHeader("Content-Type", "text/html; charset=utf-8");
    { std::ostringstream oss; oss << body.size(); resp.setHeader("Content-Length", oss.str()); }
    resp.setBody(body);
    _wbuf = resp.serialize();
    _status_code = scode; _t_write_start = now_ms_conn();
    _cgiState = CGI_DONE;
}

bool Connection::startCgiWith(const std::string &cgiPass, const std::string &cgiPath,
                              const std::string &effRoot, const HttpRequest &req) {
    if (cgiPass.empty()) { makeError500(); _t_write_start = now_ms_conn(); return true; }
    std::string script = cgiPath.empty() ? join_path_simple(effRoot, req.target) : cgiPath;

    int inpipe[2] = { -1, -1 }; int outpipe[2] = { -1, -1 };
    if (::pipe(inpipe) != 0) { makeError500(); _t_write_start = now_ms_conn(); return true; }
    if (::pipe(outpipe) != 0) { ::close(inpipe[0]); ::close(inpipe[1]); makeError500(); _t_write_start = now_ms_conn(); return true; }

    pid_t pid = ::fork();
    if (pid < 0) {
        ::close(inpipe[0]); ::close(inpipe[1]); ::close(outpipe[0]); ::close(outpipe[1]);
        makeError500(); _t_write_start = now_ms_conn(); return true;
    }

    if (pid == 0) {
        // Child
        ::dup2(inpipe[0], STDIN_FILENO);
        ::dup2(outpipe[1], STDOUT_FILENO);
        ::close(inpipe[0]); ::close(inpipe[1]);
        ::close(outpipe[0]); ::close(outpipe[1]);
        // Build environment
        std::vector<std::string> envv; std::vector<char*> envp;
        envv.push_back(std::string("REQUEST_METHOD=") + req.method);
        envv.push_back(std::string("SERVER_PROTOCOL=") + req.version);
        envv.push_back(std::string("SCRIPT_FILENAME=") + script);
        envv.push_back(std::string("SCRIPT_NAME=") + script);
        envv.push_back(std::string("PATH_INFO=") + script);
        std::string sname = _vhostName.empty() ? "localhost" : _vhostName;
        envv.push_back(std::string("SERVER_NAME=") + sname);
        std::string port = ""; { std::string::size_type p = _bindKey.find(':'); if (p != std::string::npos) port = _bindKey.substr(p+1); }
        envv.push_back(std::string("SERVER_PORT=") + port);
        std::string target = _req.target; std::string::size_type q = target.find('?'); std::string qs = (q == std::string::npos) ? std::string("") : target.substr(q + 1);
        envv.push_back(std::string("QUERY_STRING=") + qs);
        std::string ct = find_header_icase(req.headers, "Content-Type"); if (!ct.empty()) envv.push_back(std::string("CONTENT_TYPE=") + ct);
        if (!_bodyBuf.empty()) { std::ostringstream cl; cl << _bodyBuf.size(); envv.push_back(std::string("CONTENT_LENGTH=") + cl.str()); }
        envv.push_back("GATEWAY_INTERFACE=CGI/1.1");
        for (std::map<std::string,std::string>::const_iterator it = req.headers.begin(); it != req.headers.end(); ++it) {
            std::string name = it->first; std::string val = it->second;
            for (size_t i=0;i<name.size();++i){char &c=name[i]; if (c=='-') c='_'; else if (c>='a'&&c<='z') c = (char)(c - 'a' + 'A');}
            envv.push_back(std::string("HTTP_") + name + "=" + val);
        }
        for (size_t i = 0; i < envv.size(); ++i) { envp.push_back(const_cast<char*>(envv[i].c_str())); }
                envp.push_back(0);
        const char *argv0 = cgiPass.c_str();
        std::string argvScript = script;
        std::string::size_type slash = script.find_last_of('/');
        if (slash != std::string::npos) { std::string dir = script.substr(0, slash); (void)::chdir(dir.c_str()); argvScript = script.substr(slash + 1); }
        const char *argv1 = argvScript.c_str();
        char *const argvv[] = { const_cast<char*>(argv0), const_cast<char*>(argv1), 0 };
        ::execve(cgiPass.c_str(), argvv, &envp[0]);
        ::_exit(127);
    }

    // Parent
    _cgiPid = pid; _cgiIn = inpipe[1]; _cgiOut = outpipe[0];
    ::close(inpipe[0]); ::close(outpipe[1]);
    // Non-blocking
    int fl;
    fl = fcntl(_cgiIn, F_GETFL, 0); if (fl != -1) fcntl(_cgiIn, F_SETFL, fl | O_NONBLOCK);
    fl = fcntl(_cgiOut, F_GETFL, 0); if (fl != -1) fcntl(_cgiOut, F_SETFL, fl | O_NONBLOCK);

    _cgiState = CGI_STREAMING; _t_cgi_start = now_ms_conn(); _cgiHeadersDone = false; _cgiStatusFromCGI = 0; _cgiOutputSent = 0; _cgiHdrBuf.clear(); _cgiHdrs.clear();

    if (_loop) {
        _loop->registerAuxFd(_cgiOut, this, POLLIN);
        if (!_bodyBuf.empty()) {
            _loop->registerAuxFd(_cgiIn, this, POLLOUT);
        } else {
            ::close(_cgiIn); _cgiIn = -1;
        }
    }
    return true;
}

bool Connection::startCgiCurrent() {
    return startCgiWith(_locCgiPass, _locCgiPath, _effRootForRequest, _req);
}

bool Connection::onAuxEvent(int fd, short revents) {
    if (_closed) return false;
    const uint64_t tnow = now_ms_conn();
    if (fd == _cgiIn) {
        if (revents & (POLLERR | POLLHUP | POLLNVAL)) {
            if (_loop) _loop->unregisterAuxFd(_cgiIn);
            ::close(_cgiIn); _cgiIn = -1; return true;
        }
        if (revents & POLLOUT) {
            if (!_bodyBuf.empty()) {
                ssize_t n = ::write(_cgiIn, _bodyBuf.data(), _bodyBuf.size());
                if (n > 0) {
                    _bodyBuf.erase(0, (size_t)n); _t_last_active = tnow;
                    if (_bodyBuf.empty()) {
                        if (_loop) _loop->unregisterAuxFd(_cgiIn);
                        ::close(_cgiIn); _cgiIn = -1;
                    }
                }
            } else {
                if (_loop) _loop->unregisterAuxFd(_cgiIn);
                ::close(_cgiIn); _cgiIn = -1;
            }
        }
        return true;
    }
    if (fd == _cgiOut) {
        if (revents & (POLLERR | POLLNVAL)) { abortCgiWithStatus(502, "cgi out err"); return true; }
        if (revents & POLLIN) {
            char buf[4096]; ssize_t n = ::read(_cgiOut, buf, sizeof buf);
            if (n == 0) {
                if (_loop) _loop->unregisterAuxFd(_cgiOut);
                ::close(_cgiOut); _cgiOut = -1;
                if (!_cgiHeadersDone) { abortCgiWithStatus(502, "no headers"); return true; }
                _cgiState = CGI_DONE; (void)::waitpid(_cgiPid, 0, WNOHANG); _cgiPid = -1; return true;
            }
            if (n < 0) { return true; }
            _t_last_active = tnow;
            if (!_cgiHeadersDone) {
                _cgiHdrBuf.append(buf, n);
                std::string::size_type p = _cgiHdrBuf.find("\r\n\r\n");
                if (p == std::string::npos) { if (_cgiHdrBuf.size() > 65536) { abortCgiWithStatus(502, "hdr too big"); } return true; }
                std::string headerBlock = _cgiHdrBuf.substr(0, p);
                std::string rest = _cgiHdrBuf.substr(p + 4);
                _cgiHdrBuf.clear();
                std::istringstream iss(headerBlock);
                std::string line; int code = 200; std::string reason = "OK";
                while (std::getline(iss, line)) {
                    if (!line.empty() && line[line.size()-1] == '\r') line.erase(line.size()-1);
                    if (line.empty()) continue;
                    std::string::size_type c = line.find(":"); if (c == std::string::npos) continue;
                    std::string name = line.substr(0, c);
                    std::string value = line.substr(c+1);
                    size_t b=0; while (b<value.size() && (value[b]==' '||value[b]=='\t')) ++b; size_t e=value.size(); while (e>b && (value[e-1]==' '||value[e-1]=='\t')) --e; value = value.substr(b,e-b);
                    std::string lname = to_lower_copy(name);
                    if (lname == "status") { std::istringstream s(value); s >> code; size_t sp=value.find(' '); if (sp!=std::string::npos) reason=value.substr(sp+1); }
                    else { _cgiHdrs[name] = value; }
                }
                HttpResponse resp; resp.setStatus(code, reason);
                for (std::map<std::string,std::string>::const_iterator it=_cgiHdrs.begin(); it!=_cgiHdrs.end(); ++it) resp.setHeader(it->first, it->second);
                resp.setHeader("Connection", "close");
                std::vector<char> head = resp.serialize();
                _wbuf.insert(_wbuf.end(), head.begin(), head.end());
                _status_code = code; _t_write_start = now_ms_conn(); _cgiHeadersDone = true;
                if (!rest.empty()) { _wbuf.insert(_wbuf.end(), rest.begin(), rest.end()); _cgiOutputSent += rest.size(); if (_cgiOutputSent > CGI_OUTPUT_MAX) { abortCgiWithStatus(502, "out too big"); } }
                return true;
            }
            if (n > 0) {
                if (_cgiOutputSent + (size_t)n > CGI_OUTPUT_MAX) { abortCgiWithStatus(502, "out too big"); return true; }
                _wbuf.insert(_wbuf.end(), buf, buf + n); _cgiOutputSent += (size_t)n;
            }
            return true;
        }
        return true;
    }
    return true;
}

#include "../inc/Request.hpp"
#include <sstream>

Request::Request()
: _state(ReadingHeaders), _content_length(0), _chunked(false), _keep_alive(true) {}

std::string Request::trim(const std::string &s) {
    size_t b = 0, e = s.size();
    while (b < e && (s[b] == ' ' || s[b] == '\t' || s[b] == '\r' || s[b] == '\n')) ++b;
    while (e > b && (s[e-1] == ' ' || s[e-1] == '\t' || s[e-1] == '\r' || s[e-1] == '\n')) --e;
    return s.substr(b, e - b);
}

bool Request::parseStartLineAndHeaders() {
    size_t hdr_end = _buf.find("\r\n\r\n");
    if (hdr_end == std::string::npos) return false;

    std::string headers_part = _buf.substr(0, hdr_end);

    // split lines
    std::istringstream iss(headers_part);
    std::string line;

    if (!std::getline(iss, line)) return false;
    if (!line.empty() && line.size() && line[line.size()-1] == '\r') line.erase(line.size()-1);

    // request line
    {
        std::istringstream rl(line);
        if (!(rl >> _method >> _target >> _version)) {
            _state = Error;
            return true;
        }
    }

    // headers
    while (std::getline(iss, line)) {
        if (!line.empty() && line[line.size()-1] == '\r') line.erase(line.size()-1);
        size_t colon = line.find(':');
        if (colon == std::string::npos) continue;
        std::string key = line.substr(0, colon);
        std::string val = trim(line.substr(colon+1));
        // normalize key to lowercase for easier lookup (simple ASCII)
        for (size_t i=0;i<key.size();++i) if (key[i]>='A'&&key[i]<='Z') key[i] = static_cast<char>(key[i]-'A'+'a');
        _headers[key] = val;
    }

    // keep-alive decision (HTTP/1.1 defaults to keep-alive unless Connection: close)
    std::map<std::string,std::string>::const_iterator itConn = _headers.find("connection");
    if (itConn != _headers.end()) {
        std::string v = itConn->second;
        for (size_t i=0;i<v.size();++i) if (v[i]>='A'&&v[i]<='Z') v[i] = static_cast<char>(v[i]-'A'+'a');
        if (v.find("close") != std::string::npos) _keep_alive = false;
    }

    // Validate HTTP version and Host header (HTTP/1.1 requires Host)
    if (_version != "HTTP/1.1") {
        _state = Error;
        return true;
    }
    if (_headers.find("host") == _headers.end()) {
        _state = Error;
        return true;
    }

    // body framing
    std::map<std::string,std::string>::const_iterator itTE = _headers.find("transfer-encoding");
    if (itTE != _headers.end()) {
        std::string v = itTE->second;
        for (size_t i=0;i<v.size();++i) if (v[i]>='A'&&v[i]<='Z') v[i] = static_cast<char>(v[i]-'A'+'a');
        if (v.find("chunked") != std::string::npos) _chunked = true;
    }

    std::map<std::string,std::string>::const_iterator itCL = _headers.find("content-length");
    if (itCL != _headers.end()) {
        std::istringstream clss(itCL->second);
        size_t v = 0; clss >> v; _content_length = v;
    }

    // erase header part from buffer (including CRLFCRLF)
    _buf.erase(0, hdr_end + 4);

    if (_chunked) {
        // Minimal stub: we don't implement chunked here yet, leave as ReadingBody until external handler processes
        _state = ReadingBody;
    } else if (_content_length > 0) {
        _state = ReadingBody;
    } else {
        _state = Ready;
    }

    return true;
}

Request::State Request::feed(const std::string &data) {
    if (_state == Error || _state == Ready) return _state;
    _buf.append(data);

    if (_state == ReadingHeaders) {
        if (!parseStartLineAndHeaders()) return _state; // need more data
    }

    if (_state == ReadingBody) {
        if (_chunked) {
            // Not implemented here; keep accumulating
            return _state;
        } else {
            if (_buf.size() >= _content_length) {
                _body.assign(_buf.data(), _content_length);
                _buf.erase(0, _content_length);
                _state = Ready;
            }
        }
    }

    return _state;
}

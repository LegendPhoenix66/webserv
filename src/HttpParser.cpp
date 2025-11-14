#include "../inc/HttpParser.hpp"

HttpParser::HttpParser()
		: _errKind(ERR_NONE), _haveStartLine(false), _done(false),
		  _maxStartLine(4096), _maxHeaderLine(8192), _maxHeaders(100) {}

void HttpParser::setLimits(size_t maxStartLine, size_t maxHeaderLine, size_t maxHeaders) {
	_maxStartLine = maxStartLine;
	_maxHeaderLine = maxHeaderLine;
	_maxHeaders = maxHeaders;
}

static std::string trim(const std::string &s) {
	size_t b = 0;
	while (b < s.size() && (s[b] == ' ' || s[b] == '\t')) ++b;
	size_t e = s.size();
	while (e > b && (s[e-1] == ' ' || s[e-1] == '\t')) --e;
	return s.substr(b, e-b);
}

HttpParser::Result HttpParser::feed(const char *data, size_t len) {
	if (_done) return OK; // idempotent after done
	if (data && len) _buf.append(data, len);

	// clear previous error state for this iteration
	_err.clear();
	_errKind = ERR_NONE;

	if (!_haveStartLine) {
		if (!parseStartLine()) {
			if (!_err.empty()) return ERROR;
			return NEED_MORE;
		}
		_haveStartLine = true;
	}
	if (!_done) {
		if (!parseHeaders()) {
			if (!_err.empty()) return ERROR;
			return NEED_MORE;
		}
		_done = true;
		return OK;
	}
	return OK;
}

bool HttpParser::parseStartLine() {
	// Find CRLF
	std::string::size_type pos = _buf.find("\r\n");
	if (pos == std::string::npos) {
		if (_buf.size() > _maxStartLine) {
			_err = "request line too long";
			_errKind = ERR_REQUEST_LINE_TOO_LONG;
			return false;
		}
		return false; // need more
	}
	std::string line = _buf.substr(0, pos);
	_buf.erase(0, pos + 2);
	if (line.size() > _maxStartLine) { _err = "request line too long"; _errKind = ERR_REQUEST_LINE_TOO_LONG; return false; }

	// Split by spaces
	std::istringstream iss(line);
	std::string method, target, version;
	if (!(iss >> method >> target >> version)) {
		_err = "malformed request line";
		_errKind = ERR_MALFORMED_REQUEST;
		return false;
	}
	// Version check
	if (!(version == "HTTP/1.1" || version == "HTTP/1.0")) {
		_err = "unsupported HTTP version";
		_errKind = ERR_BAD_VERSION;
		return false;
	}
	// Minimal token sanity (printable, no control chars)
	for (size_t i = 0; i < method.size(); ++i) if (method[i] < 33 || method[i] > 126) { _err = "bad method"; _errKind = ERR_BAD_METHOD; return false; }

	_req.method = method;
	_req.target = target;
	_req.version = version;
	return true;
}

bool HttpParser::parseHeaders() {
	size_t headers = 0;
	while (true) {
		std::string::size_type pos = _buf.find("\r\n");
		if (pos == std::string::npos) {
			if (_buf.size() > _maxHeaderLine) { _err = "header line too long"; _errKind = ERR_HEADER_LINE_TOO_LONG; return false; }
			return false; // need more
		}
		if (pos == 0) {
			// End of headers
			_buf.erase(0, 2);
			return true;
		}
		std::string line = _buf.substr(0, pos);
		_buf.erase(0, pos + 2);
		if (line.size() > _maxHeaderLine) { _err = "header line too long"; _errKind = ERR_HEADER_LINE_TOO_LONG; return false; }

		// Parse "Name: value"
		std::string::size_type colon = line.find(":");
		if (colon == std::string::npos) { _err = "malformed header"; _errKind = ERR_MALFORMED_HEADER; return false; }
		std::string name = line.substr(0, colon);
		std::string value = line.substr(colon + 1);
		name = trim(name);
		value = trim(value);
		if (name.empty()) { _err = "empty header name"; _errKind = ERR_EMPTY_HEADER_NAME; return false; }
		_req.headers[name] = value;
		++headers;
		if (headers > _maxHeaders) { _err = "too many headers"; _errKind = ERR_TOO_MANY_HEADERS; return false; }
	}
}

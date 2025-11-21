#include "../inc/Connection.hpp"

static const uint64_t IDLE_TIMEOUT_MS = 15000ULL;
static const uint64_t HEADERS_TIMEOUT_MS = 5000ULL;
static const uint64_t WRITE_DRAIN_TIMEOUT_MS = 10000ULL;

Connection::Connection(int fd, const std::vector<const ServerConfig*> &group, const std::string &bindKey, EventLoop* loop)
		: _fd(fd), _closed(false), _group(group), _srv(0), _bindKey(bindKey), _vhostName("-"),_routerSrv(0),
		  _bodyState(BODY_NONE), _bodyLimit(-1), _clRemaining(0),
		  _chunkRemaining(-1), _chunkReadingTrailers(false), _drainAfterResponse(false),
		  _t_start(now_ms()), _t_last_active(_t_start), _t_headers_start(_t_start), _t_write_start(0),
		  _bytes_sent(0), _status_code(0), _logged(false), _reqLine("-"), _peer(peer_of(fd)),
		  _loop(loop), _cgiState(CGI_NONE), _cgiPid(-1), _cgiIn(-1), _cgiOut(-1), _t_cgi_start(0),
		  _cgiHeadersDone(false), _cgiStatusFromCGI(0), _cgiOutputSent(0),
		  _cgiEnabled(false) {
	if (!_group.empty() && _group[0]) {
		_srv = _group[0];
		_root = _srv->getRoot();
		_index = _srv->getIndex();
		_router.build(*_srv);
		_routerSrv = _srv;
		// Apply parser limits from server config (with sane defaults)
		size_t maxRL = (_srv->getClientMaxBodySize() >= 0) ? (size_t)_srv->getClientMaxBodySize() : 4096u;
		size_t maxHL = (_srv->getMaxHeaderSize() >= 0) ? (size_t)_srv->getMaxHeaderSize() : 16384u;
		size_t maxHC = (_srv->getMaxRequestSize() >= 0) ? (size_t)_srv->getMaxRequestSize() : 100u;
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

bool Connection::wantRead() const {
	return !_closed && (_wbuf.empty() || _drainAfterResponse);
}

bool Connection::wantWrite() const {
	return !_closed && !_wbuf.empty();
}

void	Connection::enableDrain() {
	_drainAfterResponse = true;
}

void Connection::logAccess() {
	if (_logged) return;
	uint64_t dur = now_ms() - _t_start;
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

void Connection::closeCgiPipes() {
	if (_cgiIn != -1) { ::close(_cgiIn); _cgiIn = -1; }
	if (_cgiOut != -1) { ::close(_cgiOut); _cgiOut = -1; }
}

std::string Connection::getMimeType(const std::string &path, const bool autoindex) {
	std::string				ext;
	std::string::size_type	dot = path.find_last_of('.');
	if (dot == std::string::npos)
		ext = "";
	else {
		ext = path.substr(dot + 1);
		for (std::string::iterator it = ext.begin(); it != ext.end(); ++it)
			*it = static_cast<char>(std::tolower(static_cast<unsigned char>(*it)));
	}

	if (autoindex) return "text/plain; charset=utf-8";
	if (ext == "html" || ext == "htm") return "text/html; charset=utf-8";
	if (ext == "css") return "text/css; charset=utf-8";
	if (ext == "js") return "application/javascript";
	if (ext == "json") return "application/json";
	if (ext == "png") return "image/png";
	if (ext == "jpg" || ext == "jpeg") return "image/jpeg";
	if (ext == "gif") return "image/gif";
	if (ext == "svg") return "image/svg+xml";
	if (ext == "ico") return "image/x-icon";
	if (ext == "txt") return "text/plain; charset=utf-8";
	return "application/octet-stream";
}

bool	Connection::handle(const std::string &root, const std::vector<std::string> &indexList, const HttpRequest &req, bool isHead,
						   bool autoindex, HttpResponse &outResp, const Location *loc, std::string &err) {
	std::string clean = sanitize(req.target);
	std::string path = join_path(root, clean);
	std::string	lpath = loc ? loc->getPath() : "/";
	std::string	url = ((loc && !loc->getRoot().empty()) ? lpath + clean : clean);

	bool isDir = false;
	if (!file_exists(path, &isDir)) {
		err = "not found";
		return false; // 404
	}

	if (isDir) {
		// Try index files in order
		for (size_t i = 0; i < indexList.size(); ++i) {
			std::string idx = join_path(path, indexList[i]);
			bool isDir2 = false;
			if (file_exists(idx, &isDir2) && !isDir2) {
				path = idx;
				isDir = false;
				break;
			}
		}
		// If still directory, maybe autoindex
		bool isDirFinal = false;
		(void)file_exists(path, &isDirFinal);
		if (isDirFinal) {
			if (!autoindex) {
				err = "index not found";
				return false;
			}
			std::string body;
			if (!generate_autoindex_tree(path, url, body)) {
				err = "autoindex generation failed";
				return false;
			}
			outResp.setStatus(HttpStatusCode::OK);
			outResp.setHeader("Connection", "close");
			outResp.setHeader("Content-Type", "text/html; charset=utf-8");
			{
				std::ostringstream oss; oss << body.size();
				outResp.setHeader("Content-Length", oss.str());
			}
			if (!isHead) outResp.setBody(body);
			return true;
		}
	}

	std::string body;
	long contentLen = 0;
	struct stat st;
	if (::stat(path.c_str(), &st) == 0) {
		contentLen = static_cast<long>(st.st_size);
	}
	if (!isHead) {
		if (!read_file(path, body)) {
			err = std::string("read error: ") + std::strerror(errno);
			return false; // treat as 404/500; for v0 we’ll do 404
		}
		contentLen = static_cast<long>(body.size());
	}

	outResp.setStatus(HttpStatusCode::OK);
	outResp.setHeader("Connection", "close");
	outResp.setHeader("Content-Type", getMimeType(path, autoindex));
	{
		std::ostringstream oss; oss << contentLen;
		outResp.setHeader("Content-Length", oss.str());
	}
	if (!isHead) outResp.setBody(body);
	return true;
}

bool Connection::checkTimeouts(uint64_t now_ms) {
	if (_closed) return false;
	// Reading stage (headers or body)
	if (_wbuf.empty()) {
		bool headersStage = !_headersDone;
		if (headersStage) {
			// Apply both idle and headers timeout while waiting for headers
			//uint64_t effHeadersTimeout = HEADERS_TIMEOUT_MS;
			//if (_srv && _srv->header_timeout_ms >= 0) effHeadersTimeout = (uint64_t)_srv->header_timeout_ms;
			if ((now_ms - _t_last_active) > IDLE_TIMEOUT_MS/* || (now_ms - _t_headers_start) > effHeadersTimeout*/) {
				if (_status_code == 0) {
					returnHttpResponse(HttpStatusCode::RequestTimeout);
					return true; // switch to write
				}
			}
		} else {
			// Body stage: only idle timeout applies
			if ((now_ms - _t_last_active) > IDLE_TIMEOUT_MS) {
				if (_status_code == 0) {
					returnHttpResponse(HttpStatusCode::RequestTimeout);
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
		if (_cgiPid > 0) { (void)::kill(_cgiPid, SIGKILL); (void)::waitpid(_cgiPid, 0, WNOHANG); _cgiPid = -1; }
		closeCgiPipes();
		returnHttpResponse(HttpStatusCode::GatewayTimeout);
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

std::string Connection::errorPageSetup(const HttpStatusCode::e &status_code, std::string &content_type, bool fallback) {
	std::string									body;
	const std::map<int, std::string>			err_pages = _srv->getErrorPages();
	std::map<int, std::string>::const_iterator	it = err_pages.find(statusCodeToInt(status_code));
	if (it != err_pages.end()) {
		std::string	error_page_path = _srv->getRoot() + it->second;
		std::string	error_body_content;
		read_file(error_page_path, error_body_content);
		if (!error_body_content.empty()) {
			read_file(error_page_path, body);
			content_type = getMimeType(error_page_path, false);
		}
	}
	if (body.empty() && fallback) {
		std::ostringstream error_body;
		error_body	<< "<!doctype html><html><head><title>" << statusCodeToInt(status_code)
					  << " " << getStatusMessage(status_code) << "</title></head><body><h1>"
					  << statusCodeToInt(status_code) << "</h1><h2>" << getStatusMessage(status_code)
					  << "</h2></body></html>";
		body = error_body.str();
	}
	return body;
}

void Connection::returnOKResponse(const std::string &body, const std::string &content_type) {
	HttpResponse	resp(HttpStatusCode::OK);
	resp.setHeader("Content-Type", content_type);
	if (!body.empty()) resp.setBody(body);
	std::ostringstream	oss;
	oss << body.size();
	resp.setHeader("Content-Length", oss.str());
	resp.setHeader("Connection", "close");
	_wbuf = resp.serialize();
	_status_code = 200;
	_t_write_start = now_ms();
}

void	Connection::returnHttpResponse(const HttpStatusCode::e &status_code) {
	HttpResponse	resp(status_code);
	std::string		content_type = "text/html; charset=UTF=8";
	std::string body = errorPageSetup(status_code, content_type, true);
	resp.setHeader("Content-Type", content_type);
	resp.setBody(body);
	std::ostringstream	oss;
	oss << body.size();
	resp.setHeader("Content-Length", oss.str());
	resp.setHeader("Connection", "close");
	_wbuf = resp.serialize();
	_status_code = statusCodeToInt(status_code);
	_t_write_start = now_ms();
}

void	Connection::returnHttpResponse(const HttpStatusCode::e &status_code, const Location loc) {
	ReturnDir		returnDir = loc.getReturnDir();
	HttpResponse	resp(status_code);
	std::string		content_type = "text/html; charset=UTF=8";

	if (!returnDir.url.empty())
		resp.setHeader("Location", returnDir.url);
	std::string body = errorPageSetup(status_code, content_type, true);
	resp.setHeader("Content-Type", content_type);
	resp.setBody(body);
	std::ostringstream	oss;
	oss << body.size();
	resp.setHeader("Content-Length", oss.str());
	resp.setHeader("Connection", "close");
	_wbuf = resp.serialize();
	_status_code = statusCodeToInt(status_code);
	_t_write_start = now_ms();
}

void	Connection::returnHttpResponse(const HttpStatusCode::e &status_code, const std::string &allow) {
	HttpResponse	resp(status_code);
	std::string		content_type = "text/html; charset=UTF=8";
	std::string body = errorPageSetup(status_code, content_type, true);
	resp.setHeader("Allow", allow);
	resp.setHeader("Content-Type", content_type);
	resp.setBody(body);
	std::ostringstream	oss;
	oss << body.size();
	resp.setHeader("Content-Length", oss.str());
	resp.setHeader("Connection", "close");
	_wbuf = resp.serialize();
	_status_code = statusCodeToInt(status_code);
	_t_write_start = now_ms();
}

void Connection::returnCreatedResponse(const std::string &location, const size_t sizeBytes) {
	HttpResponse	resp(HttpStatusCode::Created);
	std::string		content_type = "text/plain; charset=utf-8";
	std::string		body = errorPageSetup(HttpStatusCode::Created, content_type, false);
	if (body.empty()) {
		std::ostringstream	bss;
		bss << "Uploaded " << sizeBytes << " bytes to " << location << std::endl;
		body = bss.str();
	}
	if (!location.empty())
		resp.setHeader("Location", location);
	resp.setHeader("Content-Type", content_type);
	resp.setBody(body);
	std::ostringstream	oss;
	oss << body.size();
	resp.setHeader("Content-Length", oss.str());
	resp.setHeader("Connection", "close");
	_wbuf = resp.serialize();
	_status_code = 201;
	_t_write_start = now_ms();
}

void	Connection::returnHttpResponse(const HttpRequest &req, const ReturnDir &dir, const Location *loc) {
	HttpResponse	resp(getStatusCode(dir.code));
	// Preserve suffix policy (normalize simplistic)
	std::string target = req.target;
	if (target.empty() || target[0] != '/') target = std::string("/") + target;
	for (size_t i = 0; i < target.size(); ++i) if (target[i] == '\\') target[i] = '/';
	std::string suffix;
	if (target.size() >= loc->getPath().size() && target.compare(0, loc->getPath().size(), loc->getPath()) == 0) {
		suffix = target.substr(loc->getPath().size());
	}
	std::string	dest = loc->getReturnDir().url + suffix;
	resp.setHeader("Location", dest);
	resp.setHeader("Content-Length", "0");
	resp.setHeader("Connection", "close");
	_wbuf = resp.serialize();
	_status_code = dir.code;
	_t_write_start = now_ms();
}

bool Connection::startCgiCurrent() {
	return startCgiWith(_locCgiPass, _locCgiPath, _effRootForRequest, _req);
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
				if (!f) {
					returnHttpResponse(HttpStatusCode::InternalServerError);
					return true;
				}
				size_t total = 0; const char *p = _bodyBuf.data(); size_t left = _bodyBuf.size();
				while (left > 0) {
					size_t w = std::fwrite(p + total, 1, left, f); if (w == 0) break;
					total += w;
					left -= w;
				}
				std::fclose(f);
				if (total != _bodyBuf.size()) {
					returnHttpResponse(HttpStatusCode::InternalServerError);
					return true;
				}
				std::string url = _matchedLocPath; if (url.empty()) url = "/"; if (url[url.size()-1] != '/') url += "/"; url += name;
				if (existed) {
					std::ostringstream body; body << "Uploaded " << total << " bytes to " << url << " (overwritten)\n";
					returnOKResponse(body.str(), "text/plain; charset=utf-8");
				} else {
					returnCreatedResponse(url, total);
				}
				return true;
			} else {
				// Placeholder 200 when no upload_store configured
				std::ostringstream body; body << "Received " << _bodyBuf.size() << " bytes\n";
				returnOKResponse(body.str(), "text/plain; charset=utf-8");
				return true;
			}
		}

		// Expecting size line?
		if (_chunkRemaining < 0) {
			// Protect against pathological growth without CRLF
			if (_rbuf.size() > CHUNK_LINE_MAX && _rbuf.find("\r\n") == std::string::npos) {
				returnHttpResponse(HttpStatusCode::BadRequest);
				return true;
			}
			std::string::size_type crlf = _rbuf.find("\r\n");
			if (crlf == std::string::npos) {
				// need more data
				return true;
			}
			std::string line = _rbuf.substr(0, crlf);
			_rbuf.erase(0, crlf + 2);
			if (line.size() > CHUNK_LINE_MAX) {
				returnHttpResponse(HttpStatusCode::BadRequest);
				return true;
			}
			// Strip chunk extensions
			std::string::size_type sc = line.find(';');
			if (sc != std::string::npos) line.erase(sc);
			// Trim spaces
			size_t b=0;
			while (b < line.size() && (line[b]==' '||line[b]=='\t')) ++b;
			size_t e=line.size();
			while (e>b && (line[e-1]==' '||line[e-1]=='\t')) --e;
			std::string hex = line.substr(b, e-b);
			if (hex.empty()) {
				returnHttpResponse(HttpStatusCode::BadRequest);
				return true;
			}
			// Parse hex
			long sz = 0; for (size_t i=0; i<hex.size(); ++i) {
				char c = hex[i]; int v;
				if (c>='0'&&c<='9') v = c - '0';
				else if (c>='a'&&c<='f') v = 10 + (c - 'a');
				else if (c>='A'&&c<='F') v = 10 + (c - 'A');
				else {
					returnHttpResponse(HttpStatusCode::BadRequest);
					return true;
				}
				// avoid overflow
				if (sz > 0x7FFFFFF / 16) {
					enableDrain();
					returnHttpResponse(HttpStatusCode::ContentTooLarge);
					return true;
				}
				sz = sz * 16 + v;
			}
			if (sz < 0) {
				returnHttpResponse(HttpStatusCode::BadRequest);
				return true;
			}
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
		if (_bodyLimit >= 0 && (long)(_bodyBuf.size() + _chunkRemaining) > _bodyLimit) {
			enableDrain();
			returnHttpResponse(HttpStatusCode::ContentTooLarge);
			return true;
		}
		// Append chunk data
		_bodyBuf.append(_rbuf.data(), (size_t)_chunkRemaining);
		_rbuf.erase(0, (size_t)_chunkRemaining);
		// Verify CRLF
		if (!(_rbuf.size() >= 2 && _rbuf[0] == '\r' && _rbuf[1] == '\n')) {
			returnHttpResponse(HttpStatusCode::BadRequest);
			return true;
		}
		_rbuf.erase(0, 2);
		// Ready for next size line
		_chunkRemaining = -1;
	}
}

int	Connection::uploadAndRespond() {
	if (_cgiEnabled) {
		startCgiCurrent();
		return 1;
	}
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
		bool existed = false;
		struct stat st;
		if (::stat(full.c_str(), &st) == 0)
			existed = S_ISREG(st.st_mode);
		FILE *f = std::fopen(full.c_str(), "wb");
		if (!f) {
			returnHttpResponse(HttpStatusCode::InternalServerError);
			return 1;
		}
		size_t total = 0;
		const char *p = _bodyBuf.data();
		size_t left = _bodyBuf.size();
		while (left > 0) {
			size_t w = std::fwrite(p + total, 1, left, f);
			if (w == 0) break;
			total += w;
			left -= w;
		}
		std::fclose(f);
		if (total != _bodyBuf.size()) {
			returnHttpResponse(HttpStatusCode::InternalServerError);
			return 1;
		}
		// Build Location URL under the matched location path
		std::string url = _matchedLocPath;
		if (url.empty()) url = "/";
		if (url[url.size()-1] != '/') url += "/";
		url += name;
		if (existed) {
			std::ostringstream body;
			body << "Uploaded " << total << " bytes to " << url << " (overwritten)\n";
			returnOKResponse(body.str(), "text/plain; charset=utf-8");
			return 1;
		} else {
			returnCreatedResponse(url, total);
			return 1;
		}
	} else {
		LOG_WARNF("post complete: upload_store is empty — returning placeholder 200 (no file write)");
		std::cout << "[trace] post complete: upload_store is empty — returning placeholder 200 (no file write)" << std::endl;
		std::ostringstream body;
		body << "Received " << _bodyBuf.size() << " bytes\n";
		returnOKResponse(body.str(), "text/plain; charset=utf-8");
		return 1;
	}
}

int	Connection::handleFixedBodyChunk(const char *buf, ssize_t n) {
	size_t take = (n > _clRemaining) ? static_cast<size_t>(_clRemaining) : static_cast<size_t>(n);
	if (_bodyLimit >= 0 && (long)(_bodyBuf.size() + take) > _bodyLimit) {
		enableDrain();
		returnHttpResponse(HttpStatusCode::ContentTooLarge);
		return 1;
	}
	_bodyBuf.append(buf, take);
	_clRemaining -= static_cast<long>(take);
	if (_clRemaining == 0) {
		return uploadAndRespond();
	}
	return 0;
}

void	Connection::selectVhost(const HttpRequest &req) {
	// Vhost selection based on Host header (case-insensitive, strip port)
	std::string host = find_header_icase(req.headers, "Host");
	if (!host.empty()) {
		std::string name = to_lower_copy(strip_port(host));
		for (size_t i = 0; i < _group.size(); ++i) {
			const ServerConfig *sc = _group[i];
			if (!sc) continue;
			for (size_t j = 0; j < sc->getServerName().size(); ++j) {
				if (to_lower_copy(sc->getServerName()[j]) == name) {
					if (sc != _srv) {
						_srv = sc;
						_root = _srv->getRoot();
						_index = _srv->getIndex();
						_vhostName = sc->getServerName()[j];
					}
					return;
				}
			}
		}
	}
}

bool	Connection::deleteMethod(const std::string &effRoot, const HttpRequest &req) {
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
		returnHttpResponse(HttpStatusCode::NotFound);
		return true;
	}
	std::string full = join_path_simple(base, name);
	LOG_INFOF("delete: target path %s", full.c_str());
	std::cout << "[trace] delete: target path " << full << std::endl;
	struct stat st;
	if (::stat(full.c_str(), &st) != 0 || !S_ISREG(st.st_mode)) {
		returnHttpResponse(HttpStatusCode::NotFound);
		return true;
	}
	if (::unlink(full.c_str()) == 0) {
		returnHttpResponse(HttpStatusCode::NoContent);
	} else {
		returnHttpResponse(HttpStatusCode::InternalServerError);
	}
	return true;
}

bool	Connection::getMethod(const HttpRequest &req, const Location *loc, std::string &effRoot,
							  std::vector<std::string> &effIndex, bool isHead, bool effAutoindex) {
	HttpResponse resp;
	std::string err;
	// If a location overrides root, strip the matched prefix from the URL before resolving
	HttpRequest adj = req;
	if (loc && !loc->getRoot().empty() && !loc->getPath().empty()) {
		std::string norm = normalize_target_simple(req.target);
		std::string	lpath = loc->getPath();
		if (!lpath.empty() && lpath[lpath.size() - 1] != '/')
			lpath += '/';
		if (norm.size() >= lpath.size() && norm.compare(0, lpath.size(), lpath) == 0) {
			std::string suffix = norm.substr(lpath.size());
			if (suffix.empty()) suffix = "/";
			if (suffix[0] != '/') suffix = std::string("/") + suffix;
			adj.target = suffix;
			LOG_INFOF("static resolve: stripped prefix '%s' → '%s' under root '%s'", lpath.c_str(), adj.target.c_str(), effRoot.c_str());
			std::cout << "[trace] static resolve: strip '" << lpath << "' → '" << adj.target << "' under root '" << effRoot << "'" << std::endl;
		}
	}
	if (handle(effRoot, effIndex, adj, isHead, effAutoindex, resp, loc, err)) {
		_wbuf = resp.serialize();
		_status_code = 200;
		_t_write_start = now_ms();
	} else {
		// Treat unexpected read/autoindex generation failures as 500; missing files as 404
		if (err.find("read error:") == 0 || err == "autoindex generation failed") {
			returnHttpResponse(HttpStatusCode::InternalServerError);
		} else {
			returnHttpResponse(HttpStatusCode::NotFound);
		}
	}
	return true; // ready to write
}

int	Connection::postMethod(const HttpRequest &req, const long effectiveLimit) {
	std::string te = find_header_icase(req.headers, "Transfer-Encoding");
	if (!te.empty() && to_lower_copy(te).find("chunked") != std::string::npos) {
		_bodyLimit = effectiveLimit;
		_bodyState = BODY_CHUNKED;
		_chunkRemaining = -1; // expect size line first
		_chunkReadingTrailers = false;
		_bodyBuf.clear();
		// Consume any already‑read bytes after headers into _rbuf and process
		std::string pref2;
		_parser.takeRemaining(pref2);
		if (!pref2.empty()) {
			_rbuf.append(pref2);
			if (!processChunkedBuffered()) return -1;
			if (!_wbuf.empty()) {
				_t_write_start = now_ms();
				return 1;
			}
		}
		// Continue reading more chunked data
		return 0;
	}
	std::string clh = find_header_icase(req.headers, "Content-Length");
	if (clh.empty()) {
		returnHttpResponse(HttpStatusCode::LengthRequired);
		return 1;
	}
	long cl = 0;
	{ std::istringstream iss(clh); iss >> cl; }
	if (cl < 0) {
		returnHttpResponse(HttpStatusCode::BadRequest);
		return 1;
	}
	if (effectiveLimit > 0 && cl > effectiveLimit) {
		enableDrain();
		returnHttpResponse(HttpStatusCode::ContentTooLarge);
		return 1;
	}
	_bodyLimit = effectiveLimit;
	_bodyState = BODY_FIXED;
	_clRemaining = cl;
	_bodyBuf.clear();
	// Consume any bytes already buffered after headers
	std::string pref;
	_parser.takeRemaining(pref);
	if (!pref.empty()) {
		size_t take = (pref.size() > (size_t)_clRemaining) ? (size_t)_clRemaining : pref.size();
		if (_bodyLimit >= 0 && (long)(_bodyBuf.size() + take) > _bodyLimit) {
			enableDrain();
			returnHttpResponse(HttpStatusCode::ContentTooLarge);
			return 1;
		}
		_bodyBuf.append(pref.data(), take);
		_clRemaining -= static_cast<long>(take);
		// ignore any extra bytes beyond Content-Length (no pipelining support in v0)
	}
	if (_clRemaining == 0) {
		return uploadAndRespond();
	}
	return 0;
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
		_t_last_active = now_ms();

		if (_drainAfterResponse) continue;

		// If we are in body reading mode, bypass header parser entirely
		if (_headersDone && _bodyState == BODY_FIXED && _clRemaining > 0) {
			int	hr = handleFixedBodyChunk(buf, n);
			if (hr == 1) return true;
			if (hr == -1) return false;
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
				returnHttpResponse(HttpStatusCode::URITooLong);
			} else if (ek == HttpParser::ERR_HEADER_LINE_TOO_LONG || ek == HttpParser::ERR_TOO_MANY_HEADERS) {
				returnHttpResponse(HttpStatusCode::RequestHeaderFieldsTooLarge);
			} else {
				returnHttpResponse(HttpStatusCode::BadRequest);
			}
			return true; // switch to write
		}
		if (r == HttpParser::OK) {
			// Snapshot request and mark headers done
			_req = _parser.request();
			const HttpRequest &req = _req;
			_headersDone = true;
			_reqLine = req.method + std::string(" ") + req.target + std::string(" ") + req.version;
			selectVhost(req);

			// (Re)build router if server changed
			if (_srv && _routerSrv != _srv) { _router.build(*_srv); _routerSrv = _srv; }

			// Match location
			RouteMatch match = _router.match(req.target);
			const Location *loc = match.loc;
			_matchedLocPath = loc ? loc->getPath() : std::string();
			_uploadStore = (loc && !loc->getUploadStore().empty()) ? loc->getUploadStore() : std::string();

			// Redirect takes precedence if configured
			if (loc && loc->hasReturnDir()) {
				returnHttpResponse(req, loc->getReturnDir(), loc);
				return true;
			}

			bool isHead = (req.method == "HEAD");
			bool isGet = (req.method == "GET");
			bool isPost = (req.method == "POST");
			bool isDelete = (req.method == "DELETE");

			// Method filtering
			if (loc && !loc->getAllowedMethods().empty()) {
				bool	getAllowed = loc->findMethod("GET") != std::string::npos;
				bool	postAllowed = loc->findMethod("POST") != std::string::npos;
				bool	deleteAllowed = loc->findMethod("DELETE") != std::string::npos;
				bool	headAllowed = getAllowed || loc->findMethod("HEAD") != std::string::npos;

				bool	disallowed = ((isGet || isHead) && !getAllowed) || (isPost && !postAllowed) || (isDelete && !deleteAllowed);

				if (disallowed) {
					std::string	allow;
					if (getAllowed)
						allow += "GET, HEAD";
					else if (headAllowed)
						allow += "HEAD";

					if (postAllowed) {
						if (!allow.empty()) allow += ", ";
						allow += "POST";
					}
					if (deleteAllowed) {
						if (!allow.empty()) allow += ", ";
						allow += "DELETE";
					}

					if (allow.empty()) allow = "GET, HEAD";
					returnHttpResponse(HttpStatusCode::MethodNotAllowed, allow);
					return true;
				}
			}
			// Compute effective root/index/autoindex
			std::string effRoot = _root;
			std::vector<std::string> effIndex = _index;
			bool effAutoindex = false;
			long effectiveLimit = -1;
			if (loc) {
				if (!loc->getRoot().empty()) effRoot = loc->getRoot();
				if (!loc->getIndex().empty()) effIndex = loc->getIndex();
				if (loc->getAutoindex()) effAutoindex = loc->getAutoindex();
				if (loc->getClientMaxBodySize() >= 0) effectiveLimit = (size_t)loc->getClientMaxBodySize();
			}
			if (effectiveLimit < 0 && _srv && _srv->getClientMaxBodySize() > 0) effectiveLimit = (size_t)_srv->getClientMaxBodySize();
			_cgiEnabled = (loc && !loc->getCgiPass().empty());
			_locCgiPass = _cgiEnabled ? loc->getCgiPass() : std::string();
			_locCgiPath = (loc && !loc->getCgiPath().empty()) ? loc->getCgiPath() : std::string();
			_effRootForRequest = effRoot;

			if (isDelete) {
				return deleteMethod(effRoot, req);
			}
			if (_cgiEnabled && isGet) {
				startCgiCurrent();
				return true;
			}
			if (isGet || isHead) {
				return getMethod(req, loc, effRoot, effIndex, isHead, effAutoindex);
			}
			// POST path — initialize body machine (fixed-length only for now)
			if (isPost) {
				int	hr = postMethod(req, effectiveLimit);
				if (hr == 1) return true;
				if (hr == -1) return false;
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
			_t_last_active = now_ms();
			_bytes_sent += (size_t)n;
			_wbuf.erase(_wbuf.begin(), _wbuf.begin() + n);
			if (_wbuf.empty()) {
				if (_drainAfterResponse) {
					if (_t_write_start == 0) _t_write_start = now_ms();
					return true;
				}
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

bool Connection::startCgiWith(const std::string &cgiPass, const std::string &cgiPath,
							  const std::string &effRoot, const HttpRequest &req) {
	if (cgiPass.empty()) { returnHttpResponse(HttpStatusCode::InternalServerError); return true; }
	std::string script = cgiPath.empty() ? join_path_simple(effRoot, req.target) : cgiPath;

	int inpipe[2] = { -1, -1 }; int outpipe[2] = { -1, -1 };
	if (::pipe(inpipe) != 0) { returnHttpResponse(HttpStatusCode::InternalServerError); return true; }
	if (::pipe(outpipe) != 0) { ::close(inpipe[0]); ::close(inpipe[1]); returnHttpResponse(HttpStatusCode::InternalServerError); return true; }

	pid_t pid = ::fork();
	if (pid < 0) {
		::close(inpipe[0]); ::close(inpipe[1]); ::close(outpipe[0]); ::close(outpipe[1]);
		returnHttpResponse(HttpStatusCode::InternalServerError); return true;
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
		std::string ct = find_header_icase(req.headers, "Content-Type");
		if (!ct.empty()) envv.push_back(std::string("CONTENT_TYPE=") + ct);
		if (!_bodyBuf.empty()) {
			std::ostringstream cl;
			cl << _bodyBuf.size();
			envv.push_back(std::string("CONTENT_LENGTH=") + cl.str());
		}
		else {
			envv.push_back(std::string("CONTENT_LENGTH=0"));
		}
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
		if (slash != std::string::npos) {
			std::string dir = script.substr(0, slash);
			(void)::chdir(dir.c_str());
			argvScript = script.substr(slash + 1);
		}
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

	_cgiState = CGI_STREAMING; _t_cgi_start = now_ms(); _cgiHeadersDone = false; _cgiStatusFromCGI = 0; _cgiOutputSent = 0; _cgiHdrBuf.clear(); _cgiHdrs.clear();

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

bool Connection::onAuxEvent(int fd, short revents) {
	if (_closed) return false;
	const uint64_t tnow = now_ms();
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
		if (revents & (POLLERR | POLLNVAL)) {
			if (_cgiPid > 0) { (void)::kill(_cgiPid, SIGKILL); (void)::waitpid(_cgiPid, 0, WNOHANG); _cgiPid = -1; }
			closeCgiPipes();
			returnHttpResponse(HttpStatusCode::BadGateway);
			return true;
		}
		if (revents & POLLIN) {
			char buf[4096];
			ssize_t n = ::read(_cgiOut, buf, sizeof buf);
			if (n == 0) {
				if (_loop) _loop->unregisterAuxFd(_cgiOut);
				::close(_cgiOut); _cgiOut = -1;
				if (!_cgiHeadersDone) {
					if (_cgiPid > 0) { (void)::kill(_cgiPid, SIGKILL); (void)::waitpid(_cgiPid, 0, WNOHANG); _cgiPid = -1; }
					closeCgiPipes();
					returnHttpResponse(HttpStatusCode::BadGateway);
					return true;
				}

				_cgiState = CGI_DONE;
				(void)::waitpid(_cgiPid, 0, WNOHANG);
				_cgiPid = -1; return true;
			}
			if (n < 0) { return true; }
			_t_last_active = tnow;
			if (!_cgiHeadersDone) {
				_cgiHdrBuf.append(buf, n);
				std::string::size_type p = _cgiHdrBuf.find("\r\n\r\n");
				if (p == std::string::npos) {
					if (_cgiHdrBuf.size() > 65536) {
						if (_cgiPid > 0) { (void)::kill(_cgiPid, SIGKILL); (void)::waitpid(_cgiPid, 0, WNOHANG); _cgiPid = -1; }
						closeCgiPipes();
						returnHttpResponse(HttpStatusCode::BadGateway);
					}
					return true;
				}
				std::string headerBlock = _cgiHdrBuf.substr(0, p);
				std::string rest = _cgiHdrBuf.substr(p + 4);
				_cgiHdrBuf.clear();
				std::istringstream iss(headerBlock);
				std::string line; int code = 200;
				while (std::getline(iss, line)) {
					if (!line.empty() && line[line.size()-1] == '\r') line.erase(line.size()-1);
					if (line.empty()) continue;
					std::string::size_type c = line.find(":"); if (c == std::string::npos) continue;
					std::string name = line.substr(0, c);
					std::string value = line.substr(c+1);
					size_t b=0; while (b<value.size() && (value[b]==' '||value[b]=='\t')) ++b; size_t e=value.size(); while (e>b && (value[e-1]==' '||value[e-1]=='\t')) --e; value = value.substr(b,e-b);
					std::string lname = to_lower_copy(name);
					if (lname == "status") { std::istringstream s(value); s >> code; }
					else { _cgiHdrs[name] = value; }
				}
				HttpResponse resp(getStatusCode(code));
				for (std::map<std::string,std::string>::const_iterator it=_cgiHdrs.begin(); it!=_cgiHdrs.end(); ++it) resp.setHeader(it->first, it->second);
				resp.setHeader("Connection", "close");
				std::vector<char> head = resp.serialize();
				_wbuf.insert(_wbuf.end(), head.begin(), head.end());
				_status_code = code; _t_write_start = now_ms(); _cgiHeadersDone = true;
				if (!rest.empty()) {
					_wbuf.insert(_wbuf.end(), rest.begin(), rest.end());
					_cgiOutputSent += rest.size();
					if (_cgiOutputSent > CGI_OUTPUT_MAX) {
						if (_cgiPid > 0) { (void)::kill(_cgiPid, SIGKILL); (void)::waitpid(_cgiPid, 0, WNOHANG); _cgiPid = -1; }
						closeCgiPipes();
						returnHttpResponse(HttpStatusCode::BadGateway);
					}
				}
				return true;
			}
			if (n > 0) {
				if (_cgiOutputSent + (size_t)n > CGI_OUTPUT_MAX) {
					if (_cgiPid > 0) {
						(void)::kill(_cgiPid, SIGKILL);
						(void)::waitpid(_cgiPid, 0, WNOHANG);
						_cgiPid = -1;
					}
					closeCgiPipes();
					returnHttpResponse(HttpStatusCode::BadGateway);
					return true;
				}
				_wbuf.insert(_wbuf.end(), buf, buf + n);
				_cgiOutputSent += (size_t)n;
			}
			return true;
		}
		return true;
	}
	return true;
}

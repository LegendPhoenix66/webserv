#ifndef CONNECTION_HPP
#define CONNECTION_HPP

#include <string>
#include <vector>
#include <map>
#include <stdint.h>
#include <sys/types.h>
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
#include <poll.h>
#include <cctype>

#include "EventLoop.hpp"
#include "Logger.hpp"
#include "ServerConfig.hpp"
#include "HttpParser.hpp"
#include "HttpResponse.hpp"
#include "Router.hpp"
#include "HttpStatusCodes.hpp"
#include "LoopUtils.hpp"
#include "ConnectionUtils.hpp"

class EventLoop;

class Connection {
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

	bool	_drainAfterResponse;

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
	std::string	_locCgiPass;
	std::string	_locCgiPath;
	std::string _effRootForRequest;

	std::string	_matchedLocPath;
	std::string	_uploadStore;

	bool saveMultipart(const std::string &content_type, std::string &outName, std::string &err, bool &existed);
	int		uploadAndRespond();

	int		handleFixedBodyChunk(const char *buf, ssize_t n);
	void	selectVhost(const HttpRequest &req);

	bool	getMethod(const HttpRequest &req, const Location *loc, std::string &effRoot,
				 		std::vector<std::string> &effIndex, bool isHead, bool effAutoindex);
	bool	deleteMethod(const std::string &effRoot, const HttpRequest &req);
	int		postMethod(const HttpRequest &req, const long effectiveLimit);

	void	logAccess();

	bool	startCgiCurrent();
	void	closeCgiPipes();

	void closeFd();
	std::string getMimeType(const std::string &path, const bool autoindex);
	std::string errorPageSetup(const HttpStatusCode::e &status_code, std::string &content_type, bool fallback);
	void	returnOtherResponse(const HttpStatusCode::e &status_code, const std::string &location);
	void	returnHttpResponse(const HttpStatusCode::e &status_code);
	void	returnHttpResponse(const HttpStatusCode::e &status_code, const std::string &allow);
	void returnHttpResponse(const ReturnDir &dir);
	void returnCreatedResponse(const std::string &location, const size_t sizeBytes);
	void returnOKResponse(std::string body, std::string content_type);

	bool	handle(const std::string &root, const std::vector<std::string> &indexList, const HttpRequest &req, bool isHead,
				   bool autoindex, HttpResponse &outResp, const Location *loc, std::string &err);

	bool	startCgiWith(const std::string &cgiPass, const std::string &cgiPath,
								  const std::string &effRoot, const HttpRequest &req);
public:
	// Construct a connection associated with a listener's vhost group and bind key.
	explicit Connection(int fd, const std::vector<const ServerConfig*> &group, const std::string &bindKey, EventLoop* loop);
	~Connection();
	int	fd() const { return _fd; }

	// Desired poll events
	bool	wantRead() const;
	bool	wantWrite() const;

	void	enableDrain();

	// Event handlers return false on fatal/close
	bool	onReadable();
	bool	onWritable();

	// Auxiliary (CGI) fds readiness; return false to close client
	bool	onAuxEvent(int fd, short revents);

	// Timeout sweep hook; returns true to keep, false to remove/close
	bool	checkTimeouts(uint64_t now_ms);

	bool	isClosed() const { return _closed; }
};


#endif

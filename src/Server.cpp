#include "../inc/Server.hpp"
#include <sstream>
#include <fstream>
#include <arpa/inet.h>
#include <netdb.h>

Server::Server()
		: listen_fd(-1) {
}

Server::Server(const Server &other)
		: listen_fd(other.listen_fd),
		  config(other.config) {
}

Server &Server::operator=(Server other) {
	this->swap(other);
	return *this;
}

Server::~Server() {
}

Server::Server(const ServerConfig &config)
		: listen_fd(-1),
		  config(config) {
}

void Server::swap(Server &other) {
	std::swap(this->config, other.config);
	std::swap(this->listen_fd, other.listen_fd);
}

void Server::start() {
	// Non-blocking initializer: only set up the listening socket.
	setupListenSocket();
}

int Server::setupListenSocket() {
	// Create socket
	listen_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (listen_fd < 0) {
		std::cerr << "socket() failed" << std::endl;
		return -1;
	}

	// Set non-blocking
	fcntl(listen_fd, F_SETFL, O_NONBLOCK);

	// Set socket options (reuse address)
	int opt = 1;
	setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

	// Bind to host/port from config
	sockaddr_in addr;
	std::memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	// Apply configured host if valid IPv4 or resolvable name; fallback to INADDR_ANY
	{
		std::string host = config.getHost();
		if (!host.empty()) {
			in_addr ina; std::memset(&ina, 0, sizeof(ina));
			// Try dotted-quad first
			if (inet_aton(host.c_str(), &ina)) {
				addr.sin_addr = ina;
			} else {
				// Try DNS resolution
				hostent* he = gethostbyname(host.c_str());
				if (he && he->h_addrtype == AF_INET && he->h_length == (int)sizeof(in_addr)) {
					std::memcpy(&addr.sin_addr, he->h_addr, sizeof(in_addr));
				} else {
					addr.sin_addr.s_addr = INADDR_ANY;
				}
			}
		} else {
			addr.sin_addr.s_addr = INADDR_ANY;
		}
	}
	addr.sin_port = htons(config.getPort());
	if (bind(listen_fd, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
		std::cerr << "bind() failed" << std::endl;
		close(listen_fd);
		listen_fd = -1;
		return -1;
	}

	// Listen
	if (listen(listen_fd, SOMAXCONN) < 0) {
		std::cerr << "listen() failed" << std::endl;
		close(listen_fd);
		listen_fd = -1;
		return -1;
	}
	return listen_fd;
}









/*std::string Server::buildHttpResponse(const std::string &method, const std::string &path)
{
	std::string			body;
	HttpStatusCode::e	status_code;
	if (method != "GET" && method != "POST" && method != "DELETE") {
		body = "<!doctype html><html><head><meta charset=\"utf-8\"><title>Method Not Allowed</title></head><body><h1>405 Method Not Allowed</h1><p>The method specified in the Request-Line is not allowed for the resource identified by the Request-URI.</p></body></html>";
		status_code = HttpStatusCode::MethodNotAllowed;
	} else if (method == "GET" && path == "/") {
		body = "<!doctype html><html><head><meta charset=\"utf-8\"><title>webserv</title></head><body><h1>It works!</h1><p>Welcome to webserv.</p></body></html>";
		status_code = HttpStatusCode::OK;
	} else {
		body = "<!doctype html><html><head><meta charset=\"utf-8\"><title>Not Found</title></head><body><h1>404 Not Found</h1><p>The requested resource was not found on this server.</p></body></html>";
		status_code = HttpStatusCode::NotFound;
	}
	std::ostringstream oss;
	oss << "HTTP/1.1 " << statusCodeToInt(status_code) << " " << getStatusMessage(status_code) << "\r\n";
	oss << "Content-Type: text/html; charset=UTF-8\r\n";
	oss << "Content-Length: " << body.size() << "\r\n";
	oss << "Connection: close\r\n\r\n";
	oss << body;
	return oss.str();
}*/

std::string	readFile(const std::string& path)
{
	std::ifstream file(path.c_str());
	if (!file.is_open()) {
		return "";
	}
	std::stringstream buffer;
	buffer << file.rdbuf();
	return buffer.str();
}

std::string	Server::buildHttpResponse(const std::string &method, const std::string &path)
{
	std::string			body;
	HttpStatusCode::e	status_code = HttpStatusCode::OK;

	if (method != "GET" && method != "POST" && method != "DELETE")
		status_code = HttpStatusCode::MethodNotAllowed;
	else if (method == "GET") {
		std::string		file_path = "." + (path == "/" ? "/index.html" : path);
		std::ifstream	file(file_path.c_str());

		if (file.good()) {
			body = readFile(file_path);
			status_code = HttpStatusCode::OK;
		}
		else
			status_code = HttpStatusCode::NotFound;
	}
	else if (method == "POST" || method == "DELETE")
		status_code = HttpStatusCode::NotImplemented;
	else
		status_code = HttpStatusCode::InternalServerError;

	if (status_code != HttpStatusCode::OK && body.empty()) {
		std::ostringstream	error_body;
		error_body	<< "<!doctype html><html><head><title>" << statusCodeToInt(status_code)
					<< " " << getStatusMessage(status_code) << "</title></head><body><h1>"
					<< statusCodeToInt(status_code) << " " << getStatusMessage(status_code)
					<< "</h1></body></html>";
		body = error_body.str();
	}

	std::ostringstream	oss;
	oss << "HTTP/1.1 " << statusCodeToInt(status_code) << " " << getStatusMessage(status_code) << "\r\n";
	oss << "Content-Type: text/html; charset=UTF-8\r\n";
	oss << "Content-Length: " << body.size() << "\r\n";
	oss << "Connection: close\r\n\r\n";
	oss << body;
	return oss.str();
}


std::string	Server::buildHttpResponse(const std::string &method, const std::string &path, const ServerConfig* cfg)
{
	std::string			body;
	HttpStatusCode::e	status_code = HttpStatusCode::OK;

	if (method != "GET" && method != "POST" && method != "DELETE")
		status_code = HttpStatusCode::MethodNotAllowed;
	else if (method == "GET") {
		const std::string root = (cfg && !cfg->getRoot().empty()) ? cfg->getRoot() : std::string(".");
		std::string file_path;

		if (path == "/") {
			bool served = false;
			if (cfg) {
				std::vector<std::string> idx = cfg->getIndex();
				for (size_t i = 0; i < idx.size(); ++i) {
					std::string candidate = root + "/" + idx[i];
					std::ifstream f(candidate.c_str());
					if (f.good()) {
						body = readFile(candidate);
						status_code = HttpStatusCode::OK;
						served = true;
						break;
					}
				}
			}
			if (!served) {
				file_path = root + "/index.html";
				std::ifstream f(file_path.c_str());
				if (f.good()) {
					body = readFile(file_path);
					status_code = HttpStatusCode::OK;
				}
				else
					status_code = HttpStatusCode::NotFound;
			}
		}
		else {
			file_path = root + path;
			std::ifstream f(file_path.c_str());
			if (f.good()) {
				body = readFile(file_path);
				status_code = HttpStatusCode::OK;
			}
			else
				status_code = HttpStatusCode::NotFound;
		}
	}
	else if (method == "POST" || method == "DELETE")
		status_code = HttpStatusCode::NotImplemented;
	else
		status_code = HttpStatusCode::InternalServerError;

	if (status_code != HttpStatusCode::OK && body.empty()) {
		std::ostringstream error_body;
		error_body << "<!doctype html><html><head><title>" << statusCodeToInt(status_code)
				  << " " << getStatusMessage(status_code) << "</title></head><body><h1>"
				  << statusCodeToInt(status_code) << " " << getStatusMessage(status_code)
				  << "</h1></body></html>";
		body = error_body.str();
	}

	std::ostringstream oss;
	oss << "HTTP/1.1 " << statusCodeToInt(status_code) << " " << getStatusMessage(status_code) << "\r\n";
	oss << "Content-Type: text/html; charset=UTF-8\r\n";
	oss << "Content-Length: " << body.size() << "\r\n";
	oss << "Connection: close\r\n\r\n";
	oss << body;
	return oss.str();
}

#include "../inc/Server.hpp"
#include <sstream>
#include <fstream>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/stat.h>

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

std::string	Server::readFile(const std::string& path)
{
	std::ifstream file(path.c_str());
	if (!file.is_open()) {
		return "";
	}
	std::stringstream buffer;
	buffer << file.rdbuf();
	return buffer.str();
}

bool	Server::checkLocationPaths(const std::vector<Location> &locations)
{
	std::set<std::string>	paths;
	for (size_t i = 0; i < locations.size(); i++) {
		const std::string &path = locations[i].getPath();
		if (paths.find(path) != paths.end())
			return true;
		paths.insert(path);
	}
	return false;
}

std::string	Server::getMimeType(const std::string &path)
{
	if (path.size() > 4 && path.substr(path.size() - 4) == ".css")
		return "text/css";
	if (path.size() > 4 && path.substr(path.size() - 4) == ".png")
		return "image/png";
	if (path.size() > 4 && path.substr(path.size() - 4) == ".jpg")
		return "image/jpeg";
	return "text/html";
}

/*std::string	Server::buildHttpResponse(const std::string &method, const std::string &path)
{
	std::string			body;
	HttpStatusCode::e	status_code;
	std::string			content_type = "text/html";

	if (this->config.getHost().empty())
		status_code = HttpStatusCode::BadRequest;
	else if (method != "GET" && method != "POST" && method != "DELETE")
		status_code = HttpStatusCode::MethodNotAllowed;
	else if (method == "GET" && !checkLocationPaths(this->config.getLocations())) {
		std::string		file_path = "." + (path == "/" ? "/index.html" : path);
		std::ifstream	file(file_path.c_str());

		if (file.good()) {
			body = readFile(file_path);
			status_code = HttpStatusCode::OK;
			content_type = getMimeType(file_path);
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
	oss << "Content-Type: " << content_type << "; charset=UTF-8\r\n";
	oss << "Content-Length: " << body.size() << "\r\n";
	oss << "Connection: close\r\n\r\n";
	oss << body;
	return oss.str();
}*/

std::string	Server::buildHttpResponse(const std::string &method, const std::string &path, const ServerConfig* cfg)
{
	std::string			body;
	HttpStatusCode::e	status_code = HttpStatusCode::OK;
	bool				add_allow = false;
	std::string			content_type = "text/html";

	if (cfg->getHost().empty())
		status_code = HttpStatusCode::BadRequest;
	else if (method != "GET" && method != "POST" && method != "DELETE") {
		status_code = HttpStatusCode::MethodNotAllowed;
		add_allow = true;
	}
	else if (method == "GET") {
		const std::string root = (cfg && !cfg->getRoot().empty()) ? cfg->getRoot() : std::string(".");
		std::string req_path = path.empty() ? "/" : path;

		// Basic traversal guard
		if (req_path.find("..") != std::string::npos) {
			status_code = HttpStatusCode::Forbidden;
		} else {
			if (req_path.empty() || req_path[0] != '/') req_path.insert(req_path.begin(), '/');
			std::string fs_path = root + req_path;

			struct stat st;
			if (stat(fs_path.c_str(), &st) == 0) {
				if (S_ISDIR(st.st_mode)) {
					bool served = false;
					if (cfg) {
						std::vector<std::string> idx = cfg->getIndex();
						for (size_t i = 0; i < idx.size(); ++i) {
							std::string candidate = fs_path;
							if (!candidate.empty() && candidate[candidate.size()-1] != '/')
								candidate += "/";
							candidate += idx[i];
							std::ifstream f(candidate.c_str());
							if (f.good()) {
								body = readFile(candidate);
								status_code = HttpStatusCode::OK;
								content_type = getMimeType(candidate);
								served = true;
								break;
							}
						}
					}
					if (!served) {
						status_code = HttpStatusCode::Forbidden;
					}
				} else {
					std::ifstream f(fs_path.c_str());
					if (f.good()) {
						body = readFile(fs_path);
						status_code = HttpStatusCode::OK;
						content_type = getMimeType(fs_path);
					} else {
						status_code = HttpStatusCode::NotFound;
					}
				}
			} else {
				status_code = HttpStatusCode::NotFound;
			}
		}
	}
	else if (method == "POST" || method == "DELETE") {
		status_code = HttpStatusCode::NotImplemented;
	}
	else {
		status_code = HttpStatusCode::InternalServerError;
	}

	if (status_code != HttpStatusCode::OK) {
		const std::map<int, std::string>			err_pages = cfg->getErrorPages();
		std::map<int, std::string>::const_iterator	it = err_pages.find(statusCodeToInt(status_code));
		if (it != err_pages.end()) {
			std::string	error_page_path = cfg->getRoot() + it->second;
			std::string	error_body_content = readFile(error_page_path);
			if (!error_body_content.empty()) {
				body = readFile(error_page_path);
				content_type = getMimeType(error_page_path);
			}
		}
		if (body.empty()) {
			std::ostringstream error_body;
			error_body	<< "<!doctype html><html><head><title>" << statusCodeToInt(status_code)
						<< " " << getStatusMessage(status_code) << "</title></head><body><h1>"
						<< statusCodeToInt(status_code) << " " << getStatusMessage(status_code)
						<< "</h1></body></html>";
			body = error_body.str();
		}
	}

	std::ostringstream oss;
	oss << "HTTP/1.1 " << statusCodeToInt(status_code) << " " << getStatusMessage(status_code) << "\r\n";
	oss << "Content-Type: " << content_type << "; charset=UTF-8\r\n";
	oss << "Content-Length: " << body.size() << "\r\n";
	if (add_allow) {
		oss << "Allow: GET, POST, DELETE\r\n";
	}
	oss << "Connection: close\r\n\r\n";
	oss << body;
	return oss.str();
}

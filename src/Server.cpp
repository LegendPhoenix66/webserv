#include "../inc/Server.hpp"
#include <sstream>
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
	if (setupListenSocket() >= 0) {
		runEventLoop();
	}
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

void Server::runEventLoop() {
	addListenSocketToPoll();

	while (true) {
		int ret = poll(&poll_fds[0], poll_fds.size(), -1);
		if (ret < 0) {
			std::cerr << "poll() failed" << std::endl;
			break;
		}
		for (size_t i = 0; i < poll_fds.size(); ++i) {
			if (poll_fds[i].revents & POLLIN) {
				if (poll_fds[i].fd == listen_fd) {
					handleListenEvent();
				} else {
					handleClientReadable(i);
				}
			} else if (poll_fds[i].revents & POLLOUT) {
				handleClientWritable(i);
			} else if (poll_fds[i].revents & (POLLERR | POLLHUP | POLLNVAL)) {
				handleClientErrorOrHangup(i);
			}
		}
	}
	close(listen_fd);
}

void Server::addListenSocketToPoll() {
	pollfd listen_pollfd;
	listen_pollfd.fd = listen_fd;
	listen_pollfd.events = POLLIN;
	poll_fds.push_back(listen_pollfd);
}

void Server::handleListenEvent() {
	// Accept new client
	int client_fd = accept(listen_fd, NULL, NULL);
	if (client_fd >= 0) {
		addClient(client_fd);
	}
}

void Server::addClient(int client_fd) {
	fcntl(client_fd, F_SETFL, O_NONBLOCK);
	pollfd client_pollfd;
	client_pollfd.fd = client_fd;
	client_pollfd.events = POLLIN;
	poll_fds.push_back(client_pollfd);
	client_states[client_fd] = ClientState();
}

void Server::removeClientAtIndex(size_t &i) {
	close(poll_fds[i].fd);
	client_states.erase(poll_fds[i].fd);
	poll_fds.erase(poll_fds.begin() + i);
	if (i > 0) {
		--i; // adjust index since current element was erased
	}
}

void Server::handleClientReadable(size_t &i) {
	// Read from client
	char buffer[1024];
	int n = recv(poll_fds[i].fd, buffer, sizeof(buffer), 0);
	if (n > 0) {
		ClientState &st = client_states[poll_fds[i].fd];
		st.in.append(buffer, n);
		// Check for end of headers
		if (st.in.find("\r\n\r\n") != std::string::npos) {
			size_t line_end = st.in.find("\r\n");
			std::string req_line = (line_end != std::string::npos) ? st.in.substr(0, line_end) : st.in;
			std::istringstream iss(req_line);
			std::string method, path, version;
			iss >> method >> path >> version;

			st.out = buildHttpResponse(method, path);
			st.sent = 0;
			poll_fds[i].events = POLLOUT;
		}
	} else {
		// Client disconnected or error
		removeClientAtIndex(i);
	}
}

void Server::handleClientWritable(size_t &i) {
	// Send response
	ClientState &st = client_states[poll_fds[i].fd];
	const std::string &out = st.out;
	if (st.sent < out.size()) {
		int n = send(poll_fds[i].fd, out.c_str() + st.sent, out.size() - st.sent, 0);
		if (n > 0) {
			st.sent += static_cast<size_t>(n);
		} else {
			// Error while sending, close connection
			removeClientAtIndex(i);
			return;
		}
	}
	if (st.sent >= st.out.size()) {
		// Response fully sent: close connection
		removeClientAtIndex(i);
	}
}

void Server::handleClientErrorOrHangup(size_t &i) {
	// Error or hangup
	removeClientAtIndex(i);
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

#include "../inc/Server.hpp"
#include <sstream>

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
	// Create socket
	listen_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (listen_fd < 0) {
		std::cerr << "socket() failed" << std::endl;
		return;
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
	addr.sin_addr.s_addr = INADDR_ANY; //config.getHost();
	addr.sin_port = htons(config.getPort());
	if (bind(listen_fd, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
		std::cerr << "bind() failed" << std::endl;
		return;
	}

	// Listen
	if (listen(listen_fd, SOMAXCONN) < 0) {
		std::cerr << "listen() failed" << std::endl;
		return;
	}

	runEventLoop();
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

std::string	Server::buildHttpResponse(const std::string &method, const std::string &path)
{
	std::string			body;
	HttpStatusCode::e	status_code;
	bool				add_allow = false;
	std::string			content_type = "text/html";

	if (this->config.getHost().empty())
		status_code = HttpStatusCode::BadRequest;
	else if (method != "GET" && method != "POST" && method != "DELETE") {
		status_code = HttpStatusCode::MethodNotAllowed;
		add_allow = true;
	}
	else if (method == "GET") {
		const std::string	root = (!this->config.getRoot().empty()) ? this->config.getRoot() : std::string(".");
		std::string			req_path = path.empty() ? "/" : path;
		if (req_path[0] != '/')
			req_path.insert(req_path.begin(), '/');

		if (req_path.find("..") != std::string::npos)
			status_code = HttpStatusCode::Forbidden;
		else {
			struct stat st;
			std::string	file_path = root + req_path;
			if (stat(file_path.c_str(), &st) == 0) {
				if (S_ISDIR(st.st_mode)) {
					std::vector <std::string> index = this->config.getIndex();
					for (size_t i = 0; i < index.size(); i++) {
						std::string candidate = file_path;
						if (candidate[candidate.size() - 1] != '/')
							candidate += "/";
						candidate += index[i];
						std::ifstream file(candidate.c_str());
						if (file.good()) {
							body = readFile(candidate);
							status_code = HttpStatusCode::OK;
							content_type = getMimeType(candidate);
							break;
						}
					}
				}
				else {
					std::ifstream	file(file_path.c_str());
					if (file.good()) {
						body = readFile(file_path);
						status_code = HttpStatusCode::OK;
						content_type = getMimeType(file_path);
					}
					else
						status_code = HttpStatusCode::NotFound;
				}
			}
			else
				status_code = HttpStatusCode::NotFound;
		}
	}
	else if (method == "POST") {
		size_t max_size = this->config.getClientMaxBodySize();
		if (max_size > 0)
			status_code = HttpStatusCode::ContentTooLarge;
		else
			status_code = HttpStatusCode::NotImplemented;
	}
	else if (method == "DELETE") {
		const std::string	root = (!this->config.getRoot().empty()) ? this->config.getRoot() : std::string(".");
		std::string			req_path = path;
		if (req_path.find("..") != std::string::npos)
			status_code = HttpStatusCode::Forbidden;
		else {
			if (req_path.empty() || req_path[0] != '/')
				req_path.insert(req_path.begin(), '/');
			std::string	file_path = root + req_path;

			std::ifstream	file(file_path.c_str());
			if (!file.good())
				status_code = HttpStatusCode::NotFound;
			else {
				file.close();
				if (std::remove(file_path.c_str()) == 0)
					status_code = HttpStatusCode::NoContent;
				else
					status_code = HttpStatusCode::Forbidden;
			}
		}
	}
	else
		status_code = HttpStatusCode::InternalServerError;

	if (status_code != HttpStatusCode::OK) {
		const std::map<int, std::string>			err_pages = this->config.getErrorPages();
		std::map<int, std::string>::const_iterator	it = err_pages.find(statusCodeToInt(status_code));
		if (it != err_pages.end()) {
			std::string	error_page_path = this->config.getRoot() + it->second;
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
	if (add_allow)
		oss << "Allow: GET, POST, DELETE\r\n";
	oss << "Connection: close\r\n\r\n";
	oss << body;
	return oss.str();
}

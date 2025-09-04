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
    pollfd listen_pollfd;
    listen_pollfd.fd = listen_fd;
    listen_pollfd.events = POLLIN;
    poll_fds.push_back(listen_pollfd);

    while (true) {
        int ret = poll(&poll_fds[0], poll_fds.size(), -1);
        if (ret < 0) {
            std::cerr << "poll() failed" << std::endl;
            break;
        }
        for (size_t i = 0; i < poll_fds.size(); ++i) {
            if (poll_fds[i].revents & POLLIN) {
                if (poll_fds[i].fd == listen_fd) {
                    // Accept new client
                    int client_fd = accept(listen_fd, NULL, NULL);
                    if (client_fd >= 0) {
                        fcntl(client_fd, F_SETFL, O_NONBLOCK);
                        pollfd client_pollfd;
                        client_pollfd.fd = client_fd;
                        client_pollfd.events = POLLIN;
                        poll_fds.push_back(client_pollfd);
                        client_states[client_fd] = ClientState();
                    }
                } else {
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

                            // respond
                            std::string body;
                            std::string status_line;
                            if (method == "GET" && path == "/") {
                                body = "<!doctype html><html><head><meta charset=\"utf-8\"><title>webserv</title></head><body><h1>It works!</h1><p>Welcome to webserv.</p></body></html>";
                                status_line = "200 OK";
                            } else {
                                body = "<!doctype html><html><head><meta charset=\"utf-8\"><title>Not Found</title></head><body><h1>404 Not Found</h1><p>The requested resource was not found on this server.</p></body></html>";
                                status_line = "404 Not Found";
                            }
                            std::ostringstream oss;
                            oss << "HTTP/1.1 " << status_line << "\r\n";
                            oss << "Content-Type: text/html; charset=UTF-8\r\n";
                            oss << "Content-Length: " << body.size() << "\r\n";
                            oss << "Connection: close\r\n\r\n";
                            oss << body;
                            st.out = oss.str();
                            st.sent = 0;
                            poll_fds[i].events = POLLOUT;
                        }
                    } else {
                        // Client disconnected or error
                        close(poll_fds[i].fd);
                        client_states.erase(poll_fds[i].fd);
                        poll_fds.erase(poll_fds.begin() + i);
                        --i;
                    }
                }
            } else if (poll_fds[i].revents & POLLOUT) {
                // Send response
                ClientState &st = client_states[poll_fds[i].fd];
                const std::string &out = st.out;
                if (st.sent < out.size()) {
                    int n = send(poll_fds[i].fd, out.c_str() + st.sent, out.size() - st.sent, 0);
                    if (n > 0) {
                        st.sent += static_cast<size_t>(n);
                    } else {
                        // Error while sending, close connection
                        close(poll_fds[i].fd);
                        client_states.erase(poll_fds[i].fd);
                        poll_fds.erase(poll_fds.begin() + i);
                        --i;
                        continue;
                    }
                }
                if (st.sent >= st.out.size()) {
                    // Response fully sent: close connection
                    close(poll_fds[i].fd);
                    client_states.erase(poll_fds[i].fd);
                    poll_fds.erase(poll_fds.begin() + i);
                    --i;
                }
            } else if (poll_fds[i].revents & (POLLERR | POLLHUP | POLLNVAL)) {
                // Error or hangup
                close(poll_fds[i].fd);
                client_states.erase(poll_fds[i].fd);
                poll_fds.erase(poll_fds.begin() + i);
                --i;
            }
        }
    }
    close(listen_fd);
}

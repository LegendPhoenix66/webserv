#include "../inc/Server.hpp"

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
    sockaddr_in addr = {};
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

    if (poll(&listen_pollfd, 1, -1) < 0) {
        std::cerr << "poll() failed" << std::endl;
        return;
    }

    if (listen_pollfd.revents & POLLIN) {
        int client_fd = accept(listen_fd, NULL, NULL);
        if (client_fd >= 0) {
            fcntl(client_fd, F_SETFL, O_NONBLOCK);
            pollfd client_pollfd;
            client_pollfd.fd = client_fd;
            client_pollfd.events = POLLIN;

            if (poll(&client_pollfd, 1, -1) < 0) {
                std::cerr << "poll() failed" << std::endl;
                close(client_fd);
                return;
            }

            if (client_pollfd.revents & POLLIN) {
                char buffer[1024];
                int n = recv(client_fd, buffer, sizeof(buffer), 0);
                if (n > 0) {
                    std::cout << "Received: " << std::string(buffer, n) << std::endl;
                }
            }
            close(client_fd);
        }
    }
    close(listen_fd);
}

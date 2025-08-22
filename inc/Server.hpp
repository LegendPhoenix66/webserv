#ifndef WEBSERV_SERVER_HPP
#define WEBSERV_SERVER_HPP

#include "ServerConfig.hpp"
#include <sys/types.h>
#include <sys/socket.h>
#include <poll.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <cstring>
#include <unistd.h>
#include <iostream>

/**
 * @class Server
 * @brief Manages a single web server instance, including socket setup and request handling.
 *
 * The Server class is responsible for initializing the listening socket, managing server configuration,
 * and starting the server loop. It provides constructors for configuration and copy semantics, and
 * encapsulates the server's runtime behavior.
 */
class Server {
private:
    /** @brief File descriptor for the listening socket. */
    int listen_fd;
    /** @brief Configuration options for this server instance. */
    ServerConfig config;

    /**
     * @brief Swaps the contents of this Server with another.
     * @param other The Server to swap with.
     */
    void swap(Server &other);

    /**
     * @brief Runs the main event loop for handling incoming connections and requests.
     *
     * This method monitors sockets for activity, accepts new connections,
     * and processes client requests according to the server configuration.
     */
    void runEventLoop();

public:
    /** @brief Default constructor. Initializes an empty server. */
    Server();

    /** @brief Copy constructor. Creates a copy of another Server. */
    Server(const Server &other);

    /**
     * @brief Copy assignment operator using copy-and-swap idiom.
     * @param other The Server to assign from.
     * @return Reference to this Server.
     */
    Server &operator=(Server other);

    /** @brief Destructor. Cleans up resources. */
    ~Server();

    /**
     * @brief Constructs a Server with the given configuration.
     * @param config The configuration to use for this server instance.
     */
    explicit Server(const ServerConfig &config);

    /**
     * @brief Starts the server: sets up the socket and begins accepting connections.
     */
    void start();
};

#endif //WEBSERV_SERVER_HPP

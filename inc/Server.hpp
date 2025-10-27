#ifndef SERVER_HPP
#define SERVER_HPP

#include <string>
#include "Config.hpp"
#include "Listener.hpp"

class Server {
public:
    explicit Server(const Config &cfg);
    ~Server() throw();

    // Start listening on the first server from config. No event loop; just opens the socket.
    bool startOnce(std::string *err);

    // Accessors for event loop wiring
    int listenerFd() const { return _listener.fd(); }
    bool isListening() const { return _listener.isListening(); }
    std::string boundAddress() const { return _listener.boundAddress(); }

private:
    Config _cfg;
    Listener _listener;
};

#endif // SERVER_HPP

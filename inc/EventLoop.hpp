#ifndef WEBSERV_EVENT_LOOP_HPP
#define WEBSERV_EVENT_LOOP_HPP

#include <vector>
#include <map>
#include <string>
#include <poll.h>
#include "Request.hpp"

class ServerConfig; // forward decl

class EventLoop {
public:
    struct Listener {
        int fd;
        const ServerConfig* config;
        Listener() : fd(-1), config(0) {}
        Listener(int f, const ServerConfig* c) : fd(f), config(c) {}
    };

    EventLoop();
    ~EventLoop();

    void addListener(int fd, const ServerConfig* cfg);
    void run();

private:
    struct ClientState {
        Request req;
        std::string out;
        size_t sent;
        ClientState() : req(), out(), sent(0) {}
    };

    std::vector<pollfd> _pfds;
    std::map<int, Listener> _listeners;  // fd -> listener
    std::map<int, ClientState> _clients; // fd -> client state

    void acceptIfReady(size_t idx);
    void onClientReadable(size_t idx);
    void onClientWritable(size_t idx);
    void onClientErrorOrHangup(size_t idx);

    void addClient(int cfd);
    void removeAtIndex(size_t &idx);
};

#endif // WEBSERV_EVENT_LOOP_HPP

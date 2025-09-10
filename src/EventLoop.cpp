#include "../inc/EventLoop.hpp"
#include "../inc/Server.hpp" // for Server::buildHttpResponse signature
#include "../inc/Response.hpp"
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <cstring>
#include <iostream>

EventLoop::EventLoop() : _pfds(), _listeners(), _clients() {}
EventLoop::~EventLoop() {}

void EventLoop::addListener(int fd, const ServerConfig* cfg) {
    if (fd < 0) return;
    pollfd p; p.fd = fd; p.events = POLLIN; p.revents = 0;
    _pfds.push_back(p);
    _listeners[fd] = Listener(fd, cfg);
}

void EventLoop::run() {
    if (_pfds.empty()) {
        std::cerr << "No listeners registered. Nothing to do." << std::endl;
        return;
    }
    const int TIMEOUT_MS = 10000; // basic finite timeout
    while (true) {
        int ret = poll(&_pfds[0], _pfds.size(), TIMEOUT_MS);
        if (ret < 0) {
            std::cerr << "poll() failed" << std::endl;
            break;
        }
        if (ret == 0) {
            // timeout tick; could implement timeouts here
            continue;
        }
        for (size_t i = 0; i < _pfds.size(); ++i) {
            const short re = _pfds[i].revents;
            if (!re) continue;
            if (re & POLLIN) {
                if (_listeners.count(_pfds[i].fd)) acceptIfReady(i);
                else onClientReadable(i);
            } else if (re & POLLOUT) {
                onClientWritable(i);
            } else if (re & (POLLERR | POLLHUP | POLLNVAL)) {
                onClientErrorOrHangup(i);
            }
        }
    }
}

void EventLoop::acceptIfReady(size_t idx) {
    int lfd = _pfds[idx].fd;
    int cfd = accept(lfd, NULL, NULL);
    if (cfd >= 0) {
        const ServerConfig* cfg = _listeners[lfd].config;
        addClient(cfd, cfg);
    }
}

void EventLoop::addClient(int cfd, const ServerConfig* cfg) {
    fcntl(cfd, F_SETFL, O_NONBLOCK);
    pollfd p; p.fd = cfd; p.events = POLLIN; p.revents = 0;
    _pfds.push_back(p);
    _clients[cfd] = ClientState();
    _clients[cfd].server = cfg;
}

void EventLoop::removeAtIndex(size_t &idx) {
    int fd = _pfds[idx].fd;
    close(fd);
    _clients.erase(fd);
    _listeners.erase(fd);
    _pfds.erase(_pfds.begin() + idx);
    if (idx > 0) --idx;
}

void EventLoop::onClientReadable(size_t idx) {
    char buf[2048];
    int n = recv(_pfds[idx].fd, buf, sizeof(buf), 0);
    if (n > 0) {
        ClientState &st = _clients[_pfds[idx].fd];
        Request::State state = st.req.feed(std::string(buf, n));
        if (state == Request::Error) {
            st.out = Response::buildErrorHtml(400, std::string("Bad Request"));
            st.sent = 0;
            _pfds[idx].events = POLLOUT;
        } else if (state == Request::Ready) {
            st.out = Server::buildHttpResponse(st.req.method(), st.req.target(), st.server);
            st.sent = 0;
            _pfds[idx].events = POLLOUT;
        }
    } else {
        removeAtIndex(idx);
    }
}

void EventLoop::onClientWritable(size_t idx) {
    ClientState &st = _clients[_pfds[idx].fd];
    const std::string &out = st.out;
    if (st.sent < out.size()) {
        int n = send(_pfds[idx].fd, out.c_str() + st.sent, out.size() - st.sent, 0);
        if (n > 0) st.sent += (size_t)n;
        else { removeAtIndex(idx); return; }
    }
    if (st.sent >= st.out.size()) {
        removeAtIndex(idx);
    }
}

void EventLoop::onClientErrorOrHangup(size_t idx) {
    removeAtIndex(idx);
}

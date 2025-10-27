#include "../../inc/EventLoop.hpp"
#include "../../inc/Connection.hpp"
#include "../../inc/SignalHandler.hpp"
#include "../../inc/Logger.hpp"

#include <iostream>
#include <cerrno>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/time.h>

static uint64_t now_ms() {
    struct timeval tv; gettimeofday(&tv, 0);
    return (uint64_t)tv.tv_sec * 1000ULL + (uint64_t)(tv.tv_usec / 1000ULL);
}

EventLoop::EventLoop() : _sigFd(-1), _running(false), _shuttingDown(false) {}
EventLoop::~EventLoop() {
    // Cleanup any remaining connections
    for (std::map<int, Connection*>::iterator it = _conns.begin(); it != _conns.end(); ++it) {
        delete it->second;
    }
    _conns.clear();
    // Best-effort close of any stray aux fds
    for (std::map<int, Connection*>::iterator ait = _auxConns.begin(); ait != _auxConns.end(); ++ait) {
        ::close(ait->first);
    }
    _auxConns.clear();
}

bool EventLoop::addListen(int fd,
                   const std::string &bindKey,
                   const std::vector<const ServerConfig*> &group,
                   std::string *err) {
    if (fd < 0) {
        if (err) *err = "addListen: invalid fd";
        return false;
    }
    if (_listenKeys.find(fd) != _listenKeys.end()) {
        if (err) *err = "addListen: fd already registered";
        return false;
    }
    struct pollfd p; p.fd = fd; p.events = POLLIN; p.revents = 0;
    _pfds.push_back(p);
    _listenKeys[fd] = bindKey;
    _listenGroups[fd] = group; // copy of pointers vector (cheap)

    // Register self-pipe if installed (only once)
    if (_sigFd == -1) {
        int sfd = SignalHandler::readFd();
        if (sfd != -1) {
            _sigFd = sfd;
            struct pollfd sp; sp.fd = _sigFd; sp.events = POLLIN; sp.revents = 0; _pfds.push_back(sp);
        }
    }
    return true;
}

void EventLoop::stop() { _running = false; }

static bool set_nonblocking(int fd) {
    int flags = ::fcntl(fd, F_GETFL, 0);
    if (flags == -1) return false;
    if (::fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) return false;
    return true;
}

void EventLoop::addClient(int cfd, int listenFd) {
    // Fetch group and bind key for this listener
    std::map<int, std::vector<const ServerConfig*> >::iterator git = _listenGroups.find(listenFd);
    if (git == _listenGroups.end()) {
        ::close(cfd);
        return;
    }
    const std::vector<const ServerConfig*> &group = git->second;
    const std::string &bkey = _listenKeys[listenFd];

    Connection *c = new Connection(cfd, group, bkey, this);
    struct pollfd p; p.fd = cfd; p.events = POLLIN; p.revents = 0;
    _pfds.push_back(p);
    _conns[cfd] = c;
    LOG_INFOF("accept fd=%d on %s (clients=%zu)", cfd, bkey.c_str(), _conns.size());
}

void EventLoop::removeClient(int cfd) {
    for (size_t i = 0; i < _pfds.size(); ++i) {
        if (_pfds[i].fd == cfd) { _pfds.erase(_pfds.begin() + i); break; }
    }
    std::map<int, Connection*>::iterator it = _conns.find(cfd);
    if (it != _conns.end()) {
        Connection* victim = it->second;
        // Unregister any aux fds owned by this connection
        for (std::map<int, Connection*>::iterator ait = _auxConns.begin(); ait != _auxConns.end();) {
            if (ait->second == victim) {
                int afd = ait->first;
                for (size_t j = 0; j < _pfds.size(); ++j) {
                    if (_pfds[j].fd == afd) { _pfds.erase(_pfds.begin() + j); break; }
                }
                _auxConns.erase(ait++);
                continue;
            }
            ++ait;
        }
        delete victim;
        _conns.erase(it);
    }
}

void EventLoop::disableAllListensInPoll() {
    for (size_t i = 0; i < _pfds.size();) {
        int fd = _pfds[i].fd;
        if (_listenKeys.find(fd) != _listenKeys.end()) {
            _pfds.erase(_pfds.begin() + i);
            continue;
        }
        ++i;
    }
}

void EventLoop::handleSignalReadable(short revents) {
    if (!(revents & POLLIN)) return;
    SignalHandler::drain();
    if (!_shuttingDown) {
        _shuttingDown = true;
        LOG_INFOF("shutdown signal received — stopping accept and draining %zu connections", _conns.size());
        disableAllListensInPoll();
    }
}

void EventLoop::handleListenReadable(int lfd, short revents) {
    if (revents & (POLLERR | POLLHUP | POLLNVAL)) {
        LOG_ERRORF("eventloop: listen fd=%d error/hup, stopping loop", lfd);
        stop();
        return;
    }
    if (!(revents & POLLIN)) return;

    for (;;) {
        int cfd = ::accept(lfd, 0, 0);
        if (cfd == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            if (errno == EINTR) continue;
            if (errno == EMFILE || errno == ENFILE) {
                LOG_WARNF("accept: %s, backing off...", std::strerror(errno));
                ::usleep(20000);
                break;
            }
            LOG_ERRORF("accept: %s", std::strerror(errno));
            break;
        }
        (void)set_nonblocking(cfd);
        addClient(cfd, lfd);
    }
}

bool EventLoop::registerAuxFd(int fd, Connection* owner, short events) {
    if (fd < 0 || !owner) return false;
    if (_auxConns.find(fd) != _auxConns.end()) return false;
    struct pollfd p; p.fd = fd; p.events = events; p.revents = 0;
    _pfds.push_back(p);
    _auxConns[fd] = owner;
    return true;
}

void EventLoop::updateAuxFd(int fd, short events) {
    for (size_t i = 0; i < _pfds.size(); ++i) {
        if (_pfds[i].fd == fd) { _pfds[i].events = events; break; }
    }
}

void EventLoop::unregisterAuxFd(int fd) {
    for (size_t i = 0; i < _pfds.size(); ++i) {
        if (_pfds[i].fd == fd) { _pfds.erase(_pfds.begin() + i); break; }
    }
    _auxConns.erase(fd);
}

void EventLoop::sweepTimeouts(uint64_t now) {
    // Iterate over a copy of keys to allow erasure during iteration
    std::vector<int> keys; keys.reserve(_conns.size());
    for (std::map<int, Connection*>::iterator it = _conns.begin(); it != _conns.end(); ++it) keys.push_back(it->first);
    for (size_t i = 0; i < keys.size(); ++i) {
        int fd = keys[i];
        std::map<int, Connection*>::iterator it = _conns.find(fd);
        if (it == _conns.end()) continue;
        Connection *c = it->second;
        if (!c->checkTimeouts(now)) {
            // Connection requested close due to timeout drain; remove it
            removeClient(fd);
        }
    }
}

int EventLoop::run() {
    // Expect at least one listener and a poll set containing it/them and optional sig fd
    if (_listenKeys.empty() || _pfds.empty()) {
        LOG_ERRORF("eventloop: nothing to run (no listen fds)");
        return 2;
    }

    _running = true;
    while (_running) {
        int rc = ::poll(&_pfds[0], static_cast<nfds_t>(_pfds.size()), 1000); // 1s tick
        if (rc == -1) {
            if (errno == EINTR) continue; // interrupted by signal, retry
            LOG_ERRORF("eventloop: poll: %s", std::strerror(errno));
            return 2;
        }
        uint64_t now = now_ms();
        sweepTimeouts(now);

        // Refresh poll interests for all connections (important after timeouts enqueue responses)
        for (std::map<int, Connection*>::iterator it = _conns.begin(); it != _conns.end(); ++it) {
            int cfd = it->first;
            Connection *c = it->second;
            short events = 0;
            if (c->wantRead()) events |= POLLIN;
            if (c->wantWrite()) events |= POLLOUT;
            for (size_t j = 0; j < _pfds.size(); ++j) {
                if (_pfds[j].fd == cfd) { _pfds[j].events = events; break; }
            }
        }

        if (_shuttingDown && _conns.empty()) {
            LOG_INFOF("shutdown complete — exiting event loop");
            break;
        }
        if (rc == 0) {
            continue; // idle tick
        }

        // Take a snapshot of ready fds to avoid iterator/reference invalidation
        std::vector< std::pair<int, short> > ready;
        ready.reserve(_pfds.size());
        for (size_t i = 0; i < _pfds.size(); ++i) {
            if (_pfds[i].revents) ready.push_back(std::make_pair(_pfds[i].fd, _pfds[i].revents));
        }

        for (size_t i = 0; i < ready.size(); ++i) {
            int fd = ready[i].first;
            short re = ready[i].second;

            if (_sigFd != -1 && fd == _sigFd) {
                handleSignalReadable(re);
                continue;
            }
            // Listener?
            if (_listenKeys.find(fd) != _listenKeys.end()) {
                if (!_shuttingDown) handleListenReadable(fd, re);
                continue;
            }

            // Auxiliary (CGI) fds?
            std::map<int, Connection*>::iterator ait = _auxConns.find(fd);
            if (ait != _auxConns.end()) {
                Connection *c = ait->second;
                if (c) {
                    bool keep = c->onAuxEvent(fd, re);
                    if (!keep || c->isClosed()) {
                        unregisterAuxFd(fd);
                        if (c->isClosed()) {
                            removeClient(c->fd());
                        }
                    }
                } else {
                    unregisterAuxFd(fd);
                }
                continue;
            }

            std::map<int, Connection*>::iterator it = _conns.find(fd);
            if (it == _conns.end()) continue;
            Connection *c = it->second;
            bool keep = true;
            if (re & (POLLERR | POLLHUP | POLLNVAL)) {
                keep = false;
            } else {
                if (re & POLLIN) keep = c->onReadable();
                if (keep && (re & POLLOUT)) keep = c->onWritable();
            }

            if (!keep || c->isClosed()) {
                removeClient(fd);
            } else {
                short events = 0;
                if (c->wantRead()) events |= POLLIN;
                if (c->wantWrite()) events |= POLLOUT;
                // Update current pfds entry for this fd (linear search; small n)
                for (size_t j = 0; j < _pfds.size(); ++j) {
                    if (_pfds[j].fd == fd) { _pfds[j].events = events; break; }
                }
            }
        }
    }
    return 0;
}

#include "../../inc/Socket.hpp"
#include "../../inc/Address.hpp"

#include <cerrno>
#include <cstring>
#include <string>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>

Socket::Socket() : _fd(-1) {}

Socket::~Socket() throw() {
    close();
}

bool Socket::openIPv4(std::string *err) {
    if (_fd != -1) close();
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) {
        if (err) *err = std::string("socket: ") + std::strerror(errno);
        return false;
    }
    // Set CLOEXEC (best-effort)
    int flags = ::fcntl(fd, F_GETFD, 0);
    if (flags != -1) {
        ::fcntl(fd, F_SETFD, flags | FD_CLOEXEC);
    }
    _fd = fd;
    return true;
}

bool Socket::setReuseAddr(std::string *err) {
    if (_fd == -1) {
        if (err) *err = "setReuseAddr: socket not open";
        return false;
    }
    int enable = 1;
    if (::setsockopt(_fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) == -1) {
        if (err) *err = std::string("setsockopt(SO_REUSEADDR): ") + std::strerror(errno);
        return false;
    }
    return true;
}

bool Socket::setNonBlocking(std::string *err) {
    if (_fd == -1) {
        if (err) *err = "setNonBlocking: socket not open";
        return false;
    }
    int flags = ::fcntl(_fd, F_GETFL, 0);
    if (flags == -1) {
        if (err) *err = std::string("fcntl(F_GETFL): ") + std::strerror(errno);
        return false;
    }
    if (::fcntl(_fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        if (err) *err = std::string("fcntl(F_SETFL,O_NONBLOCK): ") + std::strerror(errno);
        return false;
    }
    return true;
}

bool Socket::bind(const Address &addr, std::string *err) {
    if (_fd == -1) {
        if (err) *err = "bind: socket not open";
        return false;
    }
    if (::bind(_fd, addr.data(), addr.len()) == -1) {
        if (err) *err = std::string("bind: ") + std::strerror(errno);
        return false;
    }
    return true;
}

bool Socket::listen(int backlog, std::string *err) {
    if (_fd == -1) {
        if (err) *err = "listen: socket not open";
        return false;
    }
    if (::listen(_fd, backlog) == -1) {
        if (err) *err = std::string("listen: ") + std::strerror(errno);
        return false;
    }
    return true;
}

int Socket::fd() const {
    return _fd;
}

void Socket::close() throw() {
    if (_fd != -1) {
        ::close(_fd);
        _fd = -1;
    }
}

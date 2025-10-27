#ifndef SOCKET_HPP
#define SOCKET_HPP

#include <string>
#include <sys/types.h>

class Address;

class Socket {
public:
    Socket();
    ~Socket() throw();

    bool openIPv4(std::string *err);
    bool setReuseAddr(std::string *err);
    bool setNonBlocking(std::string *err);
    bool bind(const Address &addr, std::string *err);
    bool listen(int backlog, std::string *err);

    int fd() const;
    void close() throw();

private:
    int _fd;
};

#endif // SOCKET_HPP

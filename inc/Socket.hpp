#ifndef WEBSERV_SOCKET_HPP
#define WEBSERV_SOCKET_HPP

#include <string>
#include <sys/types.h>
#include <cerrno>
#include <cstring>
#include <string>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "../inc/Address.hpp"

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

#endif

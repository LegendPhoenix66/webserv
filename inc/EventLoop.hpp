#ifndef EVENTLOOP_HPP
#define EVENTLOOP_HPP

#include <vector>
#include <string>
#include <map>
#include <poll.h>
#include <stdint.h>
#include <iostream>
#include <cerrno>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>

#include "Connection.hpp"
#include "SignalHandler.hpp"
#include "Logger.hpp"
#include "ServerConfig.hpp"
#include "LoopUtils.hpp"

class Connection;

// Single-threaded poll() loop with graceful shutdown and timeouts
class EventLoop {
public:
	EventLoop();
	~EventLoop();

	// Register a listening socket fd for a bind key and its vhost group.
	// Returns false on invalid fd or duplicate registration.
	bool addListen(int fd,
				   const std::string &bindKey,
				   const std::vector<const ServerConfig*> &group,
				   std::string *err);

	// Run until stop() is called or a fatal error occurs.
	// Returns 0 on clean stop, non-zero on unrecoverable error.
	int run();

	// Request the loop to stop.
	void stop();

	// Auxiliary file descriptors (e.g., CGI pipes)
	bool registerAuxFd(int fd, Connection* owner, short events);
	void updateAuxFd(int fd, short events);
	void unregisterAuxFd(int fd);

private:
	int _sigFd;             // self-pipe read end
	bool _running;
	bool _shuttingDown;
	std::vector<struct pollfd> _pfds;
	std::map<int, Connection*> _conns; // client fd -> connection

	// Multi-listener support
	std::map<int, std::string> _listenKeys; // listen fd -> bindKey
	std::map<int, std::vector<const ServerConfig*> > _listenGroups; // listen fd -> vhost group

	// Auxiliary fds mapped to owning connections
	std::map<int, Connection*> _auxConns;

	void handleListenReadable(int lfd, short revents);
	void handleSignalReadable(short revents);
	void addClient(int cfd, int listenFd);
	void removeClient(int cfd);
	void disableAllListensInPoll();
	void sweepTimeouts(uint64_t now_ms);
};

#endif

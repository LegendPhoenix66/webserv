#ifndef LISTENER_HPP
#define LISTENER_HPP

#include <iostream>
#include <string>
#include "ServerConfig.hpp"
#include "Address.hpp"
#include "Socket.hpp"

class Listener {
public:
	Listener();
	~Listener() throw();

	bool start(const ServerConfig &cfg, std::string *err);
	void stop() throw();

	bool isListening() const;
	std::string boundAddress() const;
	int fd() const;

private:
	Socket _sock;
	Address _addr;
	bool _listening;
};

#endif

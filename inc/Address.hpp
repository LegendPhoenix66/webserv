#ifndef ADDRESS_HPP
#define ADDRESS_HPP

#include <string>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstring>
#include <sstream>

// IPv4 socket address value type (v0 scope: numeric IPv4 only)
class Address {
public:
	Address();

	// Build from host + port. v0 accepts only numeric IPv4 (e.g., "127.0.0.1", "0.0.0.0").
	// On error, returns an invalid Address (valid()==false) and optionally fills err.
	static Address fromHostPort(const uint32_t host, const uint16_t port);

	bool valid() const;

	const struct sockaddr *data() const;
	socklen_t len() const;

	std::string toString() const;

private:
	struct sockaddr_in	_sa;
	socklen_t			_len;
};

#endif

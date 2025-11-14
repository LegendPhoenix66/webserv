#include "../inc/Address.hpp"

Address::Address() : _len(0) {
	std::memset(&_sa, 0, sizeof(_sa));
}

/*static bool is_numeric_ipv4(const std::string &s) {
	// crude check: digits and dots only, at least 7 chars like 0.0.0.0
	if (s.empty()) return false;
	for (size_t i = 0; i < s.size(); ++i) {
		char c = s[i];
		if (!((c >= '0' && c <= '9') || c == '.')) return false;
	}
	return true;
}*/

Address Address::fromHostPort(const uint32_t host, const uint16_t port) {
	Address out;
	out._sa.sin_family = AF_INET;
	out._sa.sin_port = htons(port);
	out._sa.sin_addr.s_addr = htonl(host);
	out._len = sizeof(out._sa);
	return out;
}

bool Address::valid() const {
	return _len == sizeof(_sa) && _sa.sin_family == AF_INET;
}

const struct sockaddr *Address::data() const {
	return reinterpret_cast<const struct sockaddr*>(&_sa);
}

socklen_t Address::len() const {
	return _len;
}

std::string Address::toString() const {
	if (!valid()) return "<invalid>";
	char buf[INET_ADDRSTRLEN];
	const char *res = ::inet_ntop(AF_INET, (void*)&_sa.sin_addr, buf, sizeof(buf));
	std::ostringstream oss;
	oss << (res ? res : "?") << ":" << ntohs(_sa.sin_port);
	return oss.str();
}

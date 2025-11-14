#include "../inc/Listener.hpp"

Listener::Listener() : _listening(false) {}

Listener::~Listener() throw() {
	stop();
}

bool Listener::start(const ServerConfig &cfg, std::string *err) {
	std::string emsg;
	_addr = Address::fromHostPort(cfg.getHost(), cfg.getPort());
	if (!_addr.valid()) {
		if (err) *err = std::string("address error: ") + emsg;
		return false;
	}
	if (!_sock.openIPv4(&emsg)) { if (err) *err = emsg; return false; }
	if (!_sock.setReuseAddr(&emsg)) { if (err) *err = emsg; return false; }
	if (!_sock.setNonBlocking(&emsg)) { if (err) *err = emsg; return false; }
	if (!_sock.bind(_addr, &emsg)) { if (err) *err = emsg; return false; }
	if (!_sock.listen(128, &emsg)) { if (err) *err = emsg; return false; }
	_listening = true;
	std::cout << "listen: " << _addr.toString() << " [non-blocking]\n";
	return true;
}

void Listener::stop() throw() {
	if (_listening) {
		_sock.close();
		_listening = false;
	} else {
		// still close if fd is open
		_sock.close();
	}
}

bool		Listener::isListening() const { return _listening; }
std::string	Listener::boundAddress() const { return _addr.toString(); }
int			Listener::fd() const { return _sock.fd(); }

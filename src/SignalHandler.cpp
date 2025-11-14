#include "../inc/SignalHandler.hpp"

#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <errno.h>

int		SignalHandler::s_pipe[2] = { -1, -1 };
bool	SignalHandler::s_installed = false;

SignalHandler::SignalHandler() {
	install();
}

SignalHandler::~SignalHandler() {
	uninstall();
}

static void set_nonblock(int fd) {
	if (fd < 0) return;
	int flags = fcntl(fd, F_GETFL, 0);
	if (flags >= 0) fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

bool SignalHandler::install() {
	if (s_installed) return true;

	if (pipe(s_pipe) != 0) {
		s_pipe[0] = s_pipe[1] = -1;
		return false;
	}
	set_nonblock(s_pipe[0]);
	set_nonblock(s_pipe[1]);

	struct sigaction sa;
	std::memset(&sa, 0, sizeof(sa));
	sa.sa_handler = SignalHandler::onSignal;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	sigaction(SIGINT, &sa, 0);
	sigaction(SIGTERM, &sa, 0);
	s_installed = true;
	return true;
}

void SignalHandler::uninstall() {
	if (!s_installed) return;

	if (s_pipe[0] != -1) {
		close(s_pipe[0]);
		s_pipe[0] = -1;
	}
	if (s_pipe[1] != -1) {
		close(s_pipe[1]);
		s_pipe[1] = -1;
	}

	struct sigaction sa;
	std::memset(&sa, 0, sizeof(sa));
	sa.sa_handler = SIG_DFL;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	sigaction(SIGINT, &sa, 0);
	sigaction(SIGTERM, &sa, 0);

	s_installed = false;
}

int SignalHandler::readFd() {
	return s_pipe[0];
}

void SignalHandler::drain() {
	if (s_pipe[0] == -1) return;
	char buf[64];
	for (;;) {
		ssize_t n = read(s_pipe[0], buf, sizeof(buf));
		if (n <= 0) break;
	}
}

void SignalHandler::onSignal(int signo) {
	(void)signo;
	if (s_pipe[1] != -1) {
		char b = 1;
		// async-signal-safe write
		(void)write(s_pipe[1], &b, 1);
	}
}

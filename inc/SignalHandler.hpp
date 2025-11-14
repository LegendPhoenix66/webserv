#ifndef SIGNALHANDLER_HPP
#define SIGNALHANDLER_HPP

#include <signal.h>

class SignalHandler {
public:
	SignalHandler();
	~SignalHandler();

	static bool install();
	static void uninstall();
	static int readFd();
	static void drain();

private:
	static void onSignal(int signo);

	static int	s_pipe[2];
	static bool	s_installed;
};

#endif

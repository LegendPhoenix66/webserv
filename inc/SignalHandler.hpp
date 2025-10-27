#ifndef SIGNAL_HANDLER_HPP
#define SIGNAL_HANDLER_HPP

#include <signal.h>

class SignalHandler {
public:
    static bool install();
    static void uninstall();
    static int readFd();
    static void drain();

private:
    static void onSignal(int signo);
};

#endif // SIGNAL_HANDLER_HPP

#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <stdarg.h>
#include <stdio.h>

// Simple logger with stderr + optional file sinks. C++98‑friendly.

enum LogLevel { LOG_DEBUG = 0, LOG_INFO = 1, LOG_WARN = 2, LOG_ERROR = 3 };

class Logger {
public:
    static bool init(const char *accessLogPath, const char *errorLogPath);
    static void shutdown();
    static void setLevel(LogLevel lvl);

    // Error/system log (stderr + error file if set)
    static void logf(LogLevel lvl, const char *fmt, ...);

    // Access log (file sink if set; falls back to stderr)
    static void accessf(const char *fmt, ...);

private:
    static void vlogf(LogLevel lvl, const char *fmt, va_list ap);
    static void vaccessf(const char *fmt, va_list ap);
};

// Convenience macros (compile‑time no‑ops when LOG_LEVEL is high).
#ifndef LOG_LEVEL
#define LOG_LEVEL LOG_INFO
#endif

#define LOG_DEBUGF(...) do { if (LOG_LEVEL <= LOG_DEBUG) Logger::logf(LOG_DEBUG, __VA_ARGS__); } while (0)
#define LOG_INFOF(...)  do { if (LOG_LEVEL <= LOG_INFO)  Logger::logf(LOG_INFO,  __VA_ARGS__); } while (0)
#define LOG_WARNF(...)  do { if (LOG_LEVEL <= LOG_WARN)  Logger::logf(LOG_WARN,  __VA_ARGS__); } while (0)
#define LOG_ERRORF(...) do { if (LOG_LEVEL <= LOG_ERROR) Logger::logf(LOG_ERROR, __VA_ARGS__); } while (0)

#endif // LOGGER_HPP

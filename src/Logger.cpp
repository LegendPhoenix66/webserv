#include "../inc/Logger.hpp"

// Static state
static FILE *s_err = 0;
static FILE *s_access = 0;
static LogLevel s_level = LOG_INFO;

static void now_timestamp(char *buf, size_t buflen) {
	// Format: YYYY-MM-DD HH:MM:SS.mmm (local time)
	struct timespec ts;
#if defined(CLOCK_REALTIME)
	if (clock_gettime(CLOCK_REALTIME, &ts) != 0) {
        ts.tv_sec = time(NULL);
        ts.tv_nsec = 0;
    }
#else
	ts.tv_sec = time(NULL);
	ts.tv_nsec = 0;
#endif
	struct tm tmv;
#if defined(_GNU_SOURCE) || !defined(_WIN32)
	localtime_r(&ts.tv_sec, &tmv);
#else
	tmv = *localtime(&ts.tv_sec);
#endif
	long msec = (long)(ts.tv_nsec / 1000000L);
	snprintf(buf, buflen, "%04d-%02d-%02d %02d:%02d:%02d.%03ld",
			 tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday,
			 tmv.tm_hour, tmv.tm_min, tmv.tm_sec, msec);
}

static const char *level_str(LogLevel lvl) {
	switch (lvl) {
		case LOG_DEBUG: return "DEBUG";
		case LOG_INFO:  return "INFO";
		case LOG_WARN:  return "WARN";
		case LOG_ERROR: return "ERROR";
	}
	return "?";
}

bool Logger::init(const char *accessLogPath, const char *errorLogPath) {
	// Ensure logs directory exists (best-effort)
	mkdir("logs", 0755);

	s_err = stderr; // always available
	if (errorLogPath && *errorLogPath) {
		FILE *f = fopen(errorLogPath, "a");
		if (f) s_err = f; // route errors to file if open succeeds
	}
	if (accessLogPath && *accessLogPath) {
		s_access = fopen(accessLogPath, "a");
		// if open fails, fallback will be stderr
	}
	return true;
}

void Logger::shutdown() {
	if (s_err && s_err != stderr) {
		fclose(s_err);
	}
	if (s_access) fclose(s_access);
	s_err = stderr;
	s_access = 0;
}

void Logger::setLevel(LogLevel lvl) { s_level = lvl; }

void Logger::logf(LogLevel lvl, const char *fmt, ...) {
	if (lvl < s_level) return;
	va_list ap; va_start(ap, fmt);
	vlogf(lvl, fmt, ap);
	va_end(ap);
}

void Logger::accessf(const char *fmt, ...) {
	va_list ap; va_start(ap, fmt);
	vaccessf(fmt, ap);
	va_end(ap);
}

void Logger::vlogf(LogLevel lvl, const char *fmt, va_list ap) {
	if (!s_err) s_err = stderr;
	char ts[32]; now_timestamp(ts, sizeof ts);
	fprintf(s_err, "[%s] %-5s ", ts, level_str(lvl));
	vfprintf(s_err, fmt, ap);
	fputc('\n', s_err);
	fflush(s_err);
}

void Logger::vaccessf(const char *fmt, va_list ap) {
	FILE *out = s_access ? s_access : s_err ? s_err : stderr;
	char ts[32]; now_timestamp(ts, sizeof ts);
	// Access logs begin with timestamp too for readability
	fprintf(out, "[%s] ", ts);
	vfprintf(out, fmt, ap);
	fputc('\n', out);
	fflush(out);
}

#include "../inc/Logger.hpp"

// Static state
static std::ostream		*s_err;
static std::ofstream	s_err_ofs;
static std::ostream		*s_access = 0;
static std::ofstream	s_access_ofs;
static LogLevel			s_level = LOG_INFO;

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

	s_err = &std::cerr; // always available
	if (errorLogPath && *errorLogPath) {
		s_err_ofs.open(errorLogPath, std::ios::app);
		if (s_err_ofs) s_err = &s_err_ofs; // route errors to file if open succeeds
	}
	s_access = 0;
	if (accessLogPath && *accessLogPath) {
		s_access_ofs.open(accessLogPath, std::ios::app);
		if (s_access_ofs) s_access = &s_access_ofs;
		// if open fails, fallback will be stderr
	}
	return true;
}

void Logger::shutdown() {
	if (s_err_ofs.is_open()) s_err_ofs.close();
	if (s_access_ofs.is_open()) s_access_ofs.close();
	s_err = &std::cerr;
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
	if (!s_err) s_err = &std::cerr;

	char ts[32];
	now_timestamp(ts, sizeof ts);

	int	bufsize = 1024;
	std::vector<char>	buf(bufsize);
	while (true) {
		va_list	ap_copy;
		std::memcpy(ap_copy, ap, sizeof(va_list));
		int	n = vsnprintf(&buf[0], bufsize, fmt, ap_copy);
		va_end(ap_copy);
		if (n >= 0 && n < bufsize) break;
		if (n > 0) bufsize = n + 1;
		else bufsize *= 2;
		buf.resize(bufsize);
	}

	std::ostringstream	oss;
	oss << "[" << ts << "] " << std::left << std::setw(5) << level_str(lvl) << " " << &buf[0];

	(*s_err) << oss.str() << std::endl;
	s_err->flush();
}

void Logger::vaccessf(const char *fmt, va_list ap) {
	std::ostream *out = s_access ? s_access : (s_err ? s_err : &std::cerr);

	char ts[32];
	now_timestamp(ts, sizeof ts);

	int bufsize = 1024;
	std::vector<char> buf(bufsize);
	while (true) {
		va_list ap_copy;
		std::memcpy(ap_copy, ap, sizeof(va_list));
		int n = vsnprintf(&buf[0], bufsize, fmt, ap_copy);
		va_end(ap_copy);
		if (n >= 0 && n < bufsize) break;
		if (n > 0) bufsize = n + 1;
		else bufsize *= 2;
		buf.resize(bufsize);
	}

	std::ostringstream oss;
	oss << "[" << ts << "] " << &buf[0];

	(*out) << oss.str() << std::endl;
	out->flush();
}

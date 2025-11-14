#include "../inc/LoopUtils.hpp"

unsigned long long	now_ms() {
	struct timeval tv;
	gettimeofday(&tv, 0);
	return (unsigned long long) tv.tv_sec * 1000ULL + (unsigned long long)(tv.tv_usec / 1000ULL);
}

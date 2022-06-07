#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/resource.h>
#include <time.h>

#define SECOND 1000000000ul

static struct timespec time_diff(
		struct timespec cur,
		struct timespec old);

void log_error(const char *const fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	fprintf(stderr, "[ERROR]: ");
	vfprintf(stderr, fmt, args);
	va_end(args);
}

void log_warning(const char *const fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	fprintf(stderr, "[WARNING]: ");
	vfprintf(stderr, fmt, args);
	va_end(args);
}

void log_debug(const char *const fmt, ...)
{
#ifndef DEBUG
	return;
#endif
	static struct timespec start_time;
	if (start_time.tv_nsec == 0) {
		fprintf(stderr, "[ real,   cpu,  maxRSS]\n");
		clock_gettime(CLOCK_REALTIME, &start_time);
	}
	struct timespec real_time;
	struct timespec cpu_time;
	clock_gettime(CLOCK_REALTIME, &real_time);
	clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &cpu_time);
	real_time = time_diff(real_time, start_time);

	struct rusage usage;
	getrusage(RUSAGE_SELF, &usage);

	va_list args;
	va_start(args, fmt);
	fprintf(
		stderr,
		"[%ld.%03ld, %ld.%03ld, %ld KB][DEBUG]: ",
		real_time.tv_sec,
		real_time.tv_nsec / 1000000,
		cpu_time.tv_sec,
		cpu_time.tv_nsec / 1000000,
		usage.ru_maxrss
		);
	vfprintf(stderr, fmt, args);
	va_end(args);
}

void log_info(const char *const fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	fprintf(stderr, "[INFO]: ");
	vfprintf(stderr, fmt, args);
	va_end(args);
}

struct timespec time_diff(struct timespec cur,
		struct timespec old)
{
	struct timespec diff;
	diff.tv_sec = cur.tv_sec - old.tv_sec;
	if (cur.tv_nsec > old.tv_nsec) {
		diff.tv_nsec = cur.tv_nsec - old.tv_nsec;
	} else {
		diff.tv_nsec = SECOND + cur.tv_nsec - old.tv_nsec;
		diff.tv_sec -= 1;
	}
	return diff;
}

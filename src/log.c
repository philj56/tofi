#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/resource.h>
#include <time.h>

#define SECOND 1000000000ul

static struct timespec time_diff(
		struct timespec cur,
		struct timespec old);

static int indent = 0;

static void print_indent(FILE *file)
{
	for (int i = 0; i < indent; i++) {
		fprintf(file, "    ");
	}
}

void log_indent(void)
{
	indent++;
}

void log_unindent(void)
{
	if (indent > 0) {
		indent--;
	}
}

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
		fprintf(stderr, "[    real,      cpu,   maxRSS]\n");
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
		"[%ld.%06ld, %ld.%06ld, %5ld KB][DEBUG]: ",
		real_time.tv_sec,
		real_time.tv_nsec / 1000,
		cpu_time.tv_sec,
		cpu_time.tv_nsec / 1000,
		usage.ru_maxrss
		);
	print_indent(stderr);
	vfprintf(stderr, fmt, args);
	va_end(args);
}

void log_info(const char *const fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	fprintf(stderr, "[INFO]: ");
	print_indent(stderr);
	vfprintf(stderr, fmt, args);
	va_end(args);
}

void log_append_error(const char *const fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
}

void log_append_warning(const char *const fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
}

void log_append_debug(const char *const fmt, ...)
{
#ifndef DEBUG
	return;
#endif
	va_list args;
	va_start(args, fmt);
	vprintf(fmt, args);
	va_end(args);
}

void log_append_info(const char *const fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	vprintf(fmt, args);
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

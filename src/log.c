#include <stdio.h>

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
	va_list args;
	va_start(args, fmt);
	printf("[DEBUG]: ");
	vprintf(fmt, args);
	va_end(args);
}

void log_info(const char *const fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	printf("[INFO]: ");
	vprintf(fmt, args);
	va_end(args);
}

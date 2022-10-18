#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static size_t test = 0;
static char *todo = NULL;

void tap_version(size_t version)
{
	printf("TAP version %zu\n", version);
}

void tap_plan()
{
	printf("1..%zu\n", test);
}

void tap_ok(const char *message, ...)
{
	va_list args;
	va_start(args, message);
	printf("ok %zu - ", ++test);
	vprintf(message, args);
	if (todo != NULL) {
		printf(" # TODO %s", todo);
		free(todo);
		todo = NULL;
	}
	printf("\n");
	va_end(args);
}

void tap_not_ok(const char *message, ...)
{
	va_list args;
	va_start(args, message);
	printf("not ok %zu - ", ++test);
	vprintf(message, args);
	if (todo != NULL) {
		printf(" # TODO %s", todo);
		free(todo);
		todo = NULL;
	}
	printf("\n");
	va_end(args);
}

void tap_todo(const char *message)
{
	todo = strdup(message);
}


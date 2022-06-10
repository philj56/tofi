#include <stdio.h>
#include <string.h>
#include "log.h"
#include "xmalloc.h"

void *xmalloc(size_t size)
{
	void *ptr = malloc(size);

	if (ptr != NULL) {
		//log_debug("Allocated %zu bytes.\n", size);
		return ptr;
	} else {
		log_error("Out of memory, exiting.\n");
		exit(EXIT_FAILURE);
	}
}

void *xcalloc(size_t nmemb, size_t size)
{
	void *ptr = calloc(nmemb, size);

	if (ptr != NULL) {
		//log_debug("Allocated %zux%zu bytes.\n", nmemb, size);
		return ptr;
	} else {
		log_error("Out of memory, exiting.\n");
		exit(EXIT_FAILURE);
	}
}

void *xrealloc(void *ptr, size_t size)
{
	ptr = realloc(ptr, size);

	if (ptr != NULL) {
		//log_debug("Reallocated to %zu bytes.\n", size);
		return ptr;
	} else {
		log_error("Out of memory, exiting.\n");
		exit(EXIT_FAILURE);
	}
}

char *xstrdup(const char *s)
{
	char *ptr = strdup(s);

	if (ptr != NULL) {
		return ptr;
	} else {
		log_error("Out of memory, exiting.\n");
		exit(EXIT_FAILURE);
	}
}

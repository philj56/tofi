#ifndef XMALLOC_H
#define XMALLOC_H

#include <stdlib.h>

__attribute__((malloc))
void *xmalloc(size_t size);

__attribute__((malloc))
void *xcalloc(size_t nmemb, size_t size);

void *xrealloc(void *ptr, size_t size);

#endif /* XMALLOC_H */

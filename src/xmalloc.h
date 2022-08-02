#ifndef XMALLOC_H
#define XMALLOC_H

#include <stdlib.h>

[[nodiscard("memory leaked")]]
[[gnu::malloc]]
void *xmalloc(size_t size);

[[nodiscard("memory leaked")]]
[[gnu::malloc]]
void *xcalloc(size_t nmemb, size_t size);

[[nodiscard("memory leaked")]]
void *xrealloc(void *ptr, size_t size);

[[nodiscard("memory leaked")]]
[[gnu::malloc]]
char *xstrdup(const char *s);

#endif /* XMALLOC_H */

#ifndef TAP_H
#define TAP_H

#include <stddef.h>

#define tap_is(a, b, message) ((a) == (b) ? tap_ok((message)) : tap_not_ok((message)))
#define tap_isnt(a, b, message) ((a) != (b) ? tap_ok((message)) : tap_not_ok((message)))

void tap_version(size_t version);
void tap_plan(void);
void tap_ok(const char *message, ...);
void tap_not_ok(const char *message, ...);
void tap_todo(const char *message);

#endif /* TAP_H */

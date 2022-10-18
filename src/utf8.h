#ifndef UTF8_H
#define UTF8_H

#include <glib.h>
#include <stdint.h>

uint32_t utf8_isupper(uint32_t c);
uint32_t utf8_islower(uint32_t c);
uint32_t utf8_isalnum(uint32_t c);
uint32_t utf8_toupper(uint32_t c);
uint32_t utf8_tolower(uint32_t c);

uint32_t utf8_get_char(const char *s);
char *utf8_next_char(const char *s);
char *utf8_prev_char(const char *s);
char *utf8_strchr(const char *s, uint32_t c);
char *utf8_strcasechr(const char *s, uint32_t c);
size_t utf8_strlen(const char *s);
char *utf8_strcasestr(const char * restrict haystack, const char * restrict needle);
char *utf8_normalize(const char *s);

#endif /* UTF8_H */

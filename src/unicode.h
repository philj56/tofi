#ifndef UNICODE_H
#define UNICODE_H

#include <glib.h>
#include <stdbool.h>
#include <stdint.h>

uint8_t utf32_to_utf8(uint32_t c, char *buf);
uint32_t utf8_to_utf32(const char *s);
uint32_t utf8_to_utf32_validate(const char *s);
uint32_t *utf8_string_to_utf32_string(const char *s);

uint32_t utf32_isprint(uint32_t c);
uint32_t utf32_isspace(uint32_t c);
uint32_t utf32_isupper(uint32_t c);
uint32_t utf32_islower(uint32_t c);
uint32_t utf32_isalnum(uint32_t c);
uint32_t utf32_toupper(uint32_t c);
uint32_t utf32_tolower(uint32_t c);
size_t utf32_strlen(const uint32_t *s);

char *utf8_next_char(const char *s);
char *utf8_prev_char(const char *s);
char *utf8_strchr(const char *s, uint32_t c);
char *utf8_strcasechr(const char *s, uint32_t c);
size_t utf8_strlen(const char *s);
char *utf8_strcasestr(const char * restrict haystack, const char * restrict needle);
char *utf8_normalize(const char *s);
char *utf8_compose(const char *s);
bool utf8_validate(const char *s);

#endif /* UNICODE_H */

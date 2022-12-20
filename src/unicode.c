#include <stdbool.h>
#include <string.h>

#include "unicode.h"

uint8_t utf32_to_utf8(uint32_t c, char *buf)
{
	return g_unichar_to_utf8(c, buf);
}

uint32_t utf8_to_utf32(const char *s)
{
	return g_utf8_get_char(s);
}

uint32_t utf8_to_utf32_validate(const char *s)
{
	return g_utf8_get_char_validated(s, -1);
}

uint32_t *utf8_string_to_utf32_string(const char *s)
{
	return g_utf8_to_ucs4_fast(s, -1, NULL);
}

uint32_t utf32_isprint(uint32_t c)
{
	return g_unichar_isprint(c);
}

uint32_t utf32_isspace(uint32_t c)
{
	return g_unichar_isspace(c);
}

uint32_t utf32_isupper(uint32_t c)
{
	return g_unichar_isupper(c);
}

uint32_t utf32_islower(uint32_t c)
{
	return g_unichar_islower(c);
}

uint32_t utf32_isalnum(uint32_t c)
{
	return g_unichar_isalnum(c);
}

uint32_t utf32_toupper(uint32_t c)
{
	return g_unichar_toupper(c);
}

uint32_t utf32_tolower(uint32_t c)
{
	return g_unichar_tolower(c);
}

size_t utf32_strlen(const uint32_t *s)
{
	size_t len = 0;
	while (s[len] != U'\0') {
		len++;
	}
	return len;
}

char *utf8_next_char(const char *s)
{
	return g_utf8_next_char(s);
}

char *utf8_prev_char(const char *s)
{
	return g_utf8_prev_char(s);
}

char *utf8_strchr(const char *s, uint32_t c)
{
	return g_utf8_strchr(s, -1, c);
}

char *utf8_strcasechr(const char *s, uint32_t c)
{
	c = g_unichar_tolower(c);

	const char *p = s;
	while (*p != '\0' && g_unichar_tolower(g_utf8_get_char(p)) != c) {
		p = g_utf8_next_char(p);
	}
	if (*p == '\0') {
		return NULL;
	}
	return (char *)p;
}

size_t utf8_strlen(const char *s)
{
	return g_utf8_strlen(s, -1);
}

char *utf8_strcasestr(const char * restrict haystack, const char * restrict needle)
{
	char *h = g_utf8_casefold(haystack, -1);
	char *n = g_utf8_casefold(needle, -1);

	char *cmp = strstr(h, n);
	char *ret;

	if (cmp == NULL) {
		ret = NULL;
	} else {
		ret = (char *)haystack + (cmp - h);
	}

	free(h);
	free(n);

	return ret;
}

char *utf8_normalize(const char *s)
{
	return g_utf8_normalize(s, -1, G_NORMALIZE_DEFAULT);
}

char *utf8_compose(const char *s)
{
	return g_utf8_normalize(s, -1, G_NORMALIZE_DEFAULT_COMPOSE);
}

bool utf8_validate(const char *s)
{
	return g_utf8_validate(s, -1, NULL);
}

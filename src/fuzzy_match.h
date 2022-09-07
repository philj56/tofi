#ifndef FUZZY_MATCH_H
#define FUZZY_MATCH_H

#include <stdint.h>

int32_t fuzzy_match_simple_words(const char *restrict patterns, const char *restrict str);
int32_t fuzzy_match_words(const char *restrict patterns, const char *restrict str);
int32_t fuzzy_match(const char *restrict pattern, const char *restrict str);

#endif /* FUZZY_MATCH_H */

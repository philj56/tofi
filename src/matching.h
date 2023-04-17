#ifndef MATCHING_H
#define MATCHING_H

#include <stdint.h>

enum matching_algorithm {
	MATCHING_ALGORITHM_NORMAL,
	MATCHING_ALGORITHM_PREFIX,
	MATCHING_ALGORITHM_FUZZY
};

int32_t match_words(enum matching_algorithm algorithm, const char *restrict patterns, const char *restrict str);

#endif /* MATCHING_H */

#ifndef HISTORY_H
#define HISTORY_H

#include <stddef.h>

struct program {
	char *restrict name;
	size_t run_count;
};

struct history {
	size_t count;
	size_t size;
	struct program *buf;
};

[[gnu::nonnull]]
void history_destroy(struct history *restrict vec);

[[gnu::nonnull]]
void history_add(struct history *restrict vec, const char *restrict str);

//[[gnu::nonnull]]
//void history_remove(struct history *restrict vec, const char *restrict str);

[[nodiscard]]
struct history history_load(void);

void history_save(struct history *history);

#endif /* HISTORY_H */

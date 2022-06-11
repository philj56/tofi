#ifndef COMPGEN_H
#define COMPGEN_H

#include "history.h"
#include "string_vec.h"

struct string_vec compgen(void);
struct string_vec compgen_cached(void);
void compgen_history_sort(struct string_vec *programs, struct history *history);

#endif /* COMPGEN_H */

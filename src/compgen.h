#ifndef COMPGEN_H
#define COMPGEN_H

#include "history.h"
#include "string_vec.h"

[[nodiscard("memory leaked")]]
char *compgen(void);

[[nodiscard("memory leaked")]]
char *compgen_cached(void);

[[nodiscard("memory leaked")]]
struct string_ref_vec compgen_history_sort(struct string_ref_vec *programs, struct history *history);

#endif /* COMPGEN_H */

#ifndef DRUN_H
#define DRUN_H

#include "desktop_vec.h"
#include "history.h"
#include "string_vec.h"

struct desktop_vec drun_generate(void);
struct desktop_vec drun_generate_cached(void);
void drun_history_sort(struct desktop_vec *apps, struct history *history);
void drun_print(const char *filename, const char *terminal_command);
void drun_launch(const char *filename);

#endif /* DRUN_H */

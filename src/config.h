#ifndef TOFI_CONFIG_H
#define TOFI_CONFIG_H

#include <stdbool.h>
#include "tofi.h"

void config_load(struct tofi *tofi, const char *filename);
bool config_apply(struct tofi *tofi, const char *option, const char *value);
void config_fixup_values(struct tofi *tofi);

#endif /* TOFI_CONFIG_H */

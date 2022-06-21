#ifndef TOFI_CONFIG_H
#define TOFI_CONFIG_H

#include "tofi.h"

void config_load(struct tofi *tofi, const char *filename);
void apply_option(struct tofi *tofi, const char *option, const char *value);

#endif /* TOFI_CONFIG_H */

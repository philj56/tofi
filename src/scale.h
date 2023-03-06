#ifndef SCALE_H
#define SCALE_H

#include <stdint.h>

uint32_t scale_apply(uint32_t base, uint32_t scale);
uint32_t scale_apply_inverse(uint32_t base, uint32_t scale);

#endif /* SCALE_H */

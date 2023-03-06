#include <math.h>
#include <stdint.h>

/*
 * In order to correctly scale by fractions of 120 (used by
 * wp_fractional_scale_v1), we need to bias the result before rounding.
 */

uint32_t scale_apply(uint32_t base, uint32_t scale)
{
	return round(base * (scale / 120.) + 1e-6);
}

uint32_t scale_apply_inverse(uint32_t base, uint32_t scale)
{
	return round(base * (120. / scale) + 1e-6);
}

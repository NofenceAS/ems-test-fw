#include "nf_settings.h"
#include <ztest.h>

int eep_uint16_read(eep_uint16_enum_t field, uint16_t *value)
{
	ARG_UNUSED(field);
	ARG_UNUSED(value);
	return ztest_get_return_value();
}
int eep_uint16_write(eep_uint16_enum_t field, uint16_t value)
{
	ARG_UNUSED(field);
	ARG_UNUSED(value);
	return ztest_get_return_value();
}
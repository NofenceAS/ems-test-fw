//
// Created by alaa on 04.04.2022.
//

#include "nf_settings.h"
#include <ztest.h>

int eep_uint32_read(eep_uint32_enum_t field, uint32_t *value)
{
	ARG_UNUSED(field);
	ARG_UNUSED(value);
	return ztest_get_return_value();
}
int eep_uint32_write(eep_uint32_enum_t field, uint32_t value)
{
	ARG_UNUSED(field);
	ARG_UNUSED(value);
	return ztest_get_return_value();
}
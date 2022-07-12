/*
 * Copyright (c) 2022 Nofence AS
 */

#include <ztest.h>
#include "nf_settings.h"

int eep_uint16_read(eep_uint16_enum_t field, uint16_t *value)
{
	ARG_UNUSED(field);

    ztest_copy_return_data(value, sizeof(uint16_t));

	return ztest_get_return_value();
}
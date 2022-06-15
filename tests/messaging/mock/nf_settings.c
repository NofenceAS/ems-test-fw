//
// Created by alaa on 04.04.2022.
//

#include "nf_settings.h"
#include <ztest.h>

int eep_uint8_read(eep_uint8_enum_t field, uint8_t *value)
{
	ARG_UNUSED(field);
	ARG_UNUSED(value);
	return ztest_get_return_value();
}
int eep_uint8_write(eep_uint8_enum_t field, uint8_t value)
{
	ARG_UNUSED(field);
	ARG_UNUSED(value);
	return ztest_get_return_value();
}

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

int eep_uint8_read(eep_uint8_enum_t field, uint8_t *value)
{
	ARG_UNUSED(field);
	ARG_UNUSED(value);
	return ztest_get_return_value();
}

int eep_uint8_write(eep_uint8_enum_t field, uint8_t value)
{
	ARG_UNUSED(field);
	ARG_UNUSED(value);
	return ztest_get_return_value();
}

int eep_read_ble_sec_key(uint8_t *ble_sec_key, size_t bufsize)
{
	ARG_UNUSED(ble_sec_key);
	ARG_UNUSED(bufsize);
	return ztest_get_return_value();
}

int eep_write_ble_sec_key(uint8_t *ble_sec_key, size_t bufsize)
{
	ARG_UNUSED(ble_sec_key);
	ARG_UNUSED(bufsize);
	return ztest_get_return_value();
}

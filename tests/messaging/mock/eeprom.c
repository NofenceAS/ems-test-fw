/*
* Copyright (c) 2022 Nofence AS
*/

#include "eeprom.h"
#include <ztest.h>

int eep_read_serial(uint32_t *p_serial)
{
	ARG_UNUSED(p_serial);
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

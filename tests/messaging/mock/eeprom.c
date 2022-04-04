
/*
* Copyright (c) 2022 Nofence AS
*/

#include "nf_eeprom_private.h"
#include <drivers/eeprom.h>
#include <string.h>
#include <ztest.h>

/* EEPROM device pointer */
const struct device *m_p_device;

void eep_init(const struct device *dev)
{
	return;
}

int eep_write_serial(uint32_t serial)
{
	return ztest_get_return_value();
}

int eep_read_serial(uint32_t *p_serial)
{
	return ztest_get_return_value();
}

int eep_write_host_port(const char *host_port)
{
	return ztest_get_return_value();
}

int eep_read_host_port(char *host_port, size_t bufsize)
{
	return ztest_get_return_value();
}

int eep_write_ble_sec_key(uint8_t *ble_sec_key, size_t bufsize)
{
	return ztest_get_return_value();
}

int eep_read_ble_sec_key(uint8_t *ble_sec_key, size_t bufsize)
{
	return ztest_get_return_value();
}
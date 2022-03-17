
/*
* Copyright (c) 2022 Nofence AS
*/

#include "nf_eeprom_private.h"
#include <drivers/eeprom.h>
#include <string.h>

/* EEPROM device pointer */
const struct device *m_p_device;

void eep_init(const struct device *dev)
{
	m_p_device = dev;
}

int eep_write_serial(uint32_t serial)
{
	return eeprom_write(m_p_device, offsetof(struct eemem, uid), &serial,
			    sizeof(serial));
}

int eep_read_serial(uint32_t *p_serial)
{
	return eeprom_read(m_p_device, offsetof(struct eemem, uid), p_serial,
			   sizeof(*p_serial));
}

int eep_write_host_port(const char *host_port)
{
	if (strlen(host_port) > EEP_HOST_PORT_BUF_SIZE - 1) {
		return -EOVERFLOW;
	}
	/* Note, write the string including null-terminator */
	return eeprom_write(m_p_device, offsetof(struct eemem, host_port),
			    host_port, strlen(host_port) + 1);
}

int eep_read_host_port(char *host_port, size_t bufsize)
{
	if (bufsize < EEP_HOST_PORT_BUF_SIZE) {
		return -EOVERFLOW;
	}
	int ret = eeprom_read(m_p_device, offsetof(struct eemem, host_port),
			      host_port, EEP_HOST_PORT_BUF_SIZE);
	if (ret == 0) {
		host_port[EEP_HOST_PORT_BUF_SIZE - 1] = '\0';
	}
	return ret;
}

int eep_write_ble_sec_key(uint8_t *ble_sec_key, size_t bufsize)
{
	if (sizeof(bufsize) > EEP_BLE_SEC_KEY_LEN) {
		return -EOVERFLOW;
	}
	/* Note, write the string including null-terminator */
	return eeprom_write(m_p_device, offsetof(struct eemem, ble_sec_key),
			    ble_sec_key, bufsize);
}

int eep_read_ble_sec_key(uint8_t *ble_sec_key, size_t bufsize)
{
	if (bufsize < EEP_BLE_SEC_KEY_LEN) {
		return -EOVERFLOW;
	}
	int ret = eeprom_read(m_p_device, offsetof(struct eemem, ble_sec_key),
			      ble_sec_key, EEP_BLE_SEC_KEY_LEN);
	return ret;
}
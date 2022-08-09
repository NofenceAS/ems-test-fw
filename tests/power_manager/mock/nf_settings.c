#include <ztest.h>
#include "nf_settings.h"

static uint8_t m_eeprom_val = 0;

int eep_uint8_read(eep_uint8_enum_t field, uint8_t *value)
{
	ARG_UNUSED(field);

	*value = m_eeprom_val;

	return ztest_get_return_value();
}

int eep_uint8_write(eep_uint8_enum_t field, uint8_t value)
{
	ARG_UNUSED(field);

	m_eeprom_val = value;

	return ztest_get_return_value();
}